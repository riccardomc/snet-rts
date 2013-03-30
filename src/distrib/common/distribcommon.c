#include <pthread.h>

#include "distribcommon.h"
#include "imanager.h"
#include "info.h"
#include "memfun.h"
#include "omanager.h"
#include "reference.h"
#include "snetentities.h"

#define SNET_DBG_TIMING

#ifdef SNET_DBG_TIMING
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
static double start_cpu, start_real;
static double end_cpu, end_real;
static double time_cpu, time_real;

double realclock() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (double)tv.tv_sec + (double)tv.tv_usec * 1e-6;
}

double cpuclock() {
  return (double)clock();
}

#endif //SNET_DBG_TIMING

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

#ifdef SNET_DBG_TIMING
  start_cpu = cpuclock();
  start_real = realclock();
#endif

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

#ifdef SNET_DBG_TIMING
  end_cpu = cpuclock();
  end_real = realclock();
  time_cpu = (end_cpu - start_cpu) / CLOCKS_PER_SEC;
  time_real = end_real - start_real;
  FILE *f = stdout;
  char *fname = getenv("SNET_DBG_TIMING_LOG");
  if (fname != NULL) {
    f = fopen(fname, "ab+");
  }
  fprintf(f, "%f %f\n", time_real, time_cpu);
#endif

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
