#ifndef AST_H
#define AST_H

#include "list.h"
#include "memfun.h"
#include "snettypes.h"

void SNetDistribPack(void *buf, void *src, size_t size);
void SNetDistribUnpack(void *buf, void *dst, size_t size);

typedef snet_int_list_t snet_id_t;

void SNetIdInit(snet_info_t *info);
void SNetIdAppend(snet_info_t *info, int i);
void SNetIdInc(snet_info_t *info);
int SNetIdTop(snet_info_t *info);
void SNetIdDump(snet_info_t *info);
void SNetIdDumpAux(snet_id_t *id);

void SNetIdSet(snet_info_t *info, snet_id_t *id);
snet_id_t *SNetIdGet(snet_info_t *info);

inline static bool SNetIdCompare(snet_id_t *id1, snet_id_t *id2)
{
  bool result = true;
  int i, len = SNetIntListLength(id1);

  result = len == SNetIntListLength(id2);
  for (i = 0; result && (i < len); i++) {
    result = SNetIntListGet(id1, i) == SNetIntListGet(id2, i);
  }

  return result;
}

enum snet_types {
    snet_box,
    snet_filter,
    snet_translate,
    snet_nameshift,
    snet_sync,
    snet_feedback,
    snet_serial,
    snet_parallel,
    snet_star,
    snet_split
};

typedef struct {
    const char *boxname;
    snet_box_fun_t boxfun;
    snet_exerealm_create_fun_t er_create;
    snet_exerealm_update_fun_t er_update;
    snet_exerealm_destroy_fun_t er_destroy;
    snet_int_list_list_t *output_variants;
} snet_box_t;

typedef struct {
    snet_variant_t *input_variant;
    snet_expr_list_t *guard_exprs;
    snet_filter_instr_list_list_t **instr_list;
} snet_filter_translate_t;

typedef struct {
    int offset;
    snet_variant_t *untouched;
    snet_expr_list_t *guard_exprs;
} snet_nameshift_t;

typedef struct {
    snet_variant_list_t *patterns;
    snet_expr_list_t *guard_exprs;
} snet_sync_t;

//---------------------

typedef enum {
  LOC_SERIAL = 'S',
  LOC_PARALLEL = 'P',
  LOC_SPLIT = 'I',
  LOC_STAR = 'R',
  LOC_FEEDBACK = 'F',
  LOC_BOX = 'B',
  LOC_FILTER = 'L',
  LOC_SYNC = 'Y'
} snet_loctype_t;

typedef struct loc_item {
  snet_loctype_t type;
  int index, num;
  struct loc_item *parent;
} snet_locvec_t;

const char *SNetNameCreate(snet_locvec_t *locvec, snet_id_t *id, const char *name);

inline static bool SNetLocvecCompare(snet_locvec_t *l1, snet_locvec_t *l2)
{
  bool result = true;
  while (result && l1 && l2) {
    result = l1->index == l2->index &&
      l1->type == l2->type &&
      l1->num == l2->num;
    l1 = l1->parent;
    l2 = l2->parent;
  }

  if ((!l1 && l2) || (l1 && !l2)) {
    result = false;
  }

  return result;
}

inline static snet_locvec_t *SNetLocvecItemCopy(snet_locvec_t *locvec)
{
  snet_locvec_t *result = SNetMemAlloc(sizeof(snet_locvec_t));
  *result = *locvec;
  result->parent = NULL;
  return result;
}

inline static void SNetLocvecItemSerialise(snet_locvec_t *locvec, void *buf)
{
  SNetDistribPack(buf, &locvec->type, sizeof(snet_loctype_t));
  SNetDistribPack(buf, &locvec->index, sizeof(int));
  SNetDistribPack(buf, &locvec->num, sizeof(int));
}

inline static snet_locvec_t *SNetLocvecItemDeserialise(void *buf)
{
  snet_locvec_t *locvec = SNetMemAlloc(sizeof(snet_locvec_t));
  SNetDistribUnpack(buf, &locvec->type, sizeof(snet_loctype_t));
  SNetDistribUnpack(buf, &locvec->index, sizeof(int));
  SNetDistribUnpack(buf, &locvec->num, sizeof(int));
  locvec->parent = NULL;
  return locvec;
}

//---------------------

typedef struct {
    snet_variant_list_t *back_patterns;
    snet_expr_list_t *guards;
    snet_ast_t *box_a;
} snet_feedback_t;

typedef struct {
    snet_ast_t *box_a;
    snet_ast_t *box_b;
} snet_serial_t;

typedef struct {
    bool det;
    snet_variant_list_list_t *variant_lists;
    snet_ast_t **branches;
} snet_parallel_t;

typedef struct {
    bool det;
    bool incarnate;
    snet_variant_list_t *exit_patterns;
    snet_expr_list_t *guards;
    snet_ast_t *box_a;
    snet_ast_t *box_b;
} snet_star_t;

typedef struct {
    bool det;
    bool loc;
    snet_ast_t *box_a;
    int ltag, utag;
} snet_split_t;

struct SNET_AST {
    int location;
    enum snet_types type;
    snet_locvec_t locvec;
    union {
        snet_box_t box;
        snet_filter_translate_t filter;
        snet_nameshift_t nameshift;
        snet_sync_t sync;
        snet_feedback_t feedback;
        snet_serial_t serial;
        snet_parallel_t parallel;
        snet_star_t star;
        snet_split_t split;
    };
};


snet_stream_t *SNetInstantiate(snet_ast_t *tree, snet_stream_t *input, snet_info_t *info);
snet_stream_t *SNetInstantiatePlacement(snet_ast_t *tree, snet_stream_t *input, snet_info_t *info, int location);

snet_ast_t *SNetASTInit(snet_startup_fun_t fun, int location);
int SNetASTRegister(snet_ast_t *tree);
int SNetASTDeregister(snet_ast_t *tree);
snet_ast_t *SNetASTLookup(int index);
snet_ast_t *SNetASTInit(snet_startup_fun_t fun, int location);
void SNetASTCleanup();

inline static bool SNetAstCompare(snet_ast_t *a1, snet_ast_t *a2)
{
  //FIXME
  return false;
}

#endif
