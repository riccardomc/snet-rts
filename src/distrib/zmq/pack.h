/**
 * Riccardo M. Cefala                     Thu Jan 10 16:52:26 CET 2013
 *
 * Serialization functions for ZMQ distribution layer.
 *
 */

#include <zmq.h>
#include <czmq.h>
#include "reference.h"
#include "record.h"
#include "memfun.h"

inline static void ZMQPack(zframe_t **dstframe, int count, void *src)
{
  if (*dstframe != NULL) {
    size_t src_size = count;
    size_t dstframe_size = zframe_size(*dstframe);
    void *dstframe_data = zframe_data(*dstframe);
    
    void *newsrc = SNetMemAlloc(src_size + dstframe_size);

    memcpy(newsrc, dstframe_data, dstframe_size);
    memcpy(newsrc + dstframe_size, src, src_size);

    zframe_reset(*dstframe, newsrc, dstframe_size + src_size);

    SNetMemFree(newsrc);

  } else {
    *dstframe = zframe_new(src, count);
  }

}

inline static void ZMQUnpack(zframe_t **srcframe, int count, void *dst)
{
  if (*srcframe != NULL) {
    size_t dst_size = count;
    size_t srcframe_size = zframe_size(*srcframe);
    void *srcframe_data = zframe_data(*srcframe);

    if(dst_size > srcframe_size) {
      dst = NULL;
    } else {
      memcpy(dst, srcframe_data, dst_size);
      
      //FIXME: SNetMemAlloc returns null when arg is 0!
      void *newdst = malloc(srcframe_size - dst_size);
      
      memcpy(newdst, srcframe_data + count, srcframe_size - dst_size);
      zframe_reset(*srcframe, newdst, srcframe_size - dst_size);

      SNetMemFree(newdst);
    }

  } else {
    dst = NULL;
  }
}

inline static void PackInt(void *buf, int count, int *src) 
{
  ZMQPack((zframe_t **)buf, count * sizeof(int), src);
}

inline static void UnpackInt(void *buf, int count, int *dst) 
{
  ZMQUnpack((zframe_t **)buf, count * sizeof(int), dst);
}

inline static void PackByte(void *buf, int count, char *src) 
{
  ZMQPack((zframe_t **)buf, count, src);
}

inline static void UnpackByte(void *buf, int count, char *dst) 
{
  ZMQUnpack((zframe_t **)buf, count, dst);
}

inline static void PackRef(void *buf, int count, snet_ref_t **src)
{
  for (int i = 0; i < count; i++) {
    SNetRefSerialise(src[i], buf);
    SNetRefOutgoing(src[i]);
  }
}

inline static void UnpackRef(void *buf, int count, snet_ref_t **dst)
{
  for (int i = 0; i < count; i++) {
    dst[i] = SNetRefDeserialise(buf);
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

