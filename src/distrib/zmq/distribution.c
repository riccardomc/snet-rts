/**
* Riccardo M. Cefala                      Sun Jan  6 18:31:18 CET 2013
*
* ZMQ Distribution implementation attempt.
*
* TODO:
* - we'll need a simple int to addr mapping. DONE
* - packing and unpacking (serialization).
* - how to handle streams? proper protocol? (REQ/RESP or PUSH/PULL)
*
**/

#include <zmq.h>
#include <czmq.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>

zctx_t *context;
void *instream;
void *outstream;
htab_t *hosts
size_t node_location;

void SNetDistribZMQError(int errno)
{
  printf("Error: %s", strerror(errno));
  //Do something to propagate the error?
  exit(EXIT_FAILURE);
}

//These (SnetDistribZMQHosts*) will be eventually abstracted.
char *SNetDistribZMQHostsLookup(size_t addr)
{
  return htab_lookup(host_table, addr);
}

void SNetDistribZMQHostsInit(int argc, char **argv) 
{
  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "-nodesTable") == 0) {
      if ((i + 1) < argc) {
        host_table = htab_fread(argv[i+1]);
        if (!host_table)
          SNetDistribZMQError(errno())
      }

    } else if (strcmp(argv[i], "-nodeId") == 0) {
      node_location = 0;
    }
}

void SNetDistribZMQHostsStop()
{
  htab_free(hosts);
}


void SNetDistribImplementationInit(int argc, char **argv, snet_info_t *info)
{
  context = zctx_new();
  if (!context)
      SNetDistribZMQError(errno());

  SNetDistribZMQHostsInit(argc, argv);

  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "-debugWait") == 0) {
      volatile int stop = 0;
      printf("PID %d ready for attach\n", getpid());
      fflush(stdout);
      while (0 == stop) sleep(5);
      break;
    } 
  }

}


void SNetDistribLocalStop(void)
{ 
  zctx_destroy (&context);
  SNetDistribZMQHostsStop();
}


