#ifndef _SNET_DISTRIBUTION_H_
#define _SNET_DISTRIBUTION_H_

#include <stddef.h>
#include <stdint.h>

typedef struct snet_ref snet_ref_t;

#include "info.h"
#include "stream.h"
#include "snettypes.h"
#include "bool.h"
#include "ast.h"

/* The root node in distributed S-Net. */
#define ROOT_LOCATION           0

/* Provided by common distribution interface in src/distrib/common */
void SNetDistribInit(int argc, char** argv, snet_info_t *info);
void SNetDistribStart(void);
void SNetDistribStop(void);
void SNetDistribWaitExit(snet_info_t *info);

snet_stream_t *SNetRouteUpdate(snet_info_t *info, snet_stream_t *in,
                              int location, int index);

void SNetRouteDynamicEnter(snet_info_t *info, int dynamicIdx, int dynamicLoc);
snet_ref_t *SNetRefCreate(void *data, int interface);
snet_ref_t *SNetRefCopy(snet_ref_t *ref);
void *SNetRefGetData(snet_ref_t *ref);
void *SNetRefTakeData(snet_ref_t *ref);
void SNetRefDestroy(snet_ref_t *ref);


/* Implementation specific functions in src/distrib/implementation */
void SNetDistribGlobalStop(void);

int SNetDistribGetNodeId(void);
bool SNetDistribIsNodeLocation(int location);
bool SNetDistribIsRootNode(void);
bool SNetDistribIsDistributed(void);

void SNetDistribPackOld(void *src, ...);
void SNetDistribUnpackOld(void *dst, ...);

void SNetDistribPack(void *buf, void *src, size_t size);
void SNetDistribUnpack(void *buf, void *dst, size_t size);
#endif /* _SNET_DISTRIBUTION_H_ */
