#include <pthread.h>

#include "distribcommon.h"
#include "imanager.h"
#include "info.h"
#include "memfun.h"
#include "omanager.h"
#include "reference.h"
#include "snetentities.h"

static bool running = true;
static snet_info_tag_t prevDest;
static pthread_cond_t exitCond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t exitMutex = PTHREAD_MUTEX_INITIALIZER;

void SNetDistribInit(int argc, char **argv, snet_info_t *info)
{
  snet_dest_t *dest = SNetMemAlloc(sizeof(snet_dest_t));

  dest->node = 0;
  dest->dest = 0;
  dest->id = SNetIntListCreate(0);
  SNetIntListAppendStart(dest->id, 0);

  prevDest = SNetInfoCreateTag();
  SNetInfoSetTag(info, prevDest, (uintptr_t) dest,
                 (void* (*)(void*)) &SNetDestCopy, &SNetMemFree);

  SNetIdInit(info);

  SNetDistribImplementationInit(argc, argv, info);

  SNetReferenceInit();
  SNetOutputManagerInit();
  SNetInputManagerInit();
}

void SNetDistribStart(void)
{
  SNetOutputManagerStart();
  SNetInputManagerStart();
}

void SNetDistribStop(void)
{
  SNetDistribLocalStop();

  pthread_mutex_lock(&exitMutex);
  running = false;
  pthread_cond_signal(&exitCond);
  pthread_mutex_unlock(&exitMutex);
}

void SNetDistribWaitExit(snet_info_t *info)
{
  pthread_mutex_lock(&exitMutex);
  while (running) pthread_cond_wait(&exitCond, &exitMutex);
  pthread_mutex_unlock(&exitMutex);
}

snet_stream_t *SNetRouteUpdate(snet_info_t *info, snet_stream_t *in,
     int location, snet_locvec_t *locvec)
{
  snet_dest_t *dest = (snet_dest_t*) SNetInfoGetTag(info, prevDest);

  if (locvec && (dest->node != location)) {
    dest->dest = locvec->index;

    if (SNetDistribIsNodeLocation(dest->node)) {
      dest->node = location;
      dest->id = SNetIntListCopy(SNetIdGet(info));
      SNetOutputManagerNewOut(*SNetDestCopy(dest), in);

      in = NULL;
    } else if (SNetDistribIsNodeLocation(location)) {
      if (in == NULL) in = SNetStreamCreate(0);

      SNetInputManagerNewIn(*SNetDestCopy(dest), in);
      dest->node = location;
      dest->id = SNetIntListCopy(SNetIdGet(info));
    } else {
      dest->node = location;
      dest->id = SNetIntListCopy(SNetIdGet(info));
    }
  }

  return in;
}

void SNetRouteNewDynamic(snet_dest_t dest)
{
  snet_stream_t *stream = NULL;
  int location = -1;
  snet_ast_t *ast =  SNetASTLookup(dest.dest);
  snet_ast_t *ast_parent = SNetASTLookup(ast->locvec.parent->index);

  if (ast_parent->locvec.type != LOC_SPLIT) {
    location = dest.node;
    ast = ast_parent;
  } else if (ast_parent->split.loc) { //location split
    location = SNetIntListGet(dest.id, 0);
  }

  snet_info_t *info = SNetInfoInit();
  SNetIdInit(info);
  SNetInfoSetTag(info, prevDest, (uintptr_t) SNetDestCopy(&dest),
                (void* (*)(void*)) &SNetDestCopy, &SNetMemFree);
  SNetIdSet(info, SNetIntListCopy(dest.id));

  stream = SNetInstantiatePlacement(ast, NULL, info, location);
  SNetRouteUpdate(info, stream, dest.node, &ast->locvec);
  SNetInfoDestroy(info);
}

