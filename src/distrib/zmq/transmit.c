#include <zmq.h>
#include <czmq.h>

#include "distribcommon.h"
#include "interface_functions.h"
#include "record.h"
#include "reference.h"

zmsg_t *SNetDistribZMQRecv(void);
void SNetDistribZMQSend(zframe_t *payload, int type, int destination);

void SNetDistribFetchRef(snet_ref_t *ref)
{
  static zframe_t *payload;
  payload = NULL;
  SNetRefSerialise(ref, &payload);
  SNetDistribZMQSend(payload, snet_ref_fetch, SNetRefNode(ref));
}

void SNetDistribUpdateRef(snet_ref_t *ref, int count)
{
  static zframe_t *payload;
  payload = NULL;
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

  msg = (zmsg_t *)SNetDistribZMQRecv();

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
      SNetDistribUnpack(&payload_f, &result.dest, sizeof(snet_dest_t));
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
  SNetDistribPack(&payload, &dest, sizeof(snet_dest_t));
  SNetDistribZMQSend(payload, snet_rec, dest.node);
}

void SNetDistribBlockDest(snet_dest_t dest)
{
  (void) dest; /* NOT USED */
}

void SNetDistribUnblockDest(snet_dest_t dest)
{
  (void) dest; /* NOT USED */
}

void SNetDistribUpdateBlocked(void)
{
  static zframe_t *payload;
  char pload_v = 0;
  payload = NULL;
  SNetDistribPack(&payload, &pload_v, sizeof(char));
  SNetDistribZMQSend(payload, snet_update, SNetDistribGetNodeId());
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

