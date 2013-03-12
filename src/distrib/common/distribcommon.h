#ifndef DISTRIBCOMMON_H
#define DISTRIBCOMMON_H

#include "memfun.h"
#include "threading.h"
#include "list.h"
#include "ast.h"

typedef enum {
  snet_rec,
  snet_update,
  snet_block,
  snet_unblock,
  snet_ref_set,
  snet_ref_fetch,
  snet_ref_update,
  snet_stop
} snet_comm_type_t;

typedef struct  {
  int node;
  int dest;
  int dynamicParent;
  int dynamicNode;
  int dynamicLoc;
  snet_id_t *id;
} snet_dest_t;

typedef struct {
  snet_dest_t *dest;
  snet_stream_t *stream;
} snet_tuple_t;

#include "record.h"

typedef struct {
  snet_comm_type_t type;
  snet_record_t *rec;
  snet_dest_t *dest;
  snet_ref_t *ref;
  uintptr_t data;
  int val;
} snet_msg_t;

inline static void SNetDestDump(snet_dest_t *dest, char *prefix)
{
  printf("%s: %d %d %d %d %d ", prefix, dest->node,
      dest->dest, dest->dynamicParent, dest->dynamicNode, dest->dynamicLoc);
  SNetIdDumpAux(dest->id);
  printf("\n");
}

inline static bool SNetDestCompare(snet_dest_t *d1, snet_dest_t *d2)
{

  bool res = d1->node == d2->node &&
    d1->dest == d2->dest &&
    d1->dynamicParent == d2->dynamicParent &&
    d1->dynamicNode == d2->dynamicNode &&
    d1->dynamicLoc == d2->dynamicLoc &&
    SNetIdCompare(d1->id, d2->id);

  return res;
}

inline static snet_dest_t *SNetDestCopy(snet_dest_t *d)
{
  snet_dest_t *result = SNetMemAlloc(sizeof(snet_dest_t));
  result->node = d->node;
  result->dest = d->dest;
  result->dynamicParent = d->dynamicParent;
  result->dynamicNode = d->dynamicNode;
  result->dynamicLoc = d->dynamicLoc;
  result->id = SNetIntListCopy(d->id);
  return result;
}

inline static void SNetDestSerialise(snet_dest_t *dest, void *buf)
{
  SNetDistribPack(buf, dest, sizeof(snet_dest_t));
  SNetIntListSerialise(dest->id, buf);
}

inline static snet_dest_t *SNetDestDeserialise(void *buf)
{
  snet_dest_t *dest = SNetMemAlloc(sizeof(snet_dest_t));
  SNetDistribUnpack(buf, dest, sizeof(snet_dest_t));
  dest->id = SNetMemAlloc(sizeof(snet_id_t));
  SNetIntListDeserialise(dest->id, buf);
  return dest;
}

inline static snet_dest_t *SNetDestCreate()
{
  snet_dest_t *dest = SNetMemAlloc(sizeof(snet_dest_t));

  dest->node = 0;
  dest->dest = 0;
  dest->dynamicParent = 0;
  dest->dynamicNode = 0;
  dest->dynamicLoc = 0;
  dest->id = SNetIntListCreate(0);
  SNetIntListAppendStart(dest->id, 0);

  return dest;
}

inline static void SNetDestDestroy(snet_dest_t *dest)
{
  SNetIntListDestroy(dest->id);
  SNetMemFree(dest);
  dest = NULL;
}

inline static bool SNetTupleCompare(snet_tuple_t t1, snet_tuple_t t2)
{
  (void) t1; /* NOT USED */
  (void) t2; /* NOT USED */
  return false;
}

/* To be implemented by distrib/implementation */
void SNetDistribImplementationInit(int argc, char **argv, snet_info_t *info);
void SNetDistribLocalStop(void);

snet_msg_t SNetDistribRecvMsg(void);
void SNetDistribSendRecord(snet_dest_t *dest, snet_record_t *rec);

void SNetDistribBlockDest(snet_dest_t *dest);
void SNetDistribUnblockDest(snet_dest_t *dest);

void SNetDistribUpdateBlocked(void);
void SNetDistribSendData(snet_ref_t *ref, void *data, void *dest);
#endif
