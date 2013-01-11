/**
 * Riccardo M. Cefala                     Thu Jan 10 16:52:26 CET 2013
 *
 * Serialization/Deserialization stuff for zmq distrib layer.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <zmq.h>
#include <czmq.h>

/**
 * packs an integer array into a zframe_t
 *
 * If buf is NULL, a new frame is created, otherwise data is appended.
 * It relies on ZMQ de/allocation.
 *
 * FIXME: should we enforce a maximum buffer_size?
 */
inline static void PackInt(void *buf, int count, int *src) {
  zframe_t **dstframe = (zframe_t **)buf;

  if (*dstframe != NULL) {
    size_t src_size = sizeof(int) * count;
    size_t dstframe_size = zframe_size(*dstframe);
    int *dstframe_data = (int *)zframe_data(*dstframe);
    
    int *newsrc = (int *)malloc(src_size + dstframe_size);

    memcpy(newsrc, dstframe_data, dstframe_size);
    memcpy((newsrc + (dstframe_size / sizeof(int))), src, src_size);

    zframe_reset(*dstframe, newsrc, dstframe_size + src_size);

    free(newsrc);
    zframe_print(*dstframe, "ADD PACK: ");

  } else {
    *dstframe = zframe_new(src, sizeof(int) * count);
    zframe_print(*dstframe, "NEW PACK: ");
  }
}

/**
 * unpacks an integer array from a zframe_t (buf) into dst
 *
 * FIXME: again, should I care about dst size?
 */
inline static void UnpackInt(void *buf, int count, int *dst) {
  byte *src = zframe_data(buf);
  memcpy((byte *)dst, src, count * sizeof(int));
}

#ifdef PACKTEST
int main(int argc, char **argv) {
  zctx_t *ctx = zctx_new ();
  assert (ctx);

  void *output = zsocket_new (ctx, ZMQ_PAIR);
  assert (output);
  zsocket_bind (output, "inproc://zframe.test");
  void *input = zsocket_new (ctx, ZMQ_PAIR);
  assert (input);
  zsocket_connect (input, "inproc://zframe.test");

  zframe_t *srcframe = NULL;
  size_t srcsize = 3;
  size_t srcframe_size, i, j = 0;

  int srcdata[2][3] = {{0, 1, 1}, {2, 3, 5}};
  int *dstdata;

  printf("Packing... ");
  for (i = 0; i < srcsize; i++)
    printf(" %d,", srcdata[j][i]);
  printf("\n");

  PackInt(&srcframe, srcsize, srcdata[j]);
  zframe_print(srcframe, "MAIN: ");

  srcframe_size = zframe_size(srcframe) / sizeof(int);
  dstdata = malloc(sizeof(int) * srcframe_size);
  UnpackInt(srcframe, srcframe_size, dstdata);
  printf("Unpacking... ");
  for (i = 0; i < srcframe_size ; i++)
    printf(" %d,", dstdata[i]);
  printf("\n");
  free(dstdata);

  j++;

  printf("Packing... ");
  for (i = 0; i < srcsize; i++)
    printf(" %d,", srcdata[j][i]);
  printf("\n");

  PackInt(&srcframe, srcsize, srcdata[j]);
  zframe_print(srcframe, "MAIN: ");

  srcframe_size = zframe_size(srcframe) / sizeof(int);
  dstdata = malloc(sizeof(int) * srcframe_size);
  UnpackInt(srcframe, srcframe_size, dstdata);
  printf("Unpacking... ");
  for (i = 0; i < srcframe_size; i++)
    printf(" %d,", dstdata[i]);
  printf("\n");
  free(dstdata);

  exit(EXIT_SUCCESS);
}
#endif
