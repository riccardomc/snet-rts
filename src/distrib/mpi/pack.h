#include <mpi.h>
#include <string.h>

#include "memfun.h"

typedef struct {
  int offset;
  size_t size;
  char *data;
} mpi_buf_t;

inline static void MPIPack(mpi_buf_t *buf, void *src,
                           MPI_Datatype type, int count)
{
  int size;
  char *newBuf;

  //calculate buffer size and alloc a bigger buffer if needed.
  MPI_Pack_size(count, type, MPI_COMM_WORLD, &size);
  if ((unsigned) size > buf->size - buf->offset) {
    newBuf = SNetMemAlloc(buf->offset + size);
    memcpy(newBuf, buf->data, buf->offset);
    SNetMemFree(buf->data);
    buf->data = newBuf;
    buf->size = buf->offset + size;
  }
  MPI_Pack(src, count, type, buf->data, buf->size, &buf->offset, MPI_COMM_WORLD);
}

inline static void MPIUnpack(mpi_buf_t *buf, void *dst,
                             MPI_Datatype type, int count)
{
  MPI_Unpack(buf->data, buf->size, &buf->offset, dst, count, type, MPI_COMM_WORLD);
}
