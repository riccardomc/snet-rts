#ifndef DISTRIBZMQCLOUD_H
#define DISTRIBZMQCLOUD_H

#include "host.h"

#define SNET_CLOUD_RUN_V "$SNET_DIR/bin/snet-on-cloud" //path to cloud management script
#define SNET_CLOUD_INSTLN 1024 //Length of the instances list
#define SNET_CLOUD_STRSLN 30 //Length of the string for state and id
#define SNET_CLOUD_CMDOLN 1024 //Length of the command string
#define SNET_CLOUD_STATES 8 //Number of known states


typedef enum {
  inst_pending,
  inst_running,
  inst_shutting_down,
  inst_terminated,
  inst_stopping,
  inst_stopped,

  inst_unkown,
  inst_null
} snet_inst_state;

static const char inst_state_str[SNET_CLOUD_STATES][SNET_CLOUD_STRSLN] = {"pending",
  "running",
  "shutting-down",
  "terminated",
  "stopping",
  "stopped",
  "unkown",
  "null"};

typedef struct {
  char cloud_id[SNET_CLOUD_STRSLN];
  char public_dns[SNET_ZMQ_HOSTLN];
  snet_inst_state state;
} snet_instance_t;


char *SNetCloudPopen(char *cmd);

void SNetCloudInit();
int SNetCloudInstantiate(int index);
snet_instance_t *SNetCloudInstanceGet(int index);
int SNetCloudStrtoInstance(char *csv, snet_instance_t *instance);
int SNetCloudTerminate(int index);
int SNetCloudCopy(int index, char *exe);
int SNetCloudRun(int index, char *exe, char *args);
int SNetCloudSpawn(int index, char *exe, char *raddr);
snet_inst_state SNetCloudState(int index, bool update);
int SNetCloudPollState(int index, snet_inst_state state);

void SNetCloudDump();

int SNetCloudId(char *cloud_id);
char *SNetCloudHostToId(char *host);

int SNetCloudInstantiateNetRaw(int net_size, char *exe, char *raddr);
int SNetCloudInstantiateRaw(int index, char *exe, char *raddr);

#endif /* DISTRIBZMQCLOUD_H */
