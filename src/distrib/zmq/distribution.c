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
int port_in = 20737;

htab_t *hosts
size_t node_location;

void SNetDistribZMQError(int errno)
{
  printf("Error: %s", strerror(errno));
  //FIXME: Do something to propagate the error?
  exit(EXIT_FAILURE);
}

//FIXME: These (SnetDistribZMQHosts*) will be eventually abstracted.
char *SNetDistribZMQHostsLookup(size_t addr)
{
  return htab_lookup(host_table, addr);
}

void SNetDistribZMQHostsInit(int argc, char **argv)
{
  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "-nodesTable") == 0) {
      if ((i + 1) < argc) {
        host_table = htab_fread(argv[i+1]);
        if (!host_table)
          SNetDistribZMQError(errno())
      }
    } else if (strcmp(argv[i], "-nodeId") == 0) {
      node_location = 0;
    }
  }

  //FIXME: -nodesTable & -nodeId should be mandatory.
  
  context = zctx_new();
  if (!context) SNetDistribZMQError(errno());
  sock_in = zsocket_new(context, ZMQ_PULL);
  if (!sock_in) SNetDistribZMQError(errno());

  int rc = zsock_bind(sock_in, "tcp://*:%d", port);
  if (rc != port_in) {
    //FIXME: bind failed. rc is not the errno. assert?
    SNetDistribZMQError(rc);
  }
  
}

void SNetDistribZMQHostsStop()
{
  htab_free(hosts);
}

void SNetDistribImplementationInit(int argc, char **argv, snet_info_t *info)
{
  context = zctx_new();
  if (!context)
      SNetDistribZMQError(errno());

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
  zctx_destroy (&context);
  SNetDistribZMQHostsStop();
}

/**
 * Add own identity to msg and send it to destination
 */
void SNetDistribZMQSend(zmsg_t **msg, int destination) 
{
  zframe_t *source;
  PackInt(&origin, 1, &node_location);
  zmsg_push(*msg, source);

  void *sock_out = zsocket_new(context, ZMQ_PUSH); //FIXME: create sock every time?
  
  zsocket_connect(sock_out, SNetDistribZMQHostsLookup(destination)) //FIXME: what if lookup fails?
  zmsg_send(msg, sock_out);
  
  zframe_destroy(&source);
  zsocket_destroy(sock_out); //FIXME: check what happens to msg when destroying socket right away
}

void SNetDistribFetchRef(snet_ref_t *ref) 
{
  zframe_t *type; //snet_comm_type_t enum
  zframe_t *payload; //data content

  zmsg_t *msg = zmsg_new(); //will contain the frames above

  SNetRefSerialise(ref, payload, &PackInt, &PackByte);
  zmsg_push(msg, payload);

  PackInt(&type, 1, snet_ref_fetch); 
  zmsg_push(msg, type);

  SNetDistribZMQSend(&msg, SNetRefNode(ref));

  zframe_destroy(&type);
  zframe_destroy(&payload);
}

void SNetDistribUpdateRef(snet_ref_t *ref, int count) 
{
  zframe_t *type;
  zframe_t *payload;

  zmsg_t *msg = zmsg_new();

  SNetRefSerialise(ref, &payload, &PackInt, &PackByte);
  PackInt(&buf, 1, &count); //why &??
  zmsg_push(msg, payload);

  PackInt(&type, 1, snet_ref_update);
  zmsg_push(msg, type);

  SNetDistribZMQSend(&msg, SNetRefNode(ref));

  zframe_destroy(&type);
  zframe_destroy(&payload);
}

snet_msg_t SNetDistribRecvMsg(void) 
{
  int count;
  snet_msg_t result;
  zframe_t *source_f;
  zframe_t *type_f;
  zframe_t *payload_f;

  zmsg_t *msg;

  msg = zmsg_receive(sock_in); //FIXME: do some proper error checking

  source_f = zmsg_pop(msg);
  type_f = zmsg_pop(msg);
  payload_f = zmsg_pop(msg);

  UnpackInt(type_f, zframe_size(type_f), &result.type);

  switch(result.type) {
    case snet_rec:
      result.rec = SnetRecDeserialise(payload_f, &UnpackInt, &UnpackRef);
    case snet_block:
    case snet_unblock:
      UnpackInt(source_f, zframe_size(source_f), &result.dest.node);
      UnpackDest(payload_f, zframe_size(payload_f), &result.dest);
      break;
    case snet_ref_set:
      result.ref = SNetRefDeserialise(payload_f, &UnpackInt, &UnpackByte);
      //FIXME: what is this?
      result.data = (uintptr_t) SNetInterfaceGet(SNetRefInterface(result.ref))->unpackfun(&recvBuf);
      break;
    case snet_ref_fetch:
      result.ref = SNetRefDeserialise(payload_f, &UnpackInt, &UnpackByte);
      UnpackInt(source_f, zframe_size(source_f), &result.data);
      break;
    case snet_ref_update:
      result.ref = SNetRefDeserialise(payload_f, &UnpackInt, &UnpackByte);
      UnpackInt(payload_f, 1, &result.val);
      break;
    default:
      break;
  }

  zframe_destroy(source_f);
  zframe_destroy(type_f);
  zframe_destroy(payload_f);
  zmsg_destroy(msg);

  return result;
}

void SNetDistribSendRecord(snet_dest_t dest, snet_record_t *rec)
{
  zframe_t *type;
  zframe_t *payload;

  zmsg_t *msg = zmsg_new();

  SNetRecSerialise(rec, &payload, &PackInt, &PackRef);
  PackDest(&payload, &dest);
  zmsg_push(msg, payload);

  PackInt(&type, 1, snet_rec);
  zmsg_push(msg, type);

  SNetDistribZMQSend(&msg, dest.node);
  
  zframe_destroy(&type);
  zframe_destroy(&payload);
}

void SNetDistribBlockDest(snet_dest_t dest)
{
  zframe_t *type;
  zframe_t *payload;

  zmsg_t *msg = zmsg_new();

  PackDest(&payload, &dest);
  zmsg_push(msg, payload);

  PackInt(&type, 1, snet_block);
  zmsg_push(msg, type);

  SNetDistribZMQSend(&msg, dest.node);
  
  zframe_destroy(&type);
  zframe_destroy(&payload);
}

void SNetDistribUnblockDest(snet_dest_t dest)
{
  zframe_t *type;
  zframe_t *payload;

  zmsg_t *msg = zmsg_new();

  PackDest(&payload, &dest);
  zmsg_push(msg, payload);

  PackInt(&type, 1, snet_unblock);
  zmsg_push(msg, type);

  SNetDistribZMQSend(&msg, dest.node);
  
  zframe_destroy(&type);
  zframe_destroy(&payload);
}

void SNetDistribUpdateBlocked(void)
{
  zframe_t *type;
  zframe_t *payload;

  zmsg_t *msg = zmsg_new();

  PackByte(&payload, 1, 0); //FIXME: do we need empty payload messages?
  zmsg_push(msg, payload);

  PackInt(&type, 1, snet_update);
  zmsg_push(msg, type);

  SNetDistribZMQSend(&msg, node_location);
  
  zframe_destroy(&type);
  zframe_destroy(&payload);
}

void SNetDistribSendData(snet_ref_t *ref, void *data, void *dest)
{
  zframe_t *type;
  zframe_t *payload;

  zmsg_t *msg = zmsg_new();

  SNetRefSerialise(ref, &payload, &PackInt, &PackByte);
  SNetInterfaceGet(SNetRefInterface(ref))->packfun(data, &payload);
  zmsg_push(msg, payload);

  PackInt(&type, 1, snet_ref_set);
  zmsg_push(msg, type);

  SNetDistribZMQSend(&msg, (uintptr_t) dest);

  zframe_destroy(&type);
  zframe_destroy(&payload);
}

void SNetDistribGlobalStop(void) {
  //FIXME: to be implemented.
}

int SNetDistribGetNodeId(void) { return node_location; }

bool SNetDistribIsNodeLocation(int loc) { return node_location == loc; }

bool SNetDistribIsRootNode(void) { return node_location == 0; }

