#include <mpi.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "distribution.h"
#include "distribcommon.h"

#include "mpi_buf.h"

int node_location;

void SNetDistribImplementationInit(int argc, char **argv, snet_info_t *info)
{
  int level;

  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &level);
  if (level < MPI_THREAD_MULTIPLE) {
    fprintf(stderr, "Required threading level not supported!\nDesired level: "
            "%d\nAvailable: %d\n", MPI_THREAD_MULTIPLE, level);
    MPI_Abort(MPI_COMM_WORLD, 2);
  }
  MPI_Comm_rank(MPI_COMM_WORLD, &node_location);

  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "-debugWait") == 0) {
      volatile int stop = 0;
      printf("PID %d (rank #%d) ready for attach\n", getpid(), node_location);
      fflush(stdout);
      while (0 == stop) sleep(5);
      break;
    }
  }
}

void SNetDistribGlobalStop(void)
{
  int i;

  MPI_Comm_size(MPI_COMM_WORLD, &i);
  for (i--; i >= 0; i--) {
    MPI_Send(&i, 1, MPI_INT, i, snet_stop, MPI_COMM_WORLD);
  }
}

void SNetDistribLocalStop(void) { MPI_Finalize(); }

int SNetDistribGetNodeId(void) { return node_location; }

bool SNetDistribIsNodeLocation(int loc) { return node_location == loc; }

bool SNetDistribIsRootNode(void) { return node_location == 0; }

void SNetDistribPackOld(void *src, ...)
{
  va_list args;
  mpi_buf_t *buf;
  size_t count;

  va_start(args, src);
  buf = va_arg(args, mpi_buf_t *);
  count = va_arg(args, size_t);
  va_end(args);

  SNetDistribPack(buf, src, count);
}

void SNetDistribUnpackOld(void *dst, ...)
{
  va_list args;
  mpi_buf_t *buf;
  int count;

  va_start(args, dst);
  buf = va_arg(args, mpi_buf_t *);
  count = va_arg(args, size_t);
  va_end(args);

  SNetDistribUnpack(buf, dst, count);
}

void SNetDistribPack(void *arg, void *src, size_t size)
{
  mpi_buf_t *buf = arg;
  if (size > buf->size - buf->offset) {
    char *newBuf = SNetMemAlloc(buf->offset + size);
    memcpy(newBuf, buf->data, buf->offset);
    SNetMemFree(buf->data);
    buf->data = newBuf;
    buf->size = buf->offset + size;
  }

  MPI_Pack(src, (int) size, MPI_BYTE, buf->data, buf->size, &buf->offset, MPI_COMM_WORLD);
}

void SNetDistribUnpack(void *arg, void *dst, size_t size)
{
  mpi_buf_t *buf = arg;
  MPI_Unpack(buf->data, buf->size, &buf->offset, dst, size, MPI_BYTE, MPI_COMM_WORLD);
}
