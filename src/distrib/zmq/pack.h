/**
 * Riccardo M. Cefala                     Thu Jan 10 16:52:26 CET 2013
 *
 * Serialization/Deserialization stuff for zmq distrib layer.
 *
 */

#include <zmq.h>
#include <czmq.h>
#include "reference.h"
#include "record.h"

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

inline static void PackByte(void *buf, int count, char *src) {
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

