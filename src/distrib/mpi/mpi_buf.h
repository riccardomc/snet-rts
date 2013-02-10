#ifndef MPI_BUF_H
#define MPU_BUF_H

typedef struct {
  int offset;
  size_t size;
  char *data;
} mpi_buf_t;

#endif
