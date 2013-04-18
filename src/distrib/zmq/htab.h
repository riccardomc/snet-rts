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

#define HTAB_HNAME_LEN 256
#define HTAB_MAX_HOSTS 1024

typedef struct {
  zctx_t *ctx;
  int sport;
  int dport;
  int node_location;
  char raddr[300];
} htab_opts_t;

typedef struct {
  int data_port;
  int sync_port;
  char *host;
  char *bind;
} htab_host_t;

typedef enum {
  htab_init,

  htab_host,
  htab_id,

  htab_lookup,
  htab_fail
} htab_msg;


/* host items */
htab_host_t *HTabHostAlloc();
htab_host_t *HTabHostCreate(char *host, char *bind,
    int data_port, int sync_port);
void HTabHostFree(htab_host_t *host);
bool HTabHostCompare(htab_host_t *h1, htab_host_t *h2);

void HTabHostPack(htab_host_t *host, void *buf);
htab_host_t *HTabHostUnpack(void *buf);

/* host table */
int HTabAdd(htab_host_t *h);
void HTabSet(htab_host_t *h, int index);
int HTabRemove(int index);
htab_host_t *HTabGet(int index);
void HTabFree();
void HTabDump();

/* threading part */
void SNetDistribZMQHTabInit(int dport, int sport,
    int node_location, char *raddr);
void SNetDistribZMQHTabStart(void);
void SNetDistribZMQHTabStop(void);
void SNetDistribZMQHTab(void *args);
htab_host_t *SNetDistribZMQHTabLookUp(int index);
int SNetDistribZMQHTabCount();

/* interface */
htab_host_t *HTabRLookUp(int index);
int HTabSend(zmsg_t *msg, int destination);
zmsg_t *HTabRecv();
int HTabNodeLocation(void);

/* utils */
char *htab_gethostname(void);
#endif

