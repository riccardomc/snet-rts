#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <threading.h>
#include "debug.h"
#include "htab.h"
#include "sysutils.h"

#define LIST_NAME_H IdStack
#define LIST_TYPE_NAME_H idstack
#define LIST_VAL_H int
#include "list-template.h"
#undef LIST_VAL_H
#undef LIST_TYPE_NAME_H
#undef LIST_NAME_H

#define LIST_NAME IdStack
#define LIST_TYPE_NAME idstack
#define LIST_VAL int
#include "list-template.c"
#undef LIST_VAL
#undef LIST_TYPE_NAME
#undef LIST_NAME

#define LIST_NAME_H Host
#define LIST_TYPE_NAME_H host
#define LIST_VAL_H snet_host_t*
#include "list-template.h"
#undef LIST_VAL_H
#undef LIST_TYPE_NAME_H
#undef LIST_NAME_H

#define LIST_NAME Host
#define LIST_TYPE_NAME host
#define LIST_VAL snet_host_t*
#define LIST_FREE_FUNCTION SNetHostFree
#include "list-template.c"
#undef LIST_FREE_FUNCTION
#undef LIST_VAL
#undef LIST_TYPE_NAME
#undef LIST_NAME

#define SNET_ZMQ_SNDHWM_V 1000
#define SNET_ZMQ_RCVHWM_V 1000
#define SNET_ZMQ_DATATO_V 120000 //data time out
#define SNET_ZMQ_SYNCTO_V 120000 //sync time out
#define SNET_ZMQ_LOOKTO_V 120000 //lookup timeout

typedef int (*sendfun)(zmsg_t*, int);

static htab_opts_t opts;
static bool running = true;
static snet_idstack_list_t *idstack = NULL;
static snet_host_list_t *hostlist = NULL;
static snet_host_t *hostid[SNET_ZMQ_HTABLN];
static void *sock_out[SNET_ZMQ_HTABLN];
static sendfun sendfuns[SNET_ZMQ_HTABLN]; //FIXME: unify these data structures?
static pthread_mutex_t htablock = PTHREAD_MUTEX_INITIALIZER; //FIXME; use zmtx?
static void *sockp; // sync reply socket
static void *sockq; // sync query socket
static void *sock_in; // incoming data socket

static char *hostname;
static int sndhwm = SNET_ZMQ_SNDHWM_V;
static int rcvhwm = SNET_ZMQ_RCVHWM_V;
static int datato = SNET_ZMQ_DATATO_V;
static int syncto = SNET_ZMQ_SYNCTO_V;
static int lookto = SNET_ZMQ_LOOKTO_V;

/*
* Id Stack (second level index)
*/

static void HTabSetNextId(int index)
{
  int last;
  if (SNetIdStackListLength(idstack) == 0) {
    SNetIdStackListAppendStart(idstack, index);
  } else {
    /* we want previously removed index on top */
    last = SNetIdStackListPopEnd(idstack);
    SNetIdStackListAppendEnd(idstack, index);
    SNetIdStackListAppendEnd(idstack, last);
  }
}

static void HTabInitId()
{
  idstack = SNetIdStackListCreate(0);
  HTabSetNextId(0);
}

static void HTabDestroyId()
{
  SNetIdStackListDestroy(idstack);
}

static int HTabNextId()
{
  int index = SNetIdStackListPopStart(idstack);
  if (SNetIdStackListLength(idstack) == 0) {
    HTabSetNextId(index + 1);
  }
  return index;
}

/*
* Host Table 
*/

void HTabInit() 
{
  pthread_mutex_lock(&htablock);
  hostlist = SNetHostListCreate(0);
  HTabInitId();
  for (int i = 0 ; i < SNET_ZMQ_HTABLN ; i++) {
    hostid[i] = NULL;
  }
  pthread_mutex_unlock(&htablock);
}

void HTabFree()
{
  SNetHostListDestroy(hostlist);
  HTabDestroyId();
}

int HTabAdd(snet_host_t *h)
{
  static int index;
  pthread_mutex_lock(&htablock);
  SNetHostListAppendEnd(hostlist, h);
  do {
    index = HTabNextId();
  } while (hostid[index] != NULL);
  hostid[index] = h;
  pthread_mutex_unlock(&htablock);
  return index;
}

void HTabSet(snet_host_t *h, int index)
{
  static int stack_index, list_index, last_id;
  stack_index = -1;
  list_index = -1;

  pthread_mutex_lock(&htablock);

  stack_index = SNetIdStackListFind(idstack, index);
  if (stack_index == (SNetIdStackListLength(idstack) - 1)) {
    //it's last element of stack: preserve nextid.
    last_id = SNetIdStackListPopEnd(idstack);
    SNetIdStackListAppendEnd(idstack, last_id + 1);
  } else if (stack_index >= 0) {
      SNetIdStackListRemove(idstack, stack_index);
  } else if (hostid[index] != NULL) {
      //index not in stack, could be occupied: free it.
      list_index = SNetHostListFind(hostlist, hostid[index]);
      if (list_index >= -1) {
        SNetHostFree(SNetHostListRemove(hostlist, list_index));
      }
  }

  SNetHostListAppendEnd(hostlist, h);
  hostid[index] = h;

  pthread_mutex_unlock(&htablock);
}

int HTabRemove(int index)
{
  static int list_index;
  pthread_mutex_lock(&htablock);
  if (hostid[index] != NULL) {
    list_index = SNetHostListFind(hostlist, hostid[index]);
    if (list_index >= 0) {
      SNetHostFree(SNetHostListRemove(hostlist, list_index));
    }
    hostid[index] = NULL;
    HTabSetNextId(index);
  }
  pthread_mutex_unlock(&htablock);
  return list_index;
}

snet_host_t *HTabLookUp(int index)
{
  static snet_host_t *ret;
  pthread_mutex_lock(&htablock);
  ret = hostid[index];
  pthread_mutex_unlock(&htablock);
  return ret;
}

void HTabDump()
{
  pthread_mutex_lock(&htablock);
  printf("\n");
  for (int i = 0 ; i < 5; i++) {
    printf("%d: ", i);
    SNetHostDump(hostid[i]);
    printf("\n");
  }
  printf("-\n");
  pthread_mutex_unlock(&htablock);
}

char *HTabGetHostName() {
  return hostname;
}


/** 
* Threading 
*/


static int HTabFirstSend(zmsg_t *msg, int dest); //FIXME: make compiler happy
static void HTabSyncSend(zframe_t *data, int type, void *socket);
static zmsg_t *HTabSyncRecv(void *socket);

static void HTabPart() {
  zframe_t *data_f = NULL;
  zmsg_t *msg = NULL;

  SNetDistribPack(&data_f, &opts.node_location, sizeof(int));
  HTabSyncSend(data_f, htab_part, sockq);
  msg = HTabSyncRecv(sockq); //we don't care about the payload.
  zmsg_destroy(&msg);
}

static void HTabConnect() {
  if (opts.node_location == 0) {
    zsocket_bind(sockp, "tcp://*:%d/", opts.sport);
    //Add yourself as node 0 and connect to yourself.
    HTabAdd(SNetHostCreate(hostname, "*", opts.dport, opts.sport));
    zsocket_connect(sockq, "tcp://%s:%d/", hostname, opts.sport);

  } else {
    zframe_t *type_f = NULL;
    zframe_t *data_f = NULL;
    zmsg_t *msg = NULL;
    snet_host_t *host;

    zsocket_connect(sockq, opts.raddr);

    SNetDistribPack(&data_f, &opts.node_location, sizeof(opts.node_location)); //request id
    host = SNetHostCreate(hostname, "*", opts.dport, opts.sport);
    SNetHostPack(host, &data_f);
    HTabSyncSend(data_f, htab_host, sockq); //send host

    msg = HTabSyncRecv(sockq); //recv id
    type_f = zmsg_pop(msg);
    data_f = zmsg_pop(msg);
    SNetDistribUnpack(&data_f, &opts.node_location, sizeof(int));
    HTabSet(host, opts.node_location); //add yourself

    zframe_destroy(&type_f);
    zframe_destroy(&data_f);
    zmsg_destroy(&msg);
 }
}

void SNetDistribZMQHTabInit(int dport, int sport, int node_location, char *raddr, char*hname)
{
  opts.ctx = zctx_new();

  if (opts.ctx == NULL) {
    SNetUtilDebugFatal("ZMQDistrib: %s", strerror(errno));
  }

  //grace period before destroying sockets
  zctx_set_linger(opts.ctx, 1000);

  opts.dport = dport;
  opts.sport = sport;
  opts.node_location = node_location;
  strncpy(opts.raddr, raddr, SNET_ZMQ_ADDRLN);
 
  if (strcmp(hname, "") == 0) {
    hostname = SNetUtilSysHostname();
    SNetUtilSysEnvStr("SNET_ZMQ_HOSTNM", hostname, SNET_ZMQ_HOSTLN);
  } else {
    hostname = hname;
  }

  SNetUtilSysEnvInt("SNET_ZMQ_SNDHWM", &sndhwm);
  SNetUtilSysEnvInt("SNET_ZMQ_RCVHWM", &rcvhwm);
  SNetUtilSysEnvInt("SNET_ZMQ_DATATO", &datato);
  SNetUtilSysEnvInt("SNET_ZMQ_SYNCTO", &syncto);
  SNetUtilSysEnvInt("SNET_ZMQ_LOOKTO", &lookto);

  //init sync and data sockets
  sockp = zsocket_new(opts.ctx, ZMQ_REP);
  sockq = zsocket_new(opts.ctx, ZMQ_REQ);
  zsocket_set_rcvtimeo(sockp, syncto);
  zsocket_set_sndtimeo(sockp, syncto);
  zsocket_set_rcvtimeo(sockq, syncto);
  zsocket_set_sndtimeo(sockq, syncto);

  for (int i = 0 ; i < SNET_ZMQ_HTABLN ; i++) {
    sock_out[i] = NULL;
    sendfuns[i] = &HTabFirstSend;
  }

  sock_in = zsocket_new(opts.ctx, ZMQ_PULL);
  if (sock_in == NULL) {
    SNetUtilDebugFatal("ZMQDistrib: %s", strerror(errno));
  }
  zsocket_set_rcvtimeo(sock_in, datato);
  zsocket_set_rcvhwm(sock_in, rcvhwm);
  zsocket_bind(sock_in, "tcp://*:%d", opts.dport);

  HTabInit();

  HTabConnect();

}

void SNetDistribZMQHTabStart(void)
{
  SNetThreadingSpawn( ENTITY_other, -1, SNetNameCreate(NULL, NULL,
      "zmq_htab"), &SNetDistribZMQHTab, NULL);

}

void SNetDistribZMQHTabStop(void)
{
  if (opts.node_location == 0) {
    while (SNetDistribZMQHTabCount() - 1) {
      zclock_sleep(1);
    }
  } else {
    HTabPart();
  }
  pthread_mutex_lock(&htablock);
  running = false;
  pthread_mutex_unlock(&htablock);
}

//Root node loop
static void HTabLoopRoot(void *args) 
{
  zframe_t *type_f = NULL;
  zframe_t *data_f = NULL;
  zframe_t *reply_f = NULL;
  zmsg_t *msg = NULL;
  int id, type_v = -1;
  snet_host_t *host;

  while (running) {
    reply_f = NULL;

    msg = HTabSyncRecv(sockp);
    type_f = zmsg_pop(msg);
    data_f = zmsg_pop(msg);

    SNetDistribUnpack(&type_f, &type_v, sizeof(int));

    switch(type_v) {
      case htab_host:
        SNetDistribUnpack(&data_f, &id, sizeof(int));
        host = SNetHostUnpack(&data_f);
        if (id > 0 && HTabLookUp(id) == NULL) {
          //the host requested an id, and it's free
          HTabSetNextId(id);
        }
        id = HTabAdd(host);
        SNetDistribPack(&reply_f, &id, sizeof(int));
        HTabSyncSend(reply_f, htab_id, sockp);
        break;

      case htab_lookup:
        SNetDistribUnpack(&data_f, &id, sizeof(int));
        host = HTabLookUp(id);
        if (host == NULL) {
          SNetDistribPack(&reply_f, &id, sizeof(int));
          HTabSyncSend(reply_f, htab_fail, sockp);
        } else {
          SNetHostPack(host, &reply_f);
          HTabSyncSend(reply_f, htab_host, sockp);
        }
        break;

      case htab_part:
        SNetDistribUnpack(&data_f, &id, sizeof(int));
        HTabRemove(id);
        SNetDistribPack(&reply_f, &id, sizeof(int));
        HTabSyncSend(reply_f, htab_part, sockp);
        break;

      default:
        //FIXME: decide what to do here
        break;
    }

    zframe_destroy(&type_f);
    zframe_destroy(&data_f);
    zmsg_destroy(&msg);

  }


}

static void HTabLoopNode(void *args) 
{

  while (running) {
    //FIXME: nodes control protocol?
    zclock_sleep(1000);
  }

}


void SNetDistribZMQHTab(void *args)
{

  if (SNetDistribGetNodeId() == 0) {
    HTabLoopRoot(args);
  } else {
    HTabLoopNode(args);
  }

  free(hostname);
  HTabFree();
  pthread_mutex_destroy(&htablock);

}

/*
* Messages Interface
*/

// Callable wrapper function
int HTabSend(zmsg_t *msg, int destination)
{
  return sendfuns[destination](msg, destination);
}

// Regular serialized send
static int HTabRegularSend(zmsg_t *msg, int dest)
{
  static int ret = -1;
  pthread_mutex_lock(&htablock);
  ret = zmsg_send(&msg, sock_out[dest]);
  pthread_mutex_unlock(&htablock);
  return ret;
}

int SNetDistribZMQConnect(int index);

// Connect before sending, then switch to regular sending
static int HTabFirstSend(zmsg_t *msg, int dest)
{
  static int ret = -1;
  ret = SNetDistribZMQConnect(dest);
  pthread_mutex_lock(&htablock);
  sendfuns[dest] = &HTabRegularSend;
  ret = zmsg_send(&msg, sock_out[dest]);
  pthread_mutex_unlock(&htablock);
  return ret;
}

// Receive wrapper function
zmsg_t *HTabRecv()
{
  zmsg_t *msg = zmsg_recv(sock_in);

  if (msg == NULL) {
    SNetUtilDebugFatal("ZMQDistrib DATA recv timeout: %s", strerror(errno));
  }

  return msg;
}

static zmsg_t *HTabSyncRecv(void *socket)
{
  zmsg_t *msg = zmsg_recv(socket);
  if (msg == NULL) {
    SNetUtilDebugFatal("ZMQDistrib SYNC recv timeout: %s", strerror(errno));
  }

  return msg;
}

// Send control htab_msg messages 
static void HTabSyncSend(zframe_t *data, int type, void *socket)
{
  int rc;
  zframe_t *type_f = NULL;
  zmsg_t *msg = zmsg_new();

  zmsg_push(msg, data);

  SNetDistribPack(&type_f, &type, sizeof(int));
  zmsg_push(msg, type_f);

  rc = zmsg_send(&msg, socket);

  if (rc != 0) {
    SNetUtilDebugFatal("ZMQDistrib SYNC send timeout: %s", strerror(errno));
  }

}


/*
* Local Calls
*/

snet_host_t *SNetDistribZMQHTabLookUp(int index)
{
  static snet_host_t *ret;
  ret = HTabLookUp(index);
  if (ret == NULL) {
    ret = HTabRemoteLookUp(index);
  }

  return ret;
}

int SNetDistribZMQConnect(int index)
{
  int ret = -1;
  int retries = lookto >= 0 ? lookto / 100 : -1;
  int delay = 100;
  snet_host_t *host;

  //look up until true or timeout (lookto)
  while (true) {
    host = SNetDistribZMQHTabLookUp(index);
    //if retries == -1 we want it to loop forever.
    if (((host == NULL) && (retries > 0)) || retries++ == -1 ) {
      --retries;
      zclock_sleep(delay);
    } else {
      break;
    }
  }

  if (host != NULL) {

    sock_out[index] = zsocket_new(opts.ctx, ZMQ_PUSH);

    if (!sock_out[index]) {
      SNetUtilDebugFatal("ZMQDistrib Cannot create socket to node: %d", index);
    }

    zsocket_set_sndhwm(sock_out[index], sndhwm);
    ret = zsocket_connect(sock_out[index], "tcp://%s:%d/", host->host, host->data_port);

  } else {
    SNetUtilDebugFatal("ZMQDistrib Cannot connect to node: %d. Lookup failed.", index);
  }

  return ret;
}

int SNetDistribZMQDisconnect(int index)
{
  int ret = -1;
  snet_host_t *host = SNetDistribZMQHTabLookUp(index);

  if (host != NULL) {
    ret = zsocket_disconnect(sock_out[index], "tcp://%s:%d/", host->host, host->data_port);
  }

  sock_out[index] = NULL;

  return ret;

}

void SNetDistribZMQBind(void *socket, int port)
{
  //FIXME: this is just a placeholder!
  int rc = zsocket_bind(socket, "tcp://%s:%d/", "localhost", port);
  if (rc < 0)
    exit(0);
}

int SNetDistribZMQHTabCount()
{
  static int ret = -1;
  pthread_mutex_lock(&htablock);
  ret = SNetHostListLength(hostlist);
  pthread_mutex_unlock(&htablock);
  return ret;
}

int HTabNodeLocation()
{
  return opts.node_location;
}

/*
* Remote calls
*/

snet_host_t *HTabRemoteLookUp(int index)
{
  int type_v;

  zframe_t *data_f = NULL;
  zframe_t *type_f = NULL;
  zmsg_t *reply = NULL;
  snet_host_t *newhost = NULL;

  SNetDistribPack(&data_f, &index, sizeof(int));
  HTabSyncSend(data_f, htab_lookup, sockq);
  reply = HTabSyncRecv(sockq);
  type_f = zmsg_pop(reply);
  SNetDistribUnpack(&type_f, &type_v, sizeof(int));
  if (type_v == htab_fail) {
    //FIXME: lookup failed, what to do?
  } else {
    data_f = zmsg_pop(reply);
    newhost = SNetHostUnpack(&data_f);
    if (hostid[index] == NULL) {
      HTabSet(newhost, index);
    }
    zframe_destroy(&data_f);
  }

  zmsg_destroy(&reply);
  zframe_destroy(&type_f);

  return newhost;
}

