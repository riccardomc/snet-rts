#include "map.h"

/* !! CAUTION !!
 * These includes are mirrored in include/map.h, keep in sync under penalty of
 * defenestration!
 */
#define MAP_NAME Int
#define MAP_TYPE_NAME int
#define MAP_VAL int
#include "map-template.c"
#undef MAP_VAL
#undef MAP_TYPE_NAME
#undef MAP_NAME

void SNetRefSerialise(snet_ref_t *ref, void *buf);
snet_ref_t *SNetRefDeserialise(void *buf);

#define MAP_NAME Ref
#define MAP_TYPE_NAME ref
#define MAP_VAL snet_ref_t*
#define MAP_VAL_COPY_FUN SNetRefCopy
#define MAP_VAL_SERIALISE_FUN SNetRefSerialise
#define MAP_VAL_DESERIALISE_FUN SNetRefDeserialise
#include "map-template.c"
#undef MAP_VAL_DESERIALISE_FUN
#undef MAP_VAL_SERIALISE_FUN
#undef MAP_VAL_COPY_FUN
#undef MAP_VAL
#undef MAP_TYPE_NAME
#undef MAP_NAME
