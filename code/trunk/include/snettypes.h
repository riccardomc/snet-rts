#ifndef _SNET_SNETTYPES_H_
#define _SNET_SNETTYPES_H_

#include "handle.h"
#include "stream_layer.h"

/* Type for start-up functions generated by snet compiler: */
#ifdef DISTRIBUTED_SNET
typedef struct snet_routing_info snet_routing_info_t;

typedef snet_routing_info_t snet_info_t;

typedef snet_tl_stream_t *(*snet_startup_fun_t)(snet_tl_stream_t *, snet_info_t *, int);
#else
typedef snet_tl_stream_t *(*snet_startup_fun_t)(snet_tl_stream_t *);
#endif

typedef void (*snet_box_fun_t)(snet_handle_t*);

#endif /* _SNET_SNETTYPES_H_ */
