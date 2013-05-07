#ifndef DISTRIBZMQSYSUT_H
#define DISTRIBZMQSYSUT_H

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include "host.h"

/*
* FIXME: consider moving this header to src/util/...
*/

static char *SNetUtilSysHostname()
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

/*
* Check if envname environment variable is set.
*/
static bool SNetUtilSysEnvInt(char *envname, int *parm)
{
  bool ret = false;
  char *envval = getenv(envname);
  if (envval != NULL) {
    *parm = atoi(envval);
    ret = true;
  }
  return ret;
}

static bool SNetUtilSysEnvStr(char *envname, char *parm, size_t size)
{
  bool ret = false;
  char *envval = getenv(envname);
  if (envval != NULL) {
    strncpy(parm, envval, size);
    ret = true;
  }
  return ret;
}

#endif /* DISTRIBZMQSYSUT_T */
