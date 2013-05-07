/**
 * Riccardo M. Cefala                       Tue Jan  8 17:09:08 CET 2013
 *
 * Naive host table for ZMQ distribution layer.
 *
 */

#ifndef DISTRIBZMQHTAB_H
#define DISTRIBZMQHTAB_H

#include <zmq.h>
#include <czmq.h>

#include "host.h"

#define SNET_ZMQ_ADDRLN 300 //zmq tcp address length
#define SNET_ZMQ_HTABLN 1024 //max host table size

typedef struct {
  zctx_t *ctx;
  int sport;
  int dport;
  int node_location;
  char raddr[SNET_ZMQ_ADDRLN];
} htab_opts_t;


typedef enum {
  htab_init,

  htab_host,
  htab_id,
  htab_part,

  htab_lookup,
  htab_fail
} htab_msg;

/* host table */
int HTabAdd(snet_host_t *h);
void HTabSet(snet_host_t *h, int index);
int HTabRemove(int index);
snet_host_t *HTabLookUp(int index);
void HTabFree();
void HTabDump();
char *HTabGetHostName();

/* threading part */
void SNetDistribZMQHTabInit(int dport, int sport, int node_location,
    char *raddr, char *hname);
void SNetDistribZMQHTabStart(void);
void SNetDistribZMQHTabStop(void);
void SNetDistribZMQHTab(void *args);
snet_host_t *SNetDistribZMQHTabLookUp(int index);
int SNetDistribZMQHTabCount();

/* interface */
snet_host_t *HTabRemoteLookUp(int index);
int HTabSend(zmsg_t *msg, int destination);
zmsg_t *HTabRecv();
int HTabNodeLocation(void);

#endif /* DISTRIBZMQHTAB_H */

