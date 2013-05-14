#include <stdio.h>
#include <sys/wait.h>
#include "cloud.h"
#include "sysutils.h"
#include "threading.h"

static snet_instance_t instances[SNET_CLOUD_INSTLN];
static char runner[SNET_CLOUD_CMDOLN] = SNET_CLOUD_RUN_V;

void SNetCloudInstanceInit(snet_instance_t *instance)
{
  instance->cloud_id[SNET_CLOUD_STRSLN - 1] = '\0' ;
  instance->public_dns[SNET_ZMQ_HOSTLN - 1] = '\0' ;
  instance->state = inst_null;
}

void SNetCloudInstanceDump(snet_instance_t *i) {
    printf("%s, %s, %s", i->cloud_id, i->public_dns, inst_state_str[i->state]);
}

snet_instance_t *SNetCloudInstanceGet(int index)
{
  snet_instance_t *instance = NULL;

  if (index >= 0 && index < SNET_CLOUD_INSTLN) {
    instance = &instances[index];
  }

  return instance;
}

void SNetCloudInit()
{
  int i;

  SNetUtilSysEnvStr("SNET_CLOUD_RUN", runner, SNET_CLOUD_CMDOLN);

  for (i = 0; i < SNET_CLOUD_INSTLN; i++) {
    SNetCloudInstanceInit(&instances[i]);
  }
}

void SNetCloudDump()
{
  int i;
  printf("\n");
  for (i = 0; i < 10; i++) {
    printf("%d ",i);
    SNetCloudInstanceDump(&instances[i]);
    printf("\n");
  }
  printf("-\n");
}

void SNetCloudTerminateAll()
{
  int i;

  for (i = 0; i < SNET_CLOUD_INSTLN; i++) {
    if (instances[i].state == inst_null) {
      continue;
    } else {
      SNetCloudTerminate(i);
    }
  }
}

/*
* Convert string instance states to snet_inst_state (see cloud.h)
*/
snet_inst_state SNetCloudEncodeState(char *state)
{
  int i;

  for (i = 0; i < SNET_CLOUD_STATES - 1; i++) {
    if (strcmp(state, inst_state_str[i]) == 0) {
      return i;
    }
  }

  return inst_unkown;
}

/*
* Parse a comma separeted triple of the form cloud_id, public_dns, state
*/
int SNetCloudStrtoInstance(char *csv, snet_instance_t *instance)
{
  char *state = SNetMemAlloc(SNET_CLOUD_STRSLN);

  int ret;

  ret = sscanf(csv, "Instance: %s %s %s", instance->cloud_id, instance->public_dns, state);
  if(ret != 3) {
    return false;
  }

  instance->state = SNetCloudEncodeState(state);
  SNetMemFree(state);
  return true;
}

/*
* Execute cmd, wait for completion, and return output.
* returns NULL on failure.
*/
char *SNetCloudPopen(char *cmd) 
{
  FILE *p;
  char *output;
  int status;

  output = SNetMemAlloc(SNET_CLOUD_CMDOLN);
  output[SNET_CLOUD_CMDOLN -1]= '\0';

  p = popen(cmd, "r");
  if (p == NULL) {
    SNetMemFree(output);
    return NULL;
  }

  fgets(output, SNET_CLOUD_CMDOLN, p);
  status = pclose(p);
  if (WEXITSTATUS(status) != 0) {
    SNetMemFree(output);
    return NULL;
  }

  return output;

}

int SNetCloudUpdateState(int index)
{
  char cmd[SNET_CLOUD_CMDOLN];
  char *output;
  char *state;
  snet_instance_t *i = &instances[index];
  bool ret = false;

  state = SNetMemAlloc(SNET_CLOUD_CMDOLN);

  if (strcmp(i->cloud_id, "") == 0) {
    return ret;
  }

  sprintf(cmd, "%s -i %s -S" , runner, i->cloud_id);

  output = SNetCloudPopen(cmd);
  if (output == NULL) {
    output = "unkown";
  } else {
    ret = true;
  }

  sscanf(output, "%s", state);
  i->state = SNetCloudEncodeState(state);
  SNetMemFree(state);

  return ret;
}

snet_inst_state SNetCloudState(int index, bool update)
{

  if (update) {
    SNetCloudUpdateState(index);
  }

  return instances[index].state;
}

int SNetCloudInstantiate(int index)
{
  char cmd[SNET_CLOUD_CMDOLN];
  char *output;
  bool ret;
  snet_instance_t *i = &instances[index];

  if (i->state == inst_pending 
      || i->state == inst_running) {
    return true;
  }

  sprintf(cmd, "%s -I" , runner);

  output = SNetCloudPopen(cmd);
  if (output == NULL) {
    printf("Instantiation failed\n");
    SNetMemFree(output);
    return false;
  }

  ret = SNetCloudStrtoInstance(output, i);
  SNetMemFree(output);
  return ret;
}

int SNetCloudTerminate(int index)
{
  char cmd[SNET_CLOUD_CMDOLN];
  char *output;
  snet_instance_t *i = &instances[index];

  if (i->state == inst_terminated
      || i->state == inst_shutting_down) {
    return true;
  }

  sprintf(cmd, "%s -i %s -T" , runner, i->cloud_id);

  output = SNetCloudPopen(cmd);
  if (output == NULL) {
    SNetMemFree(output);
    return false;
  }

  i->state = inst_shutting_down;
  return true;
}

int SNetCloudCopy(int index, char *exe)
{
  char cmd[SNET_CLOUD_CMDOLN];
  snet_instance_t *i = &instances[index];

  if (i->state != inst_running) {
    return false;
  }

  sprintf(cmd, "%s -q -i %s -C %s" , runner, i->cloud_id, exe);

  return system(cmd);

}

int SNetCloudRun(int index, char *exe, char *raddr)
{
  char cmd[SNET_CLOUD_CMDOLN];
  snet_instance_t *i = &instances[index];

  if (i->state != inst_running) {
    return false;
  }

  sprintf(cmd, "%s -v -i %s -R %s -- -node %d -raddr %s",
      runner, i->cloud_id, exe, index, raddr);

  return system(cmd);
}

int SNetCloudInstantiateNetRaw(int net_size, char *exe, char *raddr)
{
  int i;

  for (i = 1 ; i <= net_size ; i++) {
    SNetCloudInstantiateRaw(i, exe, raddr);
  }

}

/*
* Instantiate, copy, run and terminate without tracking, in background.
*/
int SNetCloudInstantiateRaw(int index, char *exe, char *raddr)
{
  char cmd[SNET_CLOUD_CMDOLN];

  sprintf(cmd, "%s -v -I -C -R -T %s -- -node %d -raddr %s &",
      runner, exe, index, raddr);

  return system(cmd);
}


