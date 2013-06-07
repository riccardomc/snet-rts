#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include "bool.h"
#include "host.h"
#include "sysutils.h"

char *SNetUtilSysHostname()
{
  //FIXME: consider using getaddrinfo
  char *hostname = malloc(sizeof(char) * SNET_ZMQ_HOSTLN);
  struct hostent* host;

  hostname[SNET_ZMQ_HOSTLN - 1] = '\0';
  gethostname(hostname, SNET_ZMQ_HOSTLN - 1);
  host = gethostbyname(hostname);
  strcpy(hostname, host->h_name);

  return hostname;
}

bool SNetUtilSysEnvInt(char *envname, int *parm)
{
  int ret = -1;
  char *envval = getenv(envname);
  if (envval != NULL) {
    ret = sscanf(envval, "%d", parm);
  }
  return ret > 0;
}

bool SNetUtilSysEnvStr(char *envname, char *parm, size_t size)
{
  bool ret = false;
  char *envval = getenv(envname);
  if (envval != NULL) {
    strncpy(parm, envval, size);
    ret = true;
  }
  return ret;
}

