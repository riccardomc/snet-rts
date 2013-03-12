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
static snet_dest_t *init_dest;

void SNetDistribInit(int argc, char **argv, snet_info_t *info)
{
  init_dest = SNetDestCreate();

  prevDest = SNetInfoCreateTag();
  SNetInfoSetTag(info, prevDest, (uintptr_t) init_dest,
                 (void* (*)(void*)) &SNetDestCopy,
                 (void (*)(void*)) &SNetDestDestroy);

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
     int location, int index)
{
  snet_dest_t *dest = (snet_dest_t*) SNetInfoGetTag(info, prevDest);
#ifdef SNET_DISTRIB_DEBUG
  SNetDestDump(dest, "RU");
#endif

  if (dest->node != location) {
    dest->dest = index;

    if (SNetDistribIsNodeLocation(dest->node)) {
      dest->node = location;
      SNetIntListDestroy(dest->id);
      dest->id = SNetIntListCopy(SNetIdGet(info));
      SNetOutputManagerNewOut(SNetDestCopy(dest), in); //FIXME Dest copy memory
#ifdef SNET_DISTRIB_DEBUG
      SNetDestDump(dest, "New Out");
#endif
      in = NULL;
    } else if (SNetDistribIsNodeLocation(location)) {
      if (in == NULL) in = SNetStreamCreate(0);

#ifdef SNET_DISTRIB_DEBUG
      SNetDestDump(dest, "New In");
#endif
      SNetInputManagerNewIn(SNetDestCopy(dest), in); //FIXME
      dest->node = location;
      SNetIntListDestroy(dest->id);
      dest->id = SNetIntListCopy(SNetIdGet(info));
    } else {
#ifdef SNET_DISTRIB_DEBUG
      SNetDestDump(dest, "Don't care");
#endif
      dest->node = location;
      SNetIntListDestroy(dest->id);
      dest->id = SNetIntListCopy(SNetIdGet(info));
    }
  }

  return in;
}

void SNetRouteNewDynamic(snet_dest_t *dest)
{
  snet_stream_t *stream = NULL;
  snet_ast_t *ast = SNetASTLookup(dest->dynamicParent);
#ifdef SNET_DISTRIB_DEBUG
  SNetDestDump(dest, "ND");
#endif
  snet_info_t *info = SNetInfoInit();
  SNetInfoSetTag(info, prevDest, (uintptr_t) SNetDestCopy(dest),
                (void* (*)(void*)) &SNetDestCopy,
                (void (*)(void*)) &SNetDestDestroy);
  SNetIdSet(info, SNetIntListCopy(dest->id));

  stream = SNetInstantiatePlacement(ast, NULL, info, dest->dynamicLoc);
  SNetRouteUpdate(info, stream, dest->dynamicNode, ast->locvec.index);
  SNetInfoDestroy(info);
}

void SNetRouteDynamicEnter(snet_info_t *info, int dynamicParent, int dynamicLoc)
{
  snet_dest_t *dest = (snet_dest_t*) SNetInfoGetTag(info, prevDest);
  dest->dynamicParent = dynamicParent;
  dest->dynamicLoc = dynamicLoc;
  dest->dynamicNode = SNetDistribGetNodeId();
}
