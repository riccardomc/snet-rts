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

static int node_location;
static int sync_port;
static int data_port;
static int net_size;
static int node_location;
static char root_addr[300];

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
      if ((srcframe_size - dst_size) != 0) {
        byte *newdst = SNetMemAlloc(srcframe_size - dst_size);
        memcpy(newdst, srcframe_data + count, srcframe_size - dst_size);
        zframe_reset(*srcframe, newdst, srcframe_size - dst_size);
        SNetMemFree(newdst);
      } else {
        zframe_destroy(srcframe);
        *srcframe = NULL;
      }
    }

  } else {
    dst = NULL;
  }
}

void SNetDistribZMQHostsInit(int argc, char **argv)
{
  int i;
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

  SNetDistribZMQHTabInit(data_port, sync_port, node_location, root_addr);
  node_location = HTabNodeLocation();

  SNetDistribZMQHTabStart();

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

  rc = HTabSend(msg, destination);
  if (rc != 0) {
    SNetUtilDebugFatal("ZMQDistrib: Cannot send message to  %d (%s): zmsg_send (%d)",
        destination, SNetDistribZMQHTabLookUp(destination)->host, rc);
  }
}

void SNetDistribImplementationInit(int argc, char **argv, snet_info_t *info)
{
  SNetDistribZMQHostsInit(argc, argv);
}

void SNetDistribLocalStop(void)
{
  SNetDistribZMQHTabStop();
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
  static snet_msg_t result;
  static zframe_t *source_f;
  static int source_v;
  static zframe_t *type_f;
  static zframe_t *payload_f;

  static zmsg_t *msg;

  msg = HTabRecv();

  source_f = zmsg_pop(msg);
  type_f = zmsg_pop(msg);
  payload_f = zmsg_pop(msg);

  int type_v = -1;
  SNetDistribUnpack(&type_f, &type_v, sizeof(int));
  result.type = type_v;

  switch(result.type) {
    case snet_block:
    case snet_unblock:
      /* DO NOTHING */
      break;
    case snet_rec:
      result.rec = SNetRecDeserialise(&payload_f);
      result.dest = SNetDestDeserialise(&payload_f);
      SNetDistribUnpack(&source_f, &result.dest->node, sizeof(int));
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

void SNetDistribSendRecord(snet_dest_t *dest, snet_record_t *rec)
{
  static zframe_t *payload;
  payload = NULL;
  SNetRecSerialise(rec, &payload);
  SNetDestSerialise(dest, &payload);
  SNetDistribZMQSend(payload, snet_rec, dest->node);
}

void SNetDistribBlockDest(snet_dest_t *dest)
{
  (void) dest; /* NOT USED */
}

void SNetDistribUnblockDest(snet_dest_t *dest)
{
  (void) dest; /* NOT USED */
}

void SNetDistribUpdateBlocked(void)
{
  int rc;
  char pload_v = 0;
  int type_v = snet_update;
  zframe_t *payload_f = NULL;
  zframe_t *source_f = NULL;
  zframe_t *type_f = NULL;
  zmsg_t *msg = zmsg_new();

  SNetDistribPack(&payload_f, &pload_v, sizeof(pload_v));
  zmsg_push(msg, payload_f);
  SNetDistribPack(&type_f, &type_v, sizeof(int));
  zmsg_push(msg, type_f);
  SNetDistribPack(&source_f, &node_location, sizeof(int));
  zmsg_push(msg, source_f);

  rc = HTabSend(msg, node_location);
  if (rc != 0) {
    SNetUtilDebugFatal("ZMQDistrib: Cannot send message to self: zmsg_send (%d)", rc);
  }
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
  int i = SNetDistribZMQHTabCount();

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
