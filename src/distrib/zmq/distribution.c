/**
* Riccardo M. Cefala                      Sun Jan  6 18:31:18 CET 2013
*
* ZMQ Distribution implementation attempt.
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
void *sock_sync;
void **sock_out;

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

/*
 *FIXME: Heavily relies on ipc, must be adapted for tcp.
 */
int SNetDistribZMQSync(int type)
{
  int rc = 0;
  int i = 0;
  char syncaddr[256]; 

  sock_sync = zsocket_new(context, type);
  if (!sock_sync) return -1;

  strcpy(syncaddr, SNetDistribZMQHostsLookup(0));
  strcat(syncaddr, "_s");

  if (type == ZMQ_REP) {
    rc = zsocket_bind(sock_sync, syncaddr);
    if (rc == 0) { 
      while (i < (SNetDistribZMQHostsCount() - 1)) {
        char *str = zstr_recv(sock_sync);
        free(str);
        zstr_send(sock_sync, "");
        i++;
      }
    }
  } else if (type == ZMQ_REQ) {
    rc = zsocket_connect(sock_sync, syncaddr); 
    if (rc == 0) {
      zstr_send(sock_sync, "");
      char *str = zstr_recv(sock_sync);
      free(str);
    }
  }

  zsocket_destroy(context, sock_sync);
  return rc;
}

void SNetDistribZMQHostsInit(int argc, char **argv)
{
  int i, rc;
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

  sock_in = zsocket_new(context, ZMQ_PULL);
  if (!sock_in) SNetUtilDebugFatal("ZMQDistrib: %s", strerror(errno));

  sock_out = SNetMemAlloc(SNetDistribZMQHostsCount() * sizeof(void *));
  for (i = 0 ; i < SNetDistribZMQHostsCount(); i++) {
    sock_out[i] = zsocket_new(context, ZMQ_PUSH);
    if (!sock_out[i]) SNetUtilDebugFatal("ZMQDistrib: %s", strerror(errno));
  }

  rc = zsocket_bind(sock_in, SNetDistribZMQHostsLookup(node_location));
  if (rc != 0) SNetUtilDebugFatal("ZMQDistrib: Socket bind failed %d", rc);
   
  //wait for all nodes to be up and running
  if (node_location == 0) {
    rc = SNetDistribZMQSync(ZMQ_REP);
  } else {
    rc = SNetDistribZMQSync(ZMQ_REQ);
  }

  for (i = 0 ; i < SNetDistribZMQHostsCount(); i++) {
      rc = zsocket_connect(sock_out[i], SNetDistribZMQHostsLookup(i)); 
    if (rc != 0) {
      SNetUtilDebugFatal("ZMQDistrib: Cannot reach node %d (%s): zsocket_connect (%d)",
        i, SNetDistribZMQHostsLookup(i), rc);
    }
  }
}

void SNetDistribImplementationInit(int argc, char **argv, snet_info_t *info)
{
  context = zctx_new();
  if (!context) SNetUtilDebugFatal("ZMQDistrib: %s", strerror(errno));
  zctx_set_linger(context, 1000); //wait 1s before destroying sockets

  SNetDistribZMQHostsInit(argc, argv);

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
  
  /*
   * FIXME: There's a synchronization problem due to the ZMQ asynchronous
   * (non-blocking)send behaviour. It can happen that a record references a
   * network that doesn't yet exist in the local environment because the
   * message that contain that record gets delivered before the network
   * instantiation.
   *
   * The delay here introduced mitigates the issue but does NOT solve it!
   * Therefore, a more sound and controllable communication protocol is needed.
   * 
   * Moving the connection step outside seems to have a positive impact on the
   * problem.
   */
  //zclock_sleep(1);

  rc = zmsg_send(&msg, sock_out[destination]);
  if (rc != 0) {
    SNetUtilDebugFatal("ZMQDistrib: Cannot send message to  %d (%s): zmsg_send (%d)",
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

  msg = zmsg_recv(sock_in); //FIXME: do some proper error checking?
  
  source_f = zmsg_pop(msg);
  type_f = zmsg_pop(msg);
  payload_f = zmsg_pop(msg);

  //printf("<- Recv\n");
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
  zframe_t *payload = NULL;
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
  zframe_t *payload = NULL;
  PackByte(&payload, 1, 0); //FIXME: do we need empty payload messages?
  SNetDistribZMQSend(payload, snet_update, node_location);
}

void SNetDistribSendData(snet_ref_t *ref, void *data, void *dest)
{
  zframe_t *payload = NULL;
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

