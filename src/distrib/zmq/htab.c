#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <pthread.h>
#include <threading.h>
#include "debug.h"
#include "htab.h"

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
#define LIST_VAL_H htab_host_t*
#include "list-template.h"
#undef LIST_VAL_H
#undef LIST_TYPE_NAME_H
#undef LIST_NAME_H

#define LIST_NAME Host
#define LIST_TYPE_NAME host
#define LIST_VAL htab_host_t*
#define LIST_FREE_FUNCTION HTabHostFree
#include "list-template.c"
#undef LIST_FREE_FUNCTION
#undef LIST_VAL
#undef LIST_TYPE_NAME
#undef LIST_NAME

#define SNET_ZMQ_SNDHWM 5
#define SNET_ZMQ_RCVHWM 5

typedef int (*sendfun)(zmsg_t*, int);

static htab_opts_t opts;
static bool running = true;
static snet_idstack_list_t *idstack = NULL;
static snet_host_list_t *hostlist = NULL;
static htab_host_t *hostid[HTAB_MAX_HOSTS];
static void *sock_out[HTAB_MAX_HOSTS];
static sendfun sendfuns[HTAB_MAX_HOSTS]; //FIXME: unify these data structures?
static pthread_mutex_t htablock = PTHREAD_MUTEX_INITIALIZER; //FIXME; use zmtx?
static void *sockp; // sync reply socket
static void *sockq; // sync query socket
static void *sock_in; // incoming data socket

static char *hostname;

static int recv_hwm = SNET_ZMQ_RCVHWM;
static int send_hwm = SNET_ZMQ_SNDHWM;

/*
* Host Items
*/

htab_host_t *HTabHostAlloc()
{
  int i;
  htab_host_t *host = malloc(sizeof(htab_host_t));
  if (host) {
    host->host = malloc(sizeof(char) * HTAB_HNAME_LEN);
    host->bind = malloc(sizeof(char) * HTAB_HNAME_LEN);
    for (i = 0; i < HTAB_HNAME_LEN; i++) {
      host->host[i] = '\0';
      host->bind[i] = '\0';
    }
    host->data_port = 0;
    host->sync_port = 0;
  }
  return host;
}

htab_host_t *HTabHostCreate(char *host, char *bind,
    int data_port, int sync_port)
{
  htab_host_t *h = HTabHostAlloc();
  strncpy(h->host, host, HTAB_HNAME_LEN);
  strncpy(h->bind, bind, HTAB_HNAME_LEN);
  h->data_port = data_port;
  h->sync_port = sync_port;
  return h;
}

void HTabHostFree(htab_host_t *host)
{
  free(host->host);
  free(host->bind);
  free(host);
  host = NULL;
}

bool HTabHostCompare(htab_host_t *h1, htab_host_t *h2)
{
  return h1->data_port == h2->data_port
    && h1->sync_port == h2->sync_port
    && strcmp(h1->host, h2->host) == 0
    && strcmp(h1->bind, h2->bind) == 0;
}

void HTabHostPack(htab_host_t *host, void *buf)
{
  SNetDistribPack(buf, &host->data_port, sizeof(int));
  SNetDistribPack(buf, &host->sync_port, sizeof(int));
  SNetDistribPack(buf, host->host, HTAB_HNAME_LEN);
  SNetDistribPack(buf, host->bind, HTAB_HNAME_LEN);
}

htab_host_t *HTabHostUnpack(void *buf) 
{
  htab_host_t *host = HTabHostAlloc();
  SNetDistribUnpack(buf, &host->data_port, sizeof(int));
  SNetDistribUnpack(buf, &host->sync_port, sizeof(int));
  SNetDistribUnpack(buf, host->host, HTAB_HNAME_LEN);
  SNetDistribUnpack(buf, host->bind, HTAB_HNAME_LEN);
  return host;
}

void HTabHostDump(htab_host_t *h)
{
  if (h) {
    printf("%d %d %s %s", h->data_port, h->sync_port, h->host, h->bind);
  } else {
    printf(". . . .");
  }
}

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
  for (int i = 0 ; i < HTAB_MAX_HOSTS ; i++) {
    hostid[i] = NULL;
  }
  pthread_mutex_unlock(&htablock);
}

void HTabFree()
{
  SNetHostListDestroy(hostlist);
  HTabDestroyId();
}

int HTabAdd(htab_host_t *h)
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

void HTabSet(htab_host_t *h, int index)
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
        HTabHostFree(SNetHostListRemove(hostlist, list_index));
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
    printf("REM: %d\n", list_index);
    if (list_index >= 0) {
      HTabHostFree(SNetHostListRemove(hostlist, list_index));
    }
    hostid[index] = NULL;
    HTabSetNextId(index);
  }
  pthread_mutex_unlock(&htablock);
  return list_index;
}

htab_host_t *HTabGet(int index)
{
  static htab_host_t *ret;
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
    HTabHostDump(hostid[i]);
    printf("\n");
  }
  printf("-\n");
  pthread_mutex_unlock(&htablock);
}

/*
* Utils
*/

char *HTabGetHostname()
{
  //FIXME: consider using getaddrinfo
  char *hostname = malloc(sizeof(char) * 256);
  struct hostent* host;

  hostname[255] = '\0';
  gethostname(hostname, 255);
  host = gethostbyname(hostname);
  strcpy(hostname, host->h_name);

  return hostname;
}

/** 
* Threading 
*/


static int HTabFirstSend(zmsg_t *msg, int dest); //FIXME: make compiler happy
static void HTabMsg(zframe_t *data, int type, void *socket);

void SNetDistribZMQHTabInit(int dport, int sport, int node_location, char *raddr)
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
  strncpy(opts.raddr, raddr, 300); //FIXME: n variable?
  
  hostname = HTabGetHostname();

  //init sync and data sockets
  sockp = zsocket_new(opts.ctx, ZMQ_REP);
  sockq = zsocket_new(opts.ctx, ZMQ_REQ);

  for (int i = 0 ; i < HTAB_MAX_HOSTS ; i++) {
    sock_out[i] = NULL;
    sendfuns[i] = &HTabFirstSend;
  }

  sock_in = zsocket_new(opts.ctx, ZMQ_PULL);
  if (sock_in == NULL) {
    SNetUtilDebugFatal("ZMQDistrib: %s", strerror(errno));
  }
  zmq_setsockopt(sock_in, ZMQ_RCVHWM, &recv_hwm, sizeof(int));
  zsocket_bind(sock_in, "tcp://*:%d", opts.dport);

  HTabInit();

  if (opts.node_location == 0) {
    zsocket_bind(sockp, "tcp://*:%d/", opts.sport);
    //Add yourself as node 0 and connect to yourself.
    HTabAdd(HTabHostCreate(hostname, "*", opts.dport, opts.sport));
    zsocket_connect(sockq, "tcp://%s:%d/", hostname, opts.sport);

  } else {
    zframe_t *type_f = NULL;
    zframe_t *data_f = NULL;
    zmsg_t *msg = NULL;
    htab_host_t *host;

    zsocket_connect(sockq, opts.raddr);

    host = HTabHostCreate(hostname, "*", opts.dport, opts.sport);
    HTabHostPack(host, &data_f);
    HTabMsg(data_f, htab_host, sockq); //send host
    HTabHostFree(host);

    msg = zmsg_recv(sockq); //recv id
    type_f = zmsg_pop(msg);
    data_f = zmsg_pop(msg);
    SNetDistribUnpack(&data_f, &opts.node_location, sizeof(int));

    //add yourself
    HTabSet(HTabHostCreate(hostname, "*", opts.dport, opts.sport), opts.node_location);

    zframe_destroy(&type_f);
    zframe_destroy(&data_f);
    zmsg_destroy(&msg);
 }

}

void SNetDistribZMQHTabStart(void)
{
  SNetThreadingSpawn( ENTITY_other, -1, SNetNameCreate(NULL, NULL,
      "zmq_htab"), &SNetDistribZMQHTab, NULL);

}

void SNetDistribZMQHTabStop(void)
{
  pthread_mutex_lock(&htablock);
  running = false;
  free(hostname);
  HTabFree();
  pthread_mutex_unlock(&htablock);
}

//Root node loop
static void HTabLoopRoot(void *args) 
{
  zframe_t *type_f = NULL;
  zframe_t *data_f = NULL;
  zframe_t *reply_f = NULL;
  zmsg_t *msg = NULL;
  int id, type_v = -1, newid;
  htab_host_t *host;

  while (running) {
    reply_f = NULL;

    msg = zmsg_recv(sockp);
    type_f = zmsg_pop(msg);
    data_f = zmsg_pop(msg);

    SNetDistribUnpack(&type_f, &type_v, sizeof(int));

    switch(type_v) {
      case htab_host:
        host = HTabHostUnpack(&data_f);
        newid = HTabAdd(host);
        SNetDistribPack(&reply_f, &newid, sizeof(int));
        HTabMsg(reply_f, htab_id, sockp);
        break;

      case htab_lookup:
        SNetDistribUnpack(&data_f, &id, sizeof(int));
        host = HTabGet(id);
        if (host == NULL) {
          SNetDistribPack(&reply_f, &id, sizeof(int));
          HTabMsg(reply_f, htab_fail, sockp);
        } else {
          HTabHostPack(host, &reply_f);
          HTabMsg(reply_f, htab_host, sockp);
        }
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
    //FIXME: what a node should do?
    sleep(1);
  }
}


void SNetDistribZMQHTab(void *args)
{

  if (SNetDistribGetNodeId() == 0) {
    HTabLoopRoot(args);
  } else {
    HTabLoopNode(args);
  }

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
  return zmsg_recv(sock_in);
}

// Send control htab_msg messages 
static void HTabMsg(zframe_t *data, int type, void *socket)
{
  int rc;
  zframe_t *type_f = NULL;
  zmsg_t *msg = zmsg_new();

  zmsg_push(msg, data);

  SNetDistribPack(&type_f, &type, sizeof(int));
  zmsg_push(msg, type_f);

  rc = zmsg_send(&msg, socket);

  if (rc != 0) {
    //FIXME: Add some error handling.
  }

}


/*
* Local Calls
*/

htab_host_t *SNetDistribZMQHTabLookUp(int index)
{
  static htab_host_t *ret;
  ret = HTabGet(index);
  if (ret == NULL) {
    ret = HTabRLookUp(index);
  }

  return ret;
}

int SNetDistribZMQConnect(int index)
{
  int ret = -1;
  int retries = 100;
  int delay = 100;
  htab_host_t *host;

  while (true) {
    host = SNetDistribZMQHTabLookUp(index);
    if ((host == NULL) && --retries) {
      zclock_sleep(delay);
    } else {
      break;
    }
  }

  if (host != NULL) {

    sock_out[index] = zsocket_new(opts.ctx, ZMQ_PUSH);

    if (!sock_out[index]) {
      //FIXME: add some error handling.
    }

    zmq_setsockopt(sock_out[index], ZMQ_SNDHWM, &send_hwm, sizeof(int));
    ret = zsocket_connect(sock_out[index], "tcp://%s:%d/", host->host, host->data_port);

  } else {
    //FIXME: Connection failed...
  }

  return ret;
}

int SNetDistribZMQDisconnect(int index)
{
  int ret = -1;
  htab_host_t *host = SNetDistribZMQHTabLookUp(index);

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

htab_host_t *HTabRLookUp(int index)
{
  int type_v;

  zframe_t *data_f = NULL;
  zframe_t *type_f = NULL;
  zmsg_t *reply = NULL;
  htab_host_t *newhost = NULL;

  SNetDistribPack(&data_f, &index, sizeof(int));
  HTabMsg(data_f, htab_lookup, sockq);
  reply = zmsg_recv(sockq);
  type_f = zmsg_pop(reply);
  SNetDistribUnpack(&type_f, &type_v, sizeof(int));
  if (type_v == htab_fail) {
    //FIXME: lookup failed, what to do?
  } else {
    data_f = zmsg_pop(reply);
    newhost = HTabHostUnpack(&data_f);
    if (hostid[index] == NULL) {
      HTabSet(newhost, index);
    }
    zframe_destroy(&data_f);
  }

  zmsg_destroy(&reply);
  zframe_destroy(&type_f);

  return newhost;
}

