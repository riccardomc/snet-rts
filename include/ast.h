#ifndef AST_H
#define AST_H

#include "list.h"
#include "snettypes.h"

typedef snet_int_list_t snet_id_t;

void SNetIdInit(snet_info_t *info);
void SNetIdAppend(snet_info_t *info, int i);
void SNetIdInc(snet_info_t *info);
int SNetIdTop(snet_info_t *info);

void SNetIdSet(snet_info_t *info, snet_id_t *id);
snet_id_t *SNetIdGet(snet_info_t *info);

snet_stream_t *SNetInstantiate(snet_ast_t *tree, snet_stream_t *input, snet_info_t *info);
snet_stream_t *SNetInstantiatePlacement(snet_ast_t *tree, snet_stream_t *input, snet_info_t *info, int location);

void SNetASTCleanup(snet_ast_t *tree);

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
  int num;
  struct loc_item *parent;
} snet_locvec_t;

const char *SNetNameCreate(snet_locvec_t *locvec, snet_id_t *id, const char *name);

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
#endif
