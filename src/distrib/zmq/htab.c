#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <threading.h>
#include "debug.h"
#include "host.h"
#include "htab.h"
#include "cloud.h"
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

typedef struct {
  sendfun             sendfun[SNET_ZMQ_HTABLN]; //send function for each host
  snet_host_t         *id[SNET_ZMQ_HTABLN];     //hosts index
  snet_host_list_t    *list;                    //dynamic host list
  snet_idstack_list_t *stack;                   //next available id stack
  zmutex_t            *mutex;                   //table mutex
} htab_table_t;

static htab_opts_t opts;
static htab_sockets_t sockets;
static htab_table_t table;    //host table

static bool running = true;

/*
* Id Stack (second level index)
*/

static void HTabSetNextId(int index)
{
  int last;
  if (SNetIdStackListLength(table.stack) == 0) {
    SNetIdStackListAppendStart(table.stack, index);
  } else {
    /* we want previously removed index on top */
    last = SNetIdStackListPopEnd(table.stack);
    SNetIdStackListAppendEnd(table.stack, index);
    SNetIdStackListAppendEnd(table.stack, last);
  }
}

static void HTabInitId()
{
  table.stack = SNetIdStackListCreate(0);
  HTabSetNextId(0);
}

static void HTabDestroyId()
{
  SNetIdStackListDestroy(table.stack);
}

static int HTabNextId()
{
  int index = SNetIdStackListPopStart(table.stack);
  if (SNetIdStackListLength(table.stack) == 0) {
    HTabSetNextId(index + 1);
  }
  return index;
}

/*
* Host Table 
*/

void HTabInit() 
{
  zmutex_lock(table.mutex);
  table.list = SNetHostListCreate(0);
  HTabInitId();
  for (int i = 0 ; i < SNET_ZMQ_HTABLN ; i++) {
    table.id[i] = NULL;
  }
  zmutex_unlock(table.mutex);
}

void HTabFree()
{
  SNetHostListDestroy(table.list);
  HTabDestroyId();
}

int HTabAdd(snet_host_t *h)
{
  static int index;
  zmutex_lock(table.mutex);
  SNetHostListAppendEnd(table.list, h);
  do {
    index = HTabNextId();
  } while (table.id[index] != NULL);
  table.id[index] = h;
  zmutex_unlock(table.mutex);
  return index;
}

void HTabSet(snet_host_t *h, int index)
{
  static int stack_index, list_index, last_id;
  stack_index = -1;
  list_index = -1;

  zmutex_lock(table.mutex);

  stack_index = SNetIdStackListFind(table.stack, index);
  if (stack_index == (SNetIdStackListLength(table.stack) - 1)) {
    //it's last element of stack: preserve nextid.
    last_id = SNetIdStackListPopEnd(table.stack);
    SNetIdStackListAppendEnd(table.stack, last_id + 1);
  } else if (stack_index >= 0) {
      SNetIdStackListRemove(table.stack, stack_index);
  } else if (table.id[index] != NULL) {
      //index not in stack, could be occupied: free it.
      list_index = SNetHostListFind(table.list, table.id[index]);
      if (list_index >= -1) {
        SNetHostFree(SNetHostListRemove(table.list, list_index));
      }
  }

  SNetHostListAppendEnd(table.list, h);
  table.id[index] = h;

  zmutex_unlock(table.mutex);
}

int HTabRemove(int index)
{
  static int list_index;
  zmutex_lock(table.mutex);
  if (table.id[index] != NULL) {
    list_index = SNetHostListFind(table.list, table.id[index]);
    if (list_index >= 0) {
      SNetHostFree(SNetHostListRemove(table.list, list_index));
    }
    table.id[index] = NULL;
    HTabSetNextId(index);
  }
  zmutex_unlock(table.mutex);
  return list_index;
}

snet_host_t *HTabLookUp(int index)
{
  static snet_host_t *ret;
  zmutex_lock(table.mutex);
  ret = table.id[index];
  zmutex_unlock(table.mutex);
  return ret;
}

void HTabDump()
{
  zmutex_lock(table.mutex);
  printf("\n");
  for (int i = 0 ; i < 5; i++) {
    printf("%d: ", i);
    SNetHostDump(table.id[i]);
    printf("\n");
  }
  printf("-\n");
  zmutex_unlock(table.mutex);
}

char *HTabGetHostName()
{
  return opts.host_name;
}


/** 
* Threading 
*/

static int HTabFirstSend(zmsg_t *msg, int dest); //FIXME: make compiler happy
static void HTabSyncSend(zframe_t *data, int type, void *socket);
static zmsg_t *HTabSyncRecv(void *socket);

// inform root node that the host is parting the network
static void HTabPart()
{
  zframe_t *data_f = NULL;
  zmsg_t *msg = NULL;

  SNetDistribPack(&data_f, &opts.node_location, sizeof(int));
  HTabSyncSend(data_f, htab_part, sockets.request);
  msg = HTabSyncRecv(sockets.request); //we don't care about the payload.
  zmsg_destroy(&msg);
}

/*
* Root node listen on sync_port.
* Regular nodes connect to root_addr and send their host information to receive id.
*/
static void HTabConnect()
{
  if (opts.node_location == 0) {
    zsocket_bind(sockets.reply, "tcp://*:%d/", opts.sync_port);
    //Add yourself as node 0 and connect to yourself.
    HTabAdd(SNetHostCreate(opts.host_name, "*", opts.data_port, opts.sync_port));
    zsocket_connect(sockets.request, "tcp://%s:%d/", opts.host_name, opts.sync_port);

  } else {
    zframe_t *type_f = NULL;
    zframe_t *data_f = NULL;
    zmsg_t *msg = NULL;
    snet_host_t *host;

    zsocket_connect(sockets.request, opts.root_addr);

    SNetDistribPack(&data_f, &opts.node_location, sizeof(opts.node_location)); //request id
    host = SNetHostCreate(opts.host_name, "*", opts.data_port, opts.sync_port);
    SNetHostPack(host, &data_f);
    HTabSyncSend(data_f, htab_host, sockets.request); //send host

    msg = HTabSyncRecv(sockets.request); //recv id
    type_f = zmsg_pop(msg);
    data_f = zmsg_pop(msg);
    SNetDistribUnpack(&data_f, &opts.node_location, sizeof(int));
    HTabSet(host, opts.node_location); //add yourself

    zframe_destroy(&type_f);
    zframe_destroy(&data_f);
    zmsg_destroy(&msg);
 }
}

// init host table and sockets
void SNetDistribZMQHTabInit(int dport, int sport, int node_location, char *raddr, char *hname, int on_cloud)
{
  opts.ctx = zctx_new();

  if (opts.ctx == NULL) {
    SNetUtilDebugFatal("ZMQDistrib: %s", strerror(errno));
  }

  //grace period before destroying sockets
  zctx_set_linger(opts.ctx, 1000);

  opts.data_port = dport;
  opts.sync_port = sport;
  opts.node_location = node_location;
  strncpy(opts.root_addr, raddr, SNET_ZMQ_ADDRLN);
 
  if (strcmp(hname, "") == 0) {
    opts.host_name = SNetUtilSysHostname();
    SNetUtilSysEnvStr("SNET_ZMQ_HOSTNM", opts.host_name, SNET_ZMQ_HOSTLN);
  } else {
    opts.host_name = hname;
  }

  opts.on_cloud = on_cloud;

  //set default values and check env vars
  sockets.send_highwm = SNET_ZMQ_SEND_HIGHWM_D;
  sockets.recv_highwm = SNET_ZMQ_RECV_HIGHWM_D;
  sockets.data_timeo = SNET_ZMQ_DATA_TIMEO_D;
  sockets.sync_timeo = SNET_ZMQ_SYNC_TIMEO_D;
  sockets.look_timeo = SNET_ZMQ_LOOK_TIMEO_D;
  SNetUtilSysEnvInt("SNET_ZMQ_SEND_HIGHWM", &sockets.send_highwm);
  SNetUtilSysEnvInt("SNET_ZMQ_RECV_HIGHWM", &sockets.recv_highwm);
  SNetUtilSysEnvInt("SNET_ZMQ_DATA_TIMEO", &sockets.data_timeo);
  SNetUtilSysEnvInt("SNET_ZMQ_SYNC_TIMEO", &sockets.sync_timeo);
  SNetUtilSysEnvInt("SNET_ZMQ_LOOK_TIMEO", &sockets.look_timeo);

  //init sync and data sockets
  sockets.reply = zsocket_new(opts.ctx, ZMQ_REP);
  sockets.request = zsocket_new(opts.ctx, ZMQ_REQ);
  zsocket_set_rcvtimeo(sockets.reply, sockets.sync_timeo);
  zsocket_set_sndtimeo(sockets.reply, sockets.sync_timeo);
  zsocket_set_rcvtimeo(sockets.request, sockets.sync_timeo);
  zsocket_set_sndtimeo(sockets.request, sockets.sync_timeo);

  for (int i = 0 ; i < SNET_ZMQ_HTABLN ; i++) {
    sockets.out[i] = NULL;
    table.sendfun[i] = &HTabFirstSend;
  }

  sockets.in = zsocket_new(opts.ctx, ZMQ_PULL);
  if (sockets.in == NULL) {
    SNetUtilDebugFatal("ZMQDistrib: %s", strerror(errno));
  }
  zsocket_set_rcvtimeo(sockets.in, sockets.data_timeo);
  zsocket_set_rcvhwm(sockets.in, sockets.recv_highwm);
  zsocket_bind(sockets.in, "tcp://*:%d", opts.data_port);

  table.mutex = zmutex_new();

  HTabInit();

  HTabConnect();

}

// start host table thread
void SNetDistribZMQHTabStart(void)
{
  SNetThreadingSpawn( ENTITY_other, -1, NULL, "<zmq_htab>", &SNetDistribZMQHTab, NULL);

}

/*
* terminate host table thread
* root node waits for all nodes to part the network
* regular nodes part
*/
void SNetDistribZMQHTabStop(void)
{
  if (opts.node_location == 0) {
    //FIXME: introduce timeout?
    while (SNetDistribZMQHTabCount() > 1) {
      zclock_sleep(1);
    }
  } else {
    HTabPart();
  }
  zmutex_lock(table.mutex);
  running = false;
  zmutex_unlock(table.mutex);
}

// root node loop
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

    msg = HTabSyncRecv(sockets.reply);
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
        HTabSyncSend(reply_f, htab_id, sockets.reply);
        break;

      case htab_id:
        SNetDistribUnpack(&data_f, &id, sizeof(int));
        host = HTabLookUp(id);
        if (host == NULL) {
          SNetDistribPack(&reply_f, &id, sizeof(int));
          HTabSyncSend(reply_f, htab_fail, sockets.reply);
          if (opts.on_cloud == 2) {
            SNetCloudSpawn(id);
          }
        } else {
          SNetHostPack(host, &reply_f);
          HTabSyncSend(reply_f, htab_host, sockets.reply);
        }
        break;

      case htab_part:
        SNetDistribUnpack(&data_f, &id, sizeof(int));
        HTabRemove(id);
        SNetDistribPack(&reply_f, &id, sizeof(int));
        HTabSyncSend(reply_f, htab_part, sockets.reply);
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

// regular nodes loop
static void HTabLoopNode(void *args) 
{

  while (running) {
    //FIXME: nodes control protocol?
    zclock_sleep(1000);
  }

}

// thread task. Calls appropriate loop
void SNetDistribZMQHTab(void *args)
{

  if (SNetDistribGetNodeId() == 0) {
    HTabLoopRoot(args);
  } else {
    HTabLoopNode(args);
  }

  free(opts.host_name);
  HTabFree();
  zctx_destroy(&opts.ctx);
  zmutex_destroy(&table.mutex);

}

/*
* Messages Interface
*/

// Callable wrapper function
int HTabSend(zmsg_t *msg, int destination)
{
  return table.sendfun[destination](msg, destination);
}

// Regular serialized send
static int HTabRegularSend(zmsg_t *msg, int dest)
{
  static int ret = -1;
  zmutex_lock(table.mutex);
  ret = zmsg_send(&msg, sockets.out[dest]);
  zmutex_unlock(table.mutex);
  return ret;
}

int SNetDistribZMQConnect(int index);

// Connect before sending, then switch to regular sending
static int HTabFirstSend(zmsg_t *msg, int dest)
{
  static int ret = -1;
  ret = SNetDistribZMQConnect(dest);
  zmutex_lock(table.mutex);
  table.sendfun[dest] = &HTabRegularSend;
  ret = zmsg_send(&msg, sockets.out[dest]);
  zmutex_unlock(table.mutex);
  return ret;
}

// Wrapper for data receive function
zmsg_t *HTabRecv()
{
  zmsg_t *msg = zmsg_recv(sockets.in);

  if (msg == NULL) {
    SNetUtilDebugFatal("ZMQDistrib DATA recv timeout: %s", strerror(errno));
  }

  return msg;
}

// Sycnhronized receive function
static zmsg_t *HTabSyncRecv(void *socket)
{
  zmsg_t *msg = zmsg_recv(socket);
  if (msg == NULL) {
    SNetUtilDebugFatal("ZMQDistrib SYNC recv timeout: %s", strerror(errno));
  }

  return msg;
}

// Send synchronization htab_msg messages 
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

// Host table look up. If local fails, try remote.
snet_host_t *SNetDistribZMQHTabLookUp(int index)
{
  static snet_host_t *ret;
  ret = HTabLookUp(index);
  if (ret == NULL) {
    ret = HTabRemoteLookUp(index);
  }

  return ret;
}

// Connect socket to node at index. If unknown, look up until timeout.
int SNetDistribZMQConnect(int index)
{
  int ret = -1;
  int retries = sockets.look_timeo >= 0 ? sockets.look_timeo / 100 : -1;
  int delay = 100;
  snet_host_t *host;

  //look up until true or timeout (sockets.look_timeo)
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

    sockets.out[index] = zsocket_new(opts.ctx, ZMQ_PUSH);

    if (!sockets.out[index]) {
      SNetUtilDebugFatal("ZMQDistrib Cannot create socket to node: %d", index);
    }

    zsocket_set_sndhwm(sockets.out[index], sockets.send_highwm);
    ret = zsocket_connect(sockets.out[index], "tcp://%s:%d/", host->host, host->data_port);

  } else {
    SNetUtilDebugFatal("ZMQDistrib Cannot connect to node: %d. Lookup failed.", index);
  }

  return ret;
}

// Disconnect from node at index.
int SNetDistribZMQDisconnect(int index)
{
  int ret = -1;
  snet_host_t *host = SNetDistribZMQHTabLookUp(index);

  if (host != NULL) {
    ret = zsocket_disconnect(sockets.out[index], "tcp://%s:%d/", host->host, host->data_port);
  }

  sockets.out[index] = NULL;

  return ret;

}

// bind local address for listening.
void SNetDistribZMQBind(void *socket, int port)
{
  //FIXME: this is just a placeholder!
  int rc = zsocket_bind(socket, "tcp://%s:%d/", "localhost", port);
  if (rc < 0)
    exit(0);
}

// returns the number of entries in the host table
int SNetDistribZMQHTabCount()
{
  static int ret = -1;
  zmutex_lock(table.mutex);
  ret = SNetHostListLength(table.list);
  zmutex_unlock(table.mutex);
  return ret;
}

// returns the local node id
int HTabNodeLocation()
{
  return opts.node_location;
}

/*
* Remote calls
*/

// request to root node host information of node at index
snet_host_t *HTabRemoteLookUp(int index)
{
  int type_v;

  zframe_t *data_f = NULL;
  zframe_t *type_f = NULL;
  zmsg_t *reply = NULL;
  snet_host_t *newhost = NULL;

  SNetDistribPack(&data_f, &index, sizeof(int));
  HTabSyncSend(data_f, htab_id, sockets.request);
  reply = HTabSyncRecv(sockets.request);
  type_f = zmsg_pop(reply);
  SNetDistribUnpack(&type_f, &type_v, sizeof(int));
  if (type_v == htab_fail) {
    //FIXME: lookup failed, what to do?
  } else {
    data_f = zmsg_pop(reply);
    newhost = SNetHostUnpack(&data_f);
    if (table.id[index] == NULL) {
      HTabSet(newhost, index);
    }
    zframe_destroy(&data_f);
  }

  zmsg_destroy(&reply);
  zframe_destroy(&type_f);

  return newhost;
}

