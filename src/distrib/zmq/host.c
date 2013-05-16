#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "bool.h"
#include "host.h"
#include "distribution.h"

snet_host_t *SNetHostAlloc()
{
  int i;
  snet_host_t *host = malloc(sizeof(snet_host_t));
  if (host) {
    host->host = malloc(sizeof(char) * SNET_ZMQ_HOSTLN);
    host->bind = malloc(sizeof(char) * SNET_ZMQ_HOSTLN);
    for (i = 0; i < SNET_ZMQ_HOSTLN; i++) {
      host->host[i] = '\0';
      host->bind[i] = '\0';
    }
    host->data_port = 0;
    host->sync_port = 0;
  }
  return host;
}

snet_host_t *SNetHostCreate(char *host, char *bind,
    int data_port, int sync_port)
{
  snet_host_t *h = SNetHostAlloc();
  strncpy(h->host, host, SNET_ZMQ_HOSTLN);
  strncpy(h->bind, bind, SNET_ZMQ_HOSTLN);
  h->data_port = data_port;
  h->sync_port = sync_port;
  return h;
}

void SNetHostFree(snet_host_t *host)
{
  free(host->host);
  free(host->bind);
  free(host);
  host = NULL;
}

bool SNetHostCompare(snet_host_t *h1, snet_host_t *h2)
{
  return h1->data_port == h2->data_port
    && h1->sync_port == h2->sync_port
    && strcmp(h1->host, h2->host) == 0
    && strcmp(h1->bind, h2->bind) == 0;
}

void SNetHostPack(snet_host_t *host, void *buf)
{
  SNetDistribPack(buf, &host->data_port, sizeof(int));
  SNetDistribPack(buf, &host->sync_port, sizeof(int));
  SNetDistribPack(buf, host->host, SNET_ZMQ_HOSTLN);
  SNetDistribPack(buf, host->bind, SNET_ZMQ_HOSTLN);
}

snet_host_t *SNetHostUnpack(void *buf) 
{
  snet_host_t *host = SNetHostAlloc();
  SNetDistribUnpack(buf, &host->data_port, sizeof(int));
  SNetDistribUnpack(buf, &host->sync_port, sizeof(int));
  SNetDistribUnpack(buf, host->host, SNET_ZMQ_HOSTLN);
  SNetDistribUnpack(buf, host->bind, SNET_ZMQ_HOSTLN);
  return host;
}

void SNetHostDump(snet_host_t *h)
{
  if (h) {
    printf("%d %d %s %s", h->data_port, h->sync_port, h->host, h->bind);
  } else {
    printf(". . . .");
  }
}

