#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "debug.h"
#include "list.h"
#include "memfun.h"
#include "snetentities.h"
#include "distribution.h"

#define MAP_NAME_H Ast
#define MAP_TYPE_NAME_H ast
#define MAP_KEY_H int
#define MAP_VAL_H snet_ast_t*
#include "map-template.h"
#undef MAP_VAL_H
#undef MAP_KEY_H
#undef MAP_TYPE_NAME_H
#undef MAP_NAME_H

#define MAP_NAME Ast
#define MAP_TYPE_NAME ast
#define MAP_KEY int
#define MAP_VAL snet_ast_t*
#define MAP_VAL_CMP SNetAstCompare
#include "map-template.c"
#undef MAP_VAL
#undef MAP_VAL_CMP
#undef MAP_KEY
#undef MAP_TYPE_NAME
#undef MAP_NAME

static snet_info_tag_t id_tag;
static snet_ast_t *AST = NULL;
static snet_ast_map_t *AST_map = NULL;
static int AST_index = 0;

void SNetIdInit(snet_info_t *info)
{
  id_tag = SNetInfoCreateTag();
  SNetInfoSetTag(info, id_tag, (uintptr_t) SNetIntListCreate(0), (void *(*)(void *)) &SNetIntListCopy, (void (*)(void *)) &SNetIntListDestroy);
}

void SNetIdDumpAux(snet_id_t *id)
{
  int val;
  printf("[ ");
  LIST_FOR_EACH(id, val) {
    printf("%d ", val);
  }
  printf("]");
}

void SNetIdDump(snet_info_t *info)
{
  SNetIdDumpAux(SNetIdGet(info));
}

void SNetIdSet(snet_info_t *info, snet_id_t *id)
{
  SNetInfoSetTag(info, id_tag, (uintptr_t) id,
                 (void *(*)(void *)) &SNetIntListCopy,
                 (void (*)(void *)) &SNetIntListDestroy);
}

snet_id_t *SNetIdGet(snet_info_t *info)
{
  return (snet_id_t*) SNetInfoGetTag(info, id_tag);
}

void SNetIdAppend(snet_info_t *info, int i)
{
  SNetIntListAppendStart(SNetIdGet(info), i);
}

void SNetIdInc(snet_info_t *info)
{
  int i = SNetIntListPopStart(SNetIdGet(info));
  SNetIntListAppendStart(SNetIdGet(info), i+1);
}

int SNetIdTop(snet_info_t *info)
{
  return SNetIntListGet(SNetIdGet(info), 0);
}

inline static int max(int x, int y) { return x > y ? x : y; }

//The number of characters required to print a number in decimal notation
inline static int num_digits(int x)
{
  if (x == 0) return 1;
  else return ceil(log10(x + 1));
}

inline static bool parentStarOrSplit(snet_locvec_t *l)
{
  return l && l->parent && (l->parent->type == LOC_SPLIT || l->parent->type == LOC_STAR);
}

static int locvecSize(snet_id_t *id, snet_locvec_t *locvec, int *idx)
{
  int result = 0;
  if (!locvec) return 0;
  else if (locvec->parent) result += locvecSize(id, locvec->parent, idx);

  if (locvec->num >= 0) {
    result += num_digits(locvec->num);
  } else if (parentStarOrSplit(locvec)) {
    result += num_digits(SNetIntListGet(id, (*idx)++));
  }

  switch (locvec->type) {
    case LOC_BOX:
    case LOC_FILTER:
    case LOC_SYNC:
      break;
    case LOC_SERIAL:
    case LOC_PARALLEL:
    case LOC_FEEDBACK:
    case LOC_SPLIT:
    case LOC_STAR:
      result += 2;
      break;
    default:
      SNetUtilDebugFatal("ACH, MEIN LEBEN!");
  }

  return result;
}

static int locvecPrint(char *buf, int *idx, snet_id_t *id, snet_locvec_t *locvec)
{
  int result = 0;
  if (!locvec) return 0;
  else if (locvec->parent) result += locvecPrint(buf, idx, id, locvec->parent);

  if (locvec->num >= 0) {
    result += sprintf(buf + result, "%d", locvec->num);
  } else if (parentStarOrSplit(locvec)) {
    result += sprintf(buf + result, "%d", SNetIntListGet(id, (*idx)++));
  }

  switch (locvec->type) {
    case LOC_BOX:
    case LOC_FILTER:
    case LOC_SYNC:
      break;
    case LOC_SERIAL:
    case LOC_PARALLEL:
    case LOC_FEEDBACK:
    case LOC_SPLIT:
    case LOC_STAR:
      result += sprintf(buf + result, ":%c", (char) locvec->type);
      break;
    default:
      SNetUtilDebugFatal("ACH, MEIN LEBEN!");
  }

  return result;
}

const char *SNetNameCreate(snet_locvec_t *locvec, snet_id_t *id, const char *name)
{
  int i = 0;
  int namelen = name ? strlen(name) : 0;
  int size = locvecSize(id, locvec, &i) + namelen + 1;
  char *result = SNetMemAlloc(size);
  i = 0;

  strncpy(result, name, namelen);
  locvecPrint(result + namelen, &i, id, locvec);
  result[size-1] = '\0';

  return result;
}

snet_stream_t *SNetInstantiatePlacement(snet_ast_t *tree, snet_stream_t *input,
                                        snet_info_t *info, int location)
{
  location = location >= 0 ? location :
                tree->location >= 0 ? tree->location : SNetDistribGetNodeId();

  snet_stream_t *output = NULL;
  switch (tree->type) {
      case snet_box:
          output = SNetBoxInst(input, info, &tree->locvec, location,
                                tree->box.boxname, tree->box.boxfun,
                                tree->box.er_create, tree->box.er_update,
                                tree->box.er_destroy, tree->box.output_variants);
          break;
      case snet_filter:
      case snet_translate:
          output = SNetFilterInst(input, info, &tree->locvec, location,
                                  tree->filter.input_variant,
                                  tree->filter.guard_exprs,
                                  tree->filter.instr_list);
          break;
      case snet_nameshift:
          output = SNetNameShiftInst(input, info, &tree->locvec, location,
                                      tree->nameshift.offset,
                                      tree->nameshift.untouched,
                                      tree->nameshift.guard_exprs);
          break;
      case snet_sync:
          output = SNetSyncInst(input, info, &tree->locvec, location,
                                tree->sync.patterns, tree->sync.guard_exprs);
          break;
      case snet_feedback:
          output = SNetFeedbackInst(input, info, &tree->locvec, location,
                                    tree->feedback.back_patterns,
                                    tree->feedback.guards,
                                    tree->feedback.box_a);
          break;
      case snet_serial:
          output = SNetSerialInst(input, info, &tree->locvec, location,
                                  tree->serial.box_a, tree->serial.box_b);
          break;
      case snet_parallel:
          if (tree->parallel.det) {
            output = SNetParallelDetInst(input, info, &tree->locvec, location,
                                          tree->parallel.variant_lists,
                                          tree->parallel.branches);
          } else {
            output = SNetParallelInst(input, info, &tree->locvec, location,
                                      tree->parallel.variant_lists,
                                      tree->parallel.branches);
          }
          break;
      case snet_split:
          if (tree->split.det && tree->split.loc) {
            output = SNetLocSplitDetInst(input, info, &tree->locvec, location,
                                         tree->split.box_a, tree->split.ltag,
                                         tree->split.utag);
          } else if (tree->split.det) {
            output = SNetSplitDetInst(input, info, &tree->locvec, location,
                                      tree->split.box_a, tree->split.ltag,
                                      tree->split.utag);
          } else if (tree->split.loc) {
            output = SNetLocSplitInst(input, info, &tree->locvec, location,
                                      tree->split.box_a, tree->split.ltag,
                                      tree->split.utag);
          } else {
            output = SNetSplitInst(input, info, &tree->locvec, location,
                                   tree->split.box_a, tree->split.ltag,
                                   tree->split.utag);
          }
          break;
      case snet_star:
          if (tree->star.det && tree->star.incarnate) {
            output = SNetStarDetIncarnateInst(input, info, &tree->locvec, location,
                                              tree->star.exit_patterns,
                                              tree->star.guards,
                                              tree->star.box_a,
                                              tree->star.box_b);
          } else if (tree->star.det) {
            output = SNetStarDetInst(input, info, &tree->locvec, location,
                                      tree->star.exit_patterns,
                                      tree->star.guards,
                                      tree->star.box_a,
                                      tree->star.box_b);
          } else if (tree->star.incarnate) {
            output = SNetStarIncarnateInst(input, info, &tree->locvec, location,
                                            tree->star.exit_patterns,
                                            tree->star.guards,
                                            tree->star.box_a,
                                            tree->star.box_b);
          } else {
            output = SNetStarInst(input, info, &tree->locvec, location,
                                  tree->star.exit_patterns,
                                  tree->star.guards,
                                  tree->star.box_a,
                                  tree->star.box_b);
          }
          break;
      default:
          SNetUtilDebugFatal("Combinator not implemented.");
          break;
  }
  return output;
}

snet_stream_t *SNetInstantiate(snet_ast_t *tree, snet_stream_t *input,
                               snet_info_t *info)
{
  return SNetInstantiatePlacement(tree, input, info, -1);
}

snet_ast_t *SNetASTInit(snet_startup_fun_t fun, int location)
{
  AST_map = SNetAstMapCreate(0);
  AST = fun(location);
  return AST;
}

snet_ast_t *SNetASTLookup(int index)
{
  return SNetAstMapGet(AST_map, index);
}

int SNetASTRegister(snet_ast_t *tree)
{
  SNetAstMapSet(AST_map, AST_index, tree);
  return AST_index++;
}

int SNetASTDeregister(snet_ast_t *tree)
{
  //FIXME
  return 0;
}

void SNetAstCleanup(snet_ast_t *tree)
{
    switch (tree->type) {
        case snet_box:
            SNetIntListListDestroy(tree->box.output_variants);
            break;
        case snet_filter:
        case snet_translate:
            SNetVariantDestroy(tree->filter.input_variant);

            if (tree->filter.instr_list != NULL) {
              int i;
              snet_expr_t *expr;
              (void) expr;

              LIST_ENUMERATE(tree->filter.guard_exprs, i, expr) {
                SNetFilterInstrListListDestroy(tree->filter.instr_list[i]);
              }

              SNetMemFree(tree->filter.instr_list);
            }

            SNetExprListDestroy(tree->filter.guard_exprs);
            break;
        case snet_nameshift:
            SNetVariantDestroy(tree->nameshift.untouched);
            SNetExprListDestroy(tree->nameshift.guard_exprs);
            break;
        case snet_sync:
            SNetVariantListDestroy(tree->sync.patterns);
            SNetExprListDestroy(tree->sync.guard_exprs);
            break;
        case snet_feedback:
            SNetVariantListDestroy(tree->feedback.back_patterns);
            SNetExprListDestroy(tree->feedback.guards);
            SNetAstCleanup(tree->feedback.box_a);
            break;
        case snet_serial:
            SNetAstCleanup(tree->serial.box_a);
            SNetAstCleanup(tree->serial.box_b);
            break;
        case snet_parallel:
            {
              int num = SNetVariantListListLength(tree->parallel.variant_lists);
              for (int i = 0; i < num; i++) {
                SNetAstCleanup(tree->parallel.branches[i]);
              }

              SNetVariantListListDestroy(tree->parallel.variant_lists);
              SNetMemFree(tree->parallel.branches);
            }
            break;
        case snet_split:
            SNetAstCleanup(tree->split.box_a);
            break;
        case snet_star:
            SNetExprListDestroy(tree->star.guards);
            SNetVariantListDestroy(tree->star.exit_patterns);
            SNetAstCleanup(tree->star.box_a);
            if (!tree->star.incarnate) SNetAstCleanup(tree->star.box_b);
            break;
        default:
             SNetUtilDebugFatal("Combinator not implemented.");
            break;
    }
    SNetMemFree(tree);
}

void SNetASTCleanup()
{
  SNetAstCleanup(AST);
}

