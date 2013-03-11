/**
 * Riccardo M. Cefala                      Sun Jan  6 18:31:18 CET 2013
 *
 * ZMQ distribution layer implementation.
 *
 */

#include <zmq.h>
#include <czmq.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include "debug.h"
#include "memfun.h"
#include "imanager.h"
#include "distribution.h"
#include "distribcommon.h"
#include "interface_functions.h"
#include "omanager.h"
#include "record.h"
#include "reference.h"
#include "htab.h"

#define SNET_ZMQ_DPORT 20737
#define SNET_ZMQ_SPORT 20738

static zctx_t *context;
static void *sock_in;
static void *sock_sync;
static void **sock_out;

static htab_t *host_table;
static int node_location;

static int sync_port;
static int data_port;
static int net_size;
static char root_addr[256];

static zmutex_t *send_mtx;

/**
 * FIXME: These (SnetDistribZMQHosts*) will be eventually abstracted.
 */
char *SNetDistribZMQHostsLookup(int id)
{
  htab_host_t *host = htab_lookup(host_table, id);
  return host->host;
}

char *SNetDistribZMQHostsBind()
{
  htab_host_t *host = htab_lookup(host_table, node_location);
  return host->bind;
}

void SNetDistribZMQHostsStop()
{
  htab_free(host_table);
}

int SNetDistribZMQHostsCount()
{
  return htab_size(host_table);
}

void SNetDistribZMQConnect(void *socket, int id, int port)
{
  int rc = zsocket_connect(socket, "tcp://%s:%d/",
      SNetDistribZMQHostsLookup(id), port); 
      //SNetDistribZMQHostsLookup(id), port + id * 2); 
  if (rc != 0)
    SNetUtilDebugFatal("ZMQDistrib: Cannot reach node %d: zsocket_connect (%d)", id, rc);
}

void SNetDistribZMQDisconnect(void *socket, int id, int port)
{
  int rc = zsocket_disconnect(socket, "tcp://%s:%d/",
      SNetDistribZMQHostsLookup(id), port); 
      //SNetDistribZMQHostsLookup(id), port + id * 2); 
  if (rc != 0)
    SNetUtilDebugFatal("ZMQDistrib: Cannot reach node %d: zsocket_disconnect (%d)", id, rc);
}

void SNetDistribZMQBind(void *socket, int port)
{
  int rc = zsocket_bind(socket, "tcp://%s:%d/",
      SNetDistribZMQHostsBind(), port);
      //SNetDistribZMQHostsBind(), port + node_location * 2);
  if (rc < 0) 
    SNetUtilDebugFatal("ZMQDistrib: Socket bind failed: zsocket_bind: (%d) ", rc);
}

void SNetDistribZMQPack(zframe_t **dstframe, void *src, size_t count)
{
  if (*dstframe != NULL) {
    size_t src_size = count;
    size_t dstframe_size = zframe_size(*dstframe);
    byte *dstframe_data = zframe_data(*dstframe);

    byte *newsrc = SNetMemAlloc(src_size + dstframe_size);

    memcpy(newsrc, dstframe_data, dstframe_size);
    memcpy(newsrc + dstframe_size, src, src_size);

    zframe_reset(*dstframe, newsrc, dstframe_size + src_size);

    SNetMemFree(newsrc);

  } else {
    *dstframe = zframe_new(src, count);
  }
}

void  SNetDistribZMQUnpack(zframe_t **srcframe, void *dst, size_t count)
{
  if (*srcframe != NULL) {
    size_t dst_size = count;
    size_t srcframe_size = zframe_size(*srcframe);
    byte *srcframe_data = zframe_data(*srcframe);

    if(dst_size > srcframe_size) {
      dst = NULL;
    } else {
      memcpy(dst, srcframe_data, dst_size);

      //FIXME: SNetMemAlloc returns null when arg is 0!
      byte *newdst = malloc(srcframe_size - dst_size);

      memcpy(newdst, srcframe_data + count, srcframe_size - dst_size);
      zframe_reset(*srcframe, newdst, srcframe_size - dst_size);

      SNetMemFree(newdst);
    }

  } else {
    dst = NULL;
  }
}

void SNetDistribZMQSync()
{
  int i = 0;

  if (node_location == 0) {
    sock_sync = zsocket_new(context, ZMQ_REP);
    SNetDistribZMQBind(sock_sync, SNET_ZMQ_SPORT);

    while (i < (SNetDistribZMQHostsCount() - 1)) {
      char *str = zstr_recv(sock_sync);
      free(str);
      zstr_send(sock_sync, "");
      i++;
    }

  } else {
    sock_sync = zsocket_new(context, ZMQ_REQ);
    SNetDistribZMQConnect(sock_sync, 0, SNET_ZMQ_SPORT);
    zstr_send(sock_sync, "");
    char *str = zstr_recv(sock_sync);
    free(str);
  }

  zsocket_destroy(context, sock_sync);
}

void SNetDistribZMQJoin()
{
  void *sockp;
  void *sockq;
  char *hostname;
  zframe_t *htab_f = NULL;

  sockp = zsocket_new(context, ZMQ_REP);
  sockq = zsocket_new(context, ZMQ_REQ);
  zsocket_bind(sockp, "tcp://*:%d/", sync_port);
  zsocket_connect(sockq, root_addr);

  hostname = htab_gethostname();
  zstr_sendf(sockq, "%s %d %d", hostname, data_port, sync_port); //send host info
  char *str = zstr_recv(sockq); //recv id
  node_location = atoi(str);
  free(hostname);
  free(str);

  htab_f = zframe_recv(sockp); //recv host table
  zstr_send(sockp, ""); //send ack
  host_table = htab_unpack(&htab_f, &SNetDistribUnpack);
  zframe_destroy(&htab_f);
}

void SNetDistribZMQRoot()
{
  void *sockp;
  int h;
  char *hostname;
  zframe_t *htab_f = NULL;

  sockp = zsocket_new(context, ZMQ_REP);
  zsocket_bind(sockp, "tcp://*:%d/", sync_port);

  hostname = htab_gethostname();
  host_table = htab_alloc(net_size);
  strcpy(host_table->tab[0]->host, hostname);
  strcpy(host_table->tab[0]->bind, "*");
  host_table->tab[0]->sync_port = sync_port;
  host_table->tab[0]->data_port = data_port;
  free(hostname);

  for (h = 1; h < net_size; h++) {
    char *str = zstr_recv(sockp); //receive hostname
    zstr_sendf(sockp, "%d", h); //reply with id
    sscanf(str, "%s %d %d", host_table->tab[h]->host,
        &host_table->tab[h]->data_port,
        &host_table->tab[h]->sync_port);
    strcpy(host_table->tab[h]->bind, "*");
    free(str);
  }

  htab_pack(host_table, &htab_f, &SNetDistribPack);

  for (h = 1; h < net_size; h++) {
    void *sockq = zsocket_new(context, ZMQ_REQ);
    SNetDistribZMQConnect(sockq, h, host_table->tab[h]->sync_port);
    zframe_send(&htab_f, sockq, ZFRAME_REUSE); //send host table
    char *str = zstr_recv(sockq); //receive ack
    free(str);
    zsocket_destroy(context, sockq);
  }
  zsocket_destroy(context, sockp);
  zframe_destroy(&htab_f);
}

void SNetDistribZMQHostsInit(int argc, char **argv)
{
  int i;
  host_table = NULL;
  node_location = -1;
  data_port = SNET_ZMQ_DPORT;
  sync_port = SNET_ZMQ_SPORT;
  net_size = 1;

  for (i = 0; i < argc; i++) {
    if (strcmp(argv[i], "-raddr") == 0) {
      strcpy(root_addr, argv[i+1]);
    } else if (strcmp(argv[i], "-sport") == 0) {
      sync_port = atoi(argv[i+1]);
    } else if (strcmp(argv[i], "-dport") == 0) {
      data_port = atoi(argv[i+1]);
    } else if (strcmp(argv[i], "-root") == 0) {
      net_size = atoi(argv[i+1]);
      node_location = 0;
    }
  }

  if ((strcmp(root_addr, "") == 0) && (node_location != 0)) {
    SNetUtilDebugFatal("ZMQDistrib: <[-root <net_size>] | [-raddr <root_addr]>");
  }

  context = zctx_new();
  if (!context) SNetUtilDebugFatal("ZMQDistrib: %s", strerror(errno));
  zctx_set_linger(context, 1000); //wait 1s before destroying sockets

  sock_in = zsocket_new(context, ZMQ_PULL);
  if (!sock_in) SNetUtilDebugFatal("ZMQDistrib: %s", strerror(errno));

  zsocket_bind(sock_in, "tcp://*:%d", data_port);

  if (node_location == 0)
    SNetDistribZMQRoot();
  else
    SNetDistribZMQJoin();

  sock_out = SNetMemAlloc(SNetDistribZMQHostsCount() * sizeof(void *));
  for (i = 0 ; i < SNetDistribZMQHostsCount(); i++) {
    sock_out[i] = zsocket_new(context, ZMQ_PUSH);
    if (!sock_out[i]) SNetUtilDebugFatal("ZMQDistrib: %s", strerror(errno));
  }

  for (i = 0 ; i < SNetDistribZMQHostsCount(); i++)
    SNetDistribZMQConnect(sock_out[i], i, host_table->tab[i]->data_port);
}

inline static void SNetDistribZMQSend(zframe_t *payload, int type, int destination)
{
  int rc;
  zframe_t *source_f = NULL;
  zframe_t *type_f = NULL;
  zmsg_t *msg = zmsg_new();

  zmsg_push(msg, payload);

  SNetDistribPack(&type_f, &type, sizeof(int));
  zmsg_push(msg, type_f);

  SNetDistribPack(&source_f, &node_location, sizeof(int));
  zmsg_push(msg, source_f);

  //printf("-> Sending: %d\n", destination);

  /**
   * FIXME: This lock is necessary because the send in PUSH ZMQ sockets is
   * non-blocking, which can lead to the interleaving of receives and sends and
   * wrong reference counts updates.
   */
  zmutex_lock(send_mtx);
  rc = zmsg_send(&msg, sock_out[destination]);
  zmutex_unlock(send_mtx);
  if (rc != 0) {
    SNetUtilDebugFatal("ZMQDistrib: Cannot send message to  %d (%s): zmsg_send (%d)",
        destination, SNetDistribZMQHostsLookup(destination), rc);
  }
}

void SNetDistribImplementationInit(int argc, char **argv, snet_info_t *info)
{

  SNetDistribZMQHostsInit(argc, argv);
  send_mtx = zmutex_new();

  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "-debugWait") == 0) {
      volatile int stop = 0;
      printf("ZMQDistrib: PID %d on node %d listening on %s\n",
          getpid(), node_location, SNetDistribZMQHostsLookup(node_location));
      fflush(stdout);
      while (0 == stop) sleep(5);
      break;
    }
  }
}

void SNetDistribLocalStop(void)
{
  zctx_destroy(&context);
  SNetMemFree(sock_out);
  SNetDistribZMQHostsStop();
  zmutex_destroy(&send_mtx);
}

void SNetDistribFetchRef(snet_ref_t *ref)
{
  zframe_t *payload = NULL;
  SNetRefSerialise(ref, &payload);
  SNetDistribZMQSend(payload, snet_ref_fetch, SNetRefNode(ref));
}

void SNetDistribUpdateRef(snet_ref_t *ref, int count)
{
  zframe_t *payload = NULL;
  SNetRefSerialise(ref, &payload);
  SNetDistribPack(&payload, &count, sizeof(count));
  SNetDistribZMQSend(payload, snet_ref_update, SNetRefNode(ref));
}

snet_msg_t SNetDistribRecvMsg(void)
{
  snet_msg_t result;
  static zframe_t *source_f;
  static int source_v;
  static zframe_t *type_f;
  static zframe_t *payload_f;

  static zmsg_t *msg;

  msg = zmsg_recv(sock_in); //FIXME: do some proper error checking?

  source_f = zmsg_pop(msg);
  type_f = zmsg_pop(msg);
  payload_f = zmsg_pop(msg);

  int type_v = -1;
  SNetDistribUnpack(&type_f, &type_v, sizeof(int));
  result.type = type_v;

  switch(result.type) {
    case snet_rec:
      result.rec = SNetRecDeserialise(&payload_f);
    case snet_block:
    case snet_unblock:
      result.dest = *SNetDestDeserialise(&payload_f);
      SNetDistribUnpack(&source_f, &result.dest.node, sizeof(int));
      break;
    case snet_ref_set:
      result.ref = SNetRefDeserialise(&payload_f);
      result.data = (uintptr_t) SNetInterfaceGet(SNetRefInterface(result.ref))->unpackfun(&payload_f);
      break;
    case snet_ref_fetch:
      result.ref = SNetRefDeserialise(&payload_f);
      SNetDistribUnpack(&source_f, &source_v, sizeof(int));
      result.data = (uintptr_t)source_v;
      break;
    case snet_ref_update:
      result.ref = SNetRefDeserialise(&payload_f);
      SNetDistribUnpack(&payload_f, &result.val, sizeof(result.val));
      break;
    default:
      break;
  }

  zframe_destroy(&source_f);
  zframe_destroy(&type_f);
  zframe_destroy(&payload_f);
  zmsg_destroy(&msg);

  return result;
}

void SNetDistribSendRecord(snet_dest_t dest, snet_record_t *rec)
{
  static zframe_t *payload;
  payload = NULL;
  SNetRecSerialise(rec, &payload);
  SNetDestSerialise(&dest, &payload);
  SNetDistribZMQSend(payload, snet_rec, dest.node);
}

void SNetDistribBlockDest(snet_dest_t dest)
{
  zframe_t *payload = NULL;
  SNetDestSerialise(&dest, &payload);
  SNetDistribZMQSend(payload, snet_block, dest.node);
}

void SNetDistribUnblockDest(snet_dest_t dest)
{
  zframe_t *payload = NULL;
  SNetDestSerialise(&dest, &payload);
  SNetDistribZMQSend(payload, snet_unblock, dest.node);
}

void SNetDistribUpdateBlocked(void)
{
  char pload_v = 0;
  zframe_t *payload = NULL;
  SNetDistribPack(&payload, &pload_v, sizeof(pload_v));
  SNetDistribZMQSend(payload, snet_update, node_location);
}

void SNetDistribSendData(snet_ref_t *ref, void *data, void *dest)
{
  static zframe_t *payload;
  static uintptr_t dest_v;
  payload = NULL;
  dest_v = (uintptr_t)dest;
  SNetRefSerialise(ref, &payload);
  SNetInterfaceGet(SNetRefInterface(ref))->packfun(data, &payload);
  SNetDistribZMQSend(payload, snet_ref_set, dest_v);
}

void SNetDistribGlobalStop(void)
{
  char pload_v = 0;
  int i = SNetDistribZMQHostsCount();

  for (i--; i >= 0; i--) {
    zframe_t *payload = NULL;
    SNetDistribPack(&payload, &pload_v, sizeof(pload_v));

    SNetDistribZMQSend(payload, snet_stop, i);
  }
}

int SNetDistribGetNodeId(void)
{
  return node_location;
}

bool SNetDistribIsNodeLocation(int loc)
{
  return node_location == loc;
}

bool SNetDistribIsRootNode(void)
{
  return node_location == 0;
}

void SNetDistribPack(void *buf, void *src, size_t size)
{
  SNetDistribZMQPack((zframe_t **)buf, src, size);
}

void SNetDistribUnpack(void *buf, void *dst, size_t size)
{
  SNetDistribZMQUnpack((zframe_t **)buf, dst, size);
}
