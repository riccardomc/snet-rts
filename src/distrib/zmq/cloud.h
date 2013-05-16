#ifndef DISTRIBZMQCLOUD_H
#define DISTRIBZMQCLOUD_H

#define SNET_CLOUD_RUN_V "$SNET_DIR/bin/snet-on-cloud" //Location of the script for static instantiation
#define SNET_CLOUD_INSTLN 1024 //Length of the instances list
#define SNET_CLOUD_STRSLN 30 //Length of the string for state and id
#define SNET_CLOUD_CMDOLN 1024 //Length of the command string
#define SNET_CLOUD_STATES 9 //Number of known states


typedef enum {
  inst_pending,
  inst_running,
  inst_shutting_down,
  inst_terminated,
  inst_stopping,
  inst_stopped,

  inst_spawning,
  inst_unkown,
  inst_null
} snet_inst_state;

static const char inst_state_str[SNET_CLOUD_STATES][SNET_CLOUD_STRSLN] = {"pending",
  "running",
  "shutting-down",
  "terminated",
  "stopping",
  "stopped",
  "spawning",
  "unkown",
  "null"};

typedef struct {
  char cloud_id[SNET_CLOUD_STRSLN];
  char public_dns[SNET_ZMQ_HOSTLN];
  snet_inst_state state;
} snet_instance_t;


typedef struct {
  char exe[SNET_CLOUD_CMDOLN];
  char raddr[SNET_ZMQ_ADDRLN];
} cloud_opts_t;

char *SNetCloudPopen(char *cmd);

void SNetCloudInit(char *exe, char *raddr);
int SNetCloudInstantiate(int index);
snet_instance_t *SNetCloudInstanceGet(int index);
int SNetCloudStrtoInstance(char *csv, snet_instance_t *instance);
int SNetCloudTerminate(int index);
int SNetCloudCopy(int index);
int SNetCloudRun(int index);
int SNetCloudSpawn(int index);
snet_inst_state SNetCloudState(int index, bool update);
int SNetCloudPollState(int index, snet_inst_state state);

void SNetCloudDump();

int SNetCloudId(char *cloud_id);
char *SNetCloudHostToId(char *host);

int SNetCloudInstantiateNetRaw(int net_size);
int SNetCloudInstantiateRaw(int index);

#endif /* DISTRIBZMQCLOUD_H */
