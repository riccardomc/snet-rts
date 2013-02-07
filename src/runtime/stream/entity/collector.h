#ifndef _COLLECTORS_H_
#define _COLLECTORS_H_

#include "ast.h"
#include "snettypes.h"
#include "info.h"


snet_stream_t *CollectorCreateStatic( int num, snet_stream_t **instreams, snet_locvec_t *locvec, int location, snet_info_t *info);


snet_stream_t *CollectorCreateDynamic( snet_stream_t *instream, snet_locvec_t *locvec, int location, snet_info_t *info);

#endif
