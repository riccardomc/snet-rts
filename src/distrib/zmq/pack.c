#include <zmq.h>
#include <czmq.h>
#include "reference.h"
#include "record.h"
#include "pack.h"

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

  } else {
    *dstframe = zframe_new(src, sizeof(int) * count);
  }
}

inline static void UnpackInt(void *buf, int count, int *dst) {
  byte *src = zframe_data(buf);
  memcpy((byte *)dst, src, count * sizeof(int));
}

inl ine static void PackByte(void *buf, int count, char *src) {
  zframe_t **dstframe = (zframe_t **)buf;

  if (*dstframe != NULL) {
    size_t src_size = count;
    size_t dstframe_size = zframe_size(*dstframe);
    byte *dstframe_data = zframe_data(*dstframe);
    
    byte *newsrc = (byte *)malloc(src_size + dstframe_size);

    memcpy(newsrc, dstframe_data, dstframe_size);
    memcpy(newsrc + dstframe_size, src, src_size);

    zframe_reset(*dstframe, newsrc, dstframe_size + src_size);

    free(newsrc);

  } else {
    *dstframe = zframe_new(src, count);
  }
}

inline static void UnpackByte(void *buf, int count, char *dst) {
  byte *src = zframe_data(buf);
  memcpy(dst, src, count);
}

inline static void PackRef(void *buf, int count, snet_ref_t **src)
{
  for (int i = 0; i < count; i++) {
    SNetRefSerialise(src[i], buf, &PackInt, &PackByte);
    SNetRefOutgoing(src[i]);
  }
}

inline static void UnpackRef(void *buf, int count, snet_ref_t **dst)
{
  for (int i = 0; i < count; i++) {
    dst[i] = SNetRefDeserialise(buf, &UnpackInt, &UnpackByte);
    SNetRefIncoming(dst[i]);
  }
}

inline static void PackDest(void *buf, snet_dest_t *dest)
{
  PackInt(buf, 1, &dest->dest);
  PackInt(buf, 1, &dest->parent);
  PackInt(buf, 1, &dest->dynamicIndex);
  PackInt(buf, 1, &dest->parentNode);
  PackInt(buf, 1, &dest->dynamicLoc);
}

inline static void UnpackDest(void *buf, snet_dest_t *dest)
{
  UnpackInt(buf, 1, &dest->dest);
  UnpackInt(buf, 1, &dest->parent);
  UnpackInt(buf, 1, &dest->dynamicIndex);
  UnpackInt(buf, 1, &dest->parentNode);
  UnpackInt(buf, 1, &dest->dynamicLoc);
}


#ifdef PACKTEST

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  zframe_t *srcframe = NULL;
  zframe_t *dstframe = NULL;
  size_t srcsize = 3;
  size_t srcframe_size, i = 0, j = 0;

  char srcdata[2][3] = {{0, 1, 1}, {2, 3, 5}};
  char *dstdata;

  zctx_t *ctx = zctx_new ();
  assert (ctx);

  void *output = zsocket_new (ctx, ZMQ_PAIR);
  assert (output);
  zsocket_bind (output, "inproc://zframe.test");

  void *input = zsocket_new (ctx, ZMQ_PAIR);
  assert (input);
  zsocket_connect (input, "inproc://zframe.test");

  PackByte(&srcframe, srcsize, srcdata[j]);
  zframe_print(srcframe, "SEND: ");
  zframe_send(&srcframe, output, ZFRAME_REUSE);

  dstframe = zframe_recv(input);
  zframe_print(dstframe, "RECV: ");

  /*
  srcframe_size = zframe_size(srcframe);
  dstdata = malloc(srcframe_size);
  UnpackByte(srcframe, srcframe_size, dstdata);
  */

  zframe_destroy(&dstframe);

  j++;

  PackByte(&srcframe, srcsize, srcdata[j]);
  zframe_print(srcframe, "SEND: ");
  zframe_send(&srcframe, output, 0);

  dstframe = zframe_recv(input);
  zframe_print(dstframe, "RECV: ");

  /*
  srcframe_size = zframe_size(srcframe);
  dstdata = malloc(srcframe_size);
  UnpackByte(srcframe, srcframe_size, dstdata);
  */

  zframe_destroy(&dstframe);
  zframe_destroy(&srcframe);
  zctx_destroy(&ctx);

  exit(EXIT_SUCCESS);
}
#endif
