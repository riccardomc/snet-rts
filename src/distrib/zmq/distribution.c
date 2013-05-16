/**
 * Riccardo M. Cefala                      Sun Jan  6 18:31:18 CET 2013
 *
 * ZMQ distribution layer implementation.
 *
 */

#include <zmq.h>
#include <czmq.h>

#include "debug.h"
#include "distribcommon.h"
#include "memfun.h"
#include "bool.h"
#include "host.h"
#include "htab.h"
#include "cloud.h"

#define SNET_ZMQ_DPORT 20737
#define SNET_ZMQ_SPORT 20738

static int node_location;
static int sync_port;
static int data_port;
static int net_size;
static bool on_cloud;
static char root_addr[SNET_ZMQ_ADDRLN];
static char host_name[SNET_ZMQ_HOSTLN];

void SNetDistribZMQHostsInit(int argc, char **argv)
{
  int i;
  node_location = -1;
  data_port = SNET_ZMQ_DPORT;
  sync_port = SNET_ZMQ_SPORT;
  net_size = 1;
  on_cloud = false;


  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-raddr") == 0) {
      strncpy(root_addr, argv[i+1], SNET_ZMQ_HOSTLN);
    } else if (strcmp(argv[i], "-hostn") == 0) {
      strncpy(host_name, argv[i+1], SNET_ZMQ_HOSTLN);
    } else if (strcmp(argv[i], "-sport") == 0) {
      sync_port = atoi(argv[i+1]);
    } else if (strcmp(argv[i], "-dport") == 0) {
      data_port = atoi(argv[i+1]);
    } else if (strcmp(argv[i], "-node") == 0) {
      node_location = atoi(argv[i+1]);
      if (node_location == 0) {
        SNetUtilDebugFatal("ZMQDistrib: -node argument cannot be 0");
      }
    } else if (strcmp(argv[i], "-root") == 0) {
      net_size = atoi(argv[i+1]);
      node_location = 0;
    } else if (strcmp(argv[i], "-cloud") == 0) {
      on_cloud = true;
    }
  }

  if ((strcmp(root_addr, "") == 0) && (node_location != 0)) {
    SNetUtilDebugFatal("ZMQDistrib: <[-root <net_size>] | [-raddr <root_addr]>");
  }

  SNetDistribZMQHTabInit(data_port, sync_port, node_location, root_addr, host_name);
  node_location = HTabNodeLocation();

  if (on_cloud && node_location == 0) {
    sprintf(root_addr, "tcp://%s:%d/", root_addr, sync_port);
    SNetCloudInstantiateNetRaw(net_size - 1, argv[0], root_addr);
  }

  SNetDistribZMQHTabStart();

}

void SNetDistribImplementationInit(int argc, char **argv, snet_info_t *info)
{
  SNetDistribZMQHostsInit(argc, argv);
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

void SNetDistribPack(void *buf, void *src, size_t size)
{
  SNetDistribZMQPack((zframe_t **)buf, src, size);
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

void SNetDistribUnpack(void *buf, void *dst, size_t size)
{
  SNetDistribZMQUnpack((zframe_t **)buf, dst, size);
}

void SNetDistribZMQSend(zframe_t *payload, int type, int destination)
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

zmsg_t *SNetDistribZMQRecv()
{
  return HTabRecv();
}

void SNetDistribLocalStop(void)
{
  SNetDistribZMQHTabStop();
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

