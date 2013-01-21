/**
* Riccardo M. Cefala                      Sun Jan  6 18:31:18 CET 2013
*
* ZMQ Distribution implementation attempt.
*
* TODO:
* - we'll need a simple int to addr mapping. DONE
* - packing and unpacking (serialization). DONE
* - rewrite functions with ZMQ. DONE
* - how to handle streams? proper protocol? DONE (PUSH/PULL)
*
**/

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
#include "pack.h"
#include "record.h"
#include "reference.h"
#include "htab.h"

zctx_t *context;
void *sock_in;
void **sock_out;
int port_in = 20737;

htab_t *host_table;
int node_location;

//FIXME: These (SnetDistribZMQHosts*) will be eventually abstracted.
char *SNetDistribZMQHostsLookup(size_t addr)
{
  return htab_lookup(host_table, addr);
}

void SNetDistribZMQHostsStop()
{
  htab_free(host_table);
}

int SNetDistribZMQHostsCount()
{
  return htab_size(host_table);
}

void SNetDistribZMQHostsInit(int argc, char **argv)
{
  int i;
  host_table = NULL;
  node_location = -1;

  for (i = 0; i < argc; i++) {
    if (strcmp(argv[i], "-nodesTable") == 0) {
      if ((i + 1) < argc) {
        host_table = htab_fread(argv[i+1]);
        if (!host_table)
          SNetUtilDebugFatal("ZMQDistrib: %s", strerror(errno));
      }
    } else if (strcmp(argv[i], "-nodeId") == 0) {
      node_location = atoi(argv[i+1]);
    }
  }

  if (host_table == NULL) {
    SNetUtilDebugFatal("ZMQDistrib: -nodesTable is mandatory");
  }

  if (node_location == -1) {
    SNetUtilDebugFatal("ZMQDistrib: -nodeId is mandatory");
  } 
  
  context = zctx_new();
  if (!context) SNetUtilDebugFatal("ZMQDistrib: %s", strerror(errno));
  sock_in = zsocket_new(context, ZMQ_PULL);
  if (!sock_in) SNetUtilDebugFatal("ZMQDistrib: %s", strerror(errno));

  sock_out = SNetMemAlloc(SNetDistribZMQHostsCount() * sizeof(void *));
  for (i = 0 ; i < SNetDistribZMQHostsCount(); i++) {
    sock_out[i] = zsocket_new(context, ZMQ_PUSH);
    if (!sock_out[i]) SNetUtilDebugFatal("ZMQDistrib: %s", strerror(errno));
  }

  zctx_set_linger(context, 10000);

  int rc = zsocket_bind(sock_in, SNetDistribZMQHostsLookup(node_location));
  if (rc != 0) {
    SNetUtilDebugFatal("ZMQDistrib: Socket bind failed %d", rc);
  } else {
    printf("ZMQDistrib: Node %d listening on %s\n", node_location, SNetDistribZMQHostsLookup(node_location));
  }
  
}

void SNetDistribImplementationInit(int argc, char **argv, snet_info_t *info)
{
  context = zctx_new();
  if (!context)
    SNetUtilDebugFatal("ZMQDistrib: %s", strerror(errno));

  SNetDistribZMQHostsInit(argc, argv);

  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "-debugWait") == 0) {
      volatile int stop = 0;
      printf("PID %d ready for attach\n", getpid());
      fflush(stdout);
      while (0 == stop) sleep(5);
      break;
    } 
  }

}

void SNetDistribLocalStop(void)
{ 
  printf("LocalStop called\n");
  zctx_destroy(&context);
  SNetMemFree(sock_out);
  SNetDistribZMQHostsStop();
}

/**
 * Pack type and own identity to msg before sending it to destination
 */
void SNetDistribZMQSend(zframe_t *payload, int type, int destination) 
{
  int rc;
  zframe_t *source_f = NULL;
  zframe_t *type_f = NULL;
  zmsg_t *msg = zmsg_new();
 
  zmsg_push(msg, payload);

  PackInt(&type_f, 1, &type);
  zmsg_push(msg, type_f);

  PackInt(&source_f, 1, &node_location);
  zmsg_push(msg, source_f);
  
  rc = zsocket_connect(sock_out[destination], SNetDistribZMQHostsLookup(destination)); 
  if (rc != 0) {
    SNetUtilDebugFatal("ZMQDistrib: Cannot reach node %d (%s): zsocket_connect returned (%d)",
        destination, SNetDistribZMQHostsLookup(destination), rc);
  }

  printf("-> Sending (%d)\n", destination);
  rc = zmsg_send(&msg, sock_out[destination]);
  if (rc != 0) {
    SNetUtilDebugFatal("ZMQDistrib: Cannot send message to  %d (%s): zmsg_send returned (%d)",
        destination, SNetDistribZMQHostsLookup(destination), rc);
  }
}

void SNetDistribFetchRef(snet_ref_t *ref) 
{
  zframe_t *payload = NULL; 
  SNetRefSerialise(ref, &payload, &PackInt, &PackByte);

  SNetDistribZMQSend(payload, snet_ref_fetch, SNetRefNode(ref));
}

void SNetDistribUpdateRef(snet_ref_t *ref, int count) 
{
  zframe_t *payload = NULL;
  SNetRefSerialise(ref, &payload, &PackInt, &PackByte);
  PackInt(&payload, 1, &count);
  SNetDistribZMQSend(payload, snet_ref_update, SNetRefNode(ref));
}

snet_msg_t SNetDistribRecvMsg(void) 
{
  int data_v;
  snet_msg_t result;
  zframe_t *source_f;
  zframe_t *type_f;
  zframe_t *payload_f;

  zmsg_t *msg;

  zclock_sleep(100);
  msg = zmsg_recv(sock_in); //FIXME: do some proper error checking?
  
  source_f = zmsg_pop(msg);
  type_f = zmsg_pop(msg);
  payload_f = zmsg_pop(msg);

  printf("<- Recv\n");
  //zframe_print(source_f, "S: ");
  //zframe_print(type_f, "T: ");
  //zframe_print(payload_f, "P: ");

  int type_v = -1;
  UnpackInt(&type_f, 1, &type_v);
  result.type = type_v;

  switch(result.type) {
    case snet_rec:
      result.rec = SNetRecDeserialise(&payload_f, &UnpackInt, &UnpackRef);
      
    case snet_block:
    case snet_unblock:
      UnpackInt(&source_f, 1, &result.dest.node);
      UnpackDest(&payload_f, &result.dest);
      break;
    case snet_ref_set:
      result.ref = SNetRefDeserialise(&payload_f, &UnpackInt, &UnpackByte);
      result.data = (uintptr_t) SNetInterfaceGet(SNetRefInterface(result.ref))->unpackfun(zframe_data(payload_f));
      break;
    case snet_ref_fetch:
      result.ref = SNetRefDeserialise(&payload_f, &UnpackInt, &UnpackByte);
      UnpackInt(&source_f, 1, &data_v);
      result.data = data_v;
      break;
    case snet_ref_update:
      result.ref = SNetRefDeserialise(&payload_f, &UnpackInt, &UnpackByte);
      UnpackInt(&payload_f, 1, &result.val);
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
  zframe_t *payload = NULL;
  SNetRecSerialise(rec, &payload, &PackInt, &PackRef);
  PackDest(&payload, &dest);
  SNetDistribZMQSend(payload, snet_rec, dest.node);
}

void SNetDistribBlockDest(snet_dest_t dest)
{
  zframe_t *payload;
  PackDest(&payload, &dest);
  SNetDistribZMQSend(payload, snet_block, dest.node);
}

void SNetDistribUnblockDest(snet_dest_t dest)
{
  zframe_t *payload = NULL;
  PackDest(&payload, &dest);
  SNetDistribZMQSend(payload, snet_unblock, dest.node);
}

void SNetDistribUpdateBlocked(void)
{
  zframe_t *payload;
  PackByte(&payload, 1, 0); //FIXME: do we need empty payload messages?
  SNetDistribZMQSend(payload, snet_update, node_location);
}

void SNetDistribSendData(snet_ref_t *ref, void *data, void *dest)
{
  zframe_t *payload;
  char data_buf[4096];
  SNetRefSerialise(ref, &payload, &PackInt, &PackByte);
  SNetInterfaceGet(SNetRefInterface(ref))->packfun(data, &data_buf);
  PackByte(&payload, 4096, data_buf); //FIXME: I don't know actual data size!
  SNetDistribZMQSend(payload, snet_ref_set,  (uintptr_t) dest);
}

void SNetDistribGlobalStop(void)
{
  char pload_v = 0;
  int i = SNetDistribZMQHostsCount();

  for (i--; i >= 0; i--) {
    zframe_t *payload = NULL;
    PackByte(&payload, 1, &pload_v);

    SNetDistribZMQSend(payload, snet_stop, i);
  }
}

int SNetDistribGetNodeId(void) { return node_location; }

bool SNetDistribIsNodeLocation(int loc) { return node_location == loc; }

bool SNetDistribIsRootNode(void) { return node_location == 0; }

