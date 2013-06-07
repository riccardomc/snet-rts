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

#define SNET_ZMQ_ADDRLN 300        //zmq tcp address length
#define SNET_ZMQ_HTABLN 1024       //max host table size

/* default values */
#define SNET_ZMQ_SEND_HIGHWM_D 1000   //send high water mark (number of msgs)
#define SNET_ZMQ_RECV_HIGHWM_D 1000   //recv high water mark (number of msgs)
#define SNET_ZMQ_DATA_TIMEO_D  120000 //data time out (milliseconds)
#define SNET_ZMQ_SYNC_TIMEO_D  120000 //sync time out (milliseconds)
#define SNET_ZMQ_LOOK_TIMEO_D  120000 //lookup timeout (milliseconds)

typedef struct {
  zctx_t *ctx;
  int sync_port;
  int data_port;
  int node_location;
  char *host_name;
  char root_addr[SNET_ZMQ_ADDRLN];
  int on_cloud;
} htab_opts_t;

typedef struct {
  void *request;              // sync request socket
  void *reply;                // sync reply socket
  void *in;                   // data incoming socket
  void *out[SNET_ZMQ_HTABLN]; // data outgoing sockets (one per node)
                              /* socket options */
  int send_highwm;            //ZMQ send high watermark (see ZMQ docs)
  int recv_highwm;            //ZMQ receive high watermark
  int sync_timeo;             //synchronization sockets timeout (milliseconds)
  int data_timeo;             //data sockets timeout (milliseconds)
  int look_timeo;             //host lookup timeout (milliseconds)
} htab_sockets_t;

/* synchronization messages types */
typedef enum {
  htab_host,    //host joins or look up reply
  htab_id,      //reply to joining host or look up request
  htab_part,    //an host parts the network
  htab_fail     //look up reply failure
} htab_msg;

/* send function prototype */
typedef int (*sendfun)(zmsg_t*, int);

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
    char *raddr, char *hname, int on_cloud);
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

