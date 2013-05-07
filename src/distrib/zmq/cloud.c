#include <stdio.h>
#include "cloud.h"
#include "sysutils.h"


void SNetCloudInstantiateStatic(char *exe, int net_size, char *raddr)
{
  /*
   * FIXME: This is a proof of concept.
   * We need a more sophisticated instantiation process.
   */
  int i;

  char run[1024] = SNET_CLOUD_RUN_V;
  char cmd[1024];

  SNetUtilSysEnvStr("SNET_CLOUD_RUN", run, 1024);
  sprintf(cmd, "%s %s -- -raddr %s &", run, exe, raddr);

  for (i = 0 ; i < net_size ; i++) {
    system(cmd);
  }

}
