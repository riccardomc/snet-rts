#include <stdio.h>
#include <sys/wait.h>
#include "bool.h"
#include "host.h"
#include "htab.h"
#include "cloud.h"
#include "sysutils.h"
#include "threading.h"

static cloud_opts_t opts;
static snet_instance_t instances[SNET_CLOUD_INSTLN];
static zmutex_t *instances_mtx;

void SNetCloudInstanceInit(snet_instance_t *instance)
{
  instance->cloud_id[SNET_CLOUD_STRSLN - 1] = '\0' ;
  instance->public_dns[SNET_ZMQ_HOSTLN - 1] = '\0' ;
  instance->state = inst_null;
}

void SNetCloudInstanceDump(snet_instance_t *i)
{
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

void SNetCloudInit(char *exe, char *raddr)
{
  int i;

  strncpy(opts.script, SNET_CLOUD_SCRIPT_D, SNET_CLOUD_CMDOLN);
  strncpy(opts.exe, exe, SNET_CLOUD_CMDOLN);
  strncpy(opts.root_addr, raddr, SNET_ZMQ_ADDRLN);

  SNetUtilSysEnvStr("SNET_CLOUD_SCRIPT", opts.script, SNET_CLOUD_CMDOLN);

  instances_mtx = zmutex_new();

  for (i = 0; i < SNET_CLOUD_INSTLN; i++) {
    SNetCloudInstanceInit(&instances[i]);
  }

}

void SNetCloudDestroy()
{
  zmutex_destroy(&instances_mtx);
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
* Parse a string of the form
* Instance: cloud_id public_dns state
*/
int SNetCloudStrtoInstance(char *csv, snet_instance_t *instance)
{
  char *state = SNetMemAlloc(SNET_CLOUD_STRSLN);

  int ret;

  ret = sscanf(csv, "Instance: %s %s %s", instance->cloud_id,
      instance->public_dns, state);
  instance->state = SNetCloudEncodeState(state);

  SNetMemFree(state);
  if(ret != 3) {
    return false;
  }

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

/*
* Retrieve state instance at index for cloud provider
* NON thread safe. Use SNetCloudState(index, true)
*/
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

  sprintf(cmd, "%s -i %s -S" , opts.script, i->cloud_id);

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

/*
* Return state of instance.
*/
snet_inst_state SNetCloudState(int index, bool update)
{
  snet_inst_state state;

  zmutex_lock(instances_mtx);
  if (update) {
    SNetCloudUpdateState(index);
  }

  state = instances[index].state;
  zmutex_unlock(instances_mtx);

  return state;
}

/*
* Instantiate instance at index.
*
* Prevents multiple instantiation requests.
*/
int SNetCloudInstantiate(int index)
{
  char cmd[SNET_CLOUD_CMDOLN];
  char *output;
  bool ret;
  snet_instance_t *i = &instances[index];
  snet_inst_state prev_state;

  zmutex_lock(instances_mtx);
  if (i->state == inst_pending
      || i->state == inst_running) {
    zmutex_unlock(instances_mtx);
    return true;
  } else {
    prev_state = i->state;
    i->state = inst_pending;
  }
  zmutex_unlock(instances_mtx);

  sprintf(cmd, "%s -I" , opts.script);

  output = SNetCloudPopen(cmd);
  if (output == NULL) {
    SNetMemFree(output);
    zmutex_lock(instances_mtx);
    i->state = prev_state;
    zmutex_unlock(instances_mtx);
    return false;
  }

  zmutex_lock(instances_mtx);
  ret = SNetCloudStrtoInstance(output, i);
  if (!ret) {
    i->state = prev_state;
  }
  zmutex_unlock(instances_mtx);
  SNetMemFree(output);
  return ret;
}

/*
* Terminates instance at index.
*
* Prevents multiple termination requests.
*/
int SNetCloudTerminate(int index)
{
  char cmd[SNET_CLOUD_CMDOLN];
  char *output;
  snet_instance_t *i = &instances[index];

  zmutex_lock(instances_mtx);
  if (i->state == inst_shutting_down ||
      i->state == inst_terminated ||
      i->state == inst_null) {
    zmutex_unlock(instances_mtx);
    return true;
  } else {
    i->state = inst_shutting_down;
  }
  zmutex_unlock(instances_mtx);

  sprintf(cmd, "%s -i %s -T" , opts.script, i->cloud_id);

  output = SNetCloudPopen(cmd);
  if (output == NULL) {
    SNetMemFree(output);
    zmutex_lock(instances_mtx);
    //assume the instance is not responding or invalid
    i->state = inst_null;
    zmutex_unlock(instances_mtx);
    return false;
  }

  zmutex_lock(instances_mtx);
  i->state = inst_null;
  zmutex_unlock(instances_mtx);

  return true;
}

/*
* Copy the executable to instance at index.
*
* Instance must be running.
*/
int SNetCloudCopy(int index)
{
  char cmd[SNET_CLOUD_CMDOLN];
  snet_instance_t *i = &instances[index];
  int status;

  if (i->state != inst_running) {
    return false;
  }

  sprintf(cmd, "%s -q -i %s -C %s" , opts.script, i->cloud_id, opts.exe);

  status = system(cmd);
  if (status == -1 || WEXITSTATUS(status) > 0) {
    return false;
  }

  return true;

}

/*
* Run the executable on instance at index.
*
* Instance must be running.
*/
int SNetCloudRun(int index)
{
  char cmd[SNET_CLOUD_CMDOLN];
  snet_instance_t *i = &instances[index];
  int status;

  if (i->state != inst_running) {
    return false;
  }

  sprintf(cmd, "%s -q -i %s -R %s -- -node %d -raddr %s",
      opts.script, i->cloud_id, opts.exe, index, opts.root_addr);

  status = system(cmd);
  if (status == -1 || WEXITSTATUS(status) > 0) {
    return false;
  }

  return true;
}

int SNetCloudInstantiateNetRaw(int net_size)
{
  int i;
  bool success = true;

  for (i = 1 ; i <= net_size ; i++) {
    success = SNetCloudInstantiateRaw(i) && success;
  }

  return success;
}

/*
* Instantiate, copy, run and terminate without tracking, in background.
*/
int SNetCloudInstantiateRaw(int index)
{
  char cmd[SNET_CLOUD_CMDOLN];
  int status;

  sprintf(cmd, "%s -q -I -C -R -T %s -- -node %d -raddr %s &",
      opts.script, opts.exe, index, opts.root_addr);

  status = system(cmd);
  if (status == -1 || WEXITSTATUS(status) > 0) {
    return false;
  }

  return true;

}

void SNetCloudSpawnAux(void *args)
{
  int index = *(int *)args;
  bool success;

  printf("Spawning %d!\n", index);

  success = SNetCloudInstantiate(index);
  if (success) {
    success = SNetCloudCopy(index);
  }
  if (success) {
    success = SNetCloudRun(index);
  }

  SNetCloudTerminate(index);

  if (!success) {
    //FIXME: Do something?
  }

}

int SNetCloudSpawn(int index)
{
  char tn[SNET_CLOUD_STRSLN];
  int *i = SNetMemAlloc(sizeof(int));
  *i = index;

  sprintf(tn, "zmq_cloud_%d", index);

  zmutex_lock(instances_mtx);
  if (instances[index].state == inst_pending ||
      instances[index].state == inst_running ||
      instances[index].state == inst_spawning) {
    zmutex_unlock(instances_mtx);
    return false;
  }
  instances[index].state = inst_spawning;
  zmutex_unlock(instances_mtx);

  SNetThreadingSpawn( ENTITY_other, -1, SNetNameCreate(NULL, NULL,
      tn), &SNetCloudSpawnAux, i);

  return true;
}
