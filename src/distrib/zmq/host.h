#ifndef DISTRIBZMQHOST_H
#define DISTRIBZMQHOST_H

#define SNET_ZMQ_HOSTLN 256 //hostname length

typedef struct {
  int data_port;
  int sync_port;
  char *host;
  char *bind;
} snet_host_t;

snet_host_t *SNetHostAlloc();
snet_host_t *SNetHostCreate(char *host, char *bind,
    int data_port, int sync_port);
void SNetHostFree(snet_host_t *host);
bool SNetHostCompare(snet_host_t *h1, snet_host_t *h2);
void SNetHostPack(snet_host_t *host, void *buf);
snet_host_t *SNetHostUnpack(void *buf);
void SNetHostDump(snet_host_t *h);

#endif /* DISTRIBZMQHOST_H */
