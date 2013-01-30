#include <stdlib.h>

#include "ast.h"
#include "debug.h"
#include "memfun.h"
#include "snetentities.h"
#include "distribution.h"

snet_stream_t *SNetInstantiatePlacement(snet_ast_t *tree, snet_stream_t *input, snet_info_t *info, int location)
{
  location = location >= 0 ? location :
                tree->location >= 0 ? tree->location : SNetDistribGetNodeId();

  snet_stream_t *output = NULL;
  switch (tree->type) {
      case snet_box:
          output = SNetBoxInst(input, info, location, tree->box.boxname,
                                tree->box.boxfun, tree->box.er_create,
                                tree->box.er_update, tree->box.er_destroy,
                                tree->box.output_variants);
          break;
      case snet_filter:
      case snet_translate:
          output = SNetFilterInst(input, info, location,
                                  tree->filter.input_variant,
                                  tree->filter.guard_exprs,
                                  tree->filter.instr_list);
          break;
      case snet_nameshift:
          output = SNetNameShiftInst(input, info, location,
                                      tree->nameshift.offset,
                                      tree->nameshift.untouched,
                                      tree->nameshift.guard_exprs);
          break;
      case snet_sync:
          output = SNetSyncInst(input, info, location, tree->sync.patterns,
                                tree->sync.guard_exprs);
          break;
      case snet_feedback:
          output = SNetFeedbackInst(input, info, location, tree->feedback.back_patterns,
                                    tree->feedback.guards,
                                    tree->feedback.box_a);
          break;
      case snet_serial:
          output = SNetSerialInst(input, info, location, tree->serial.box_a,
                                  tree->serial.box_b);
          break;
      case snet_parallel:
          if (tree->parallel.det) {
            output = SNetParallelDetInst(input, info, location,
                                          tree->parallel.variant_lists,
                                          tree->parallel.branches);
          } else {
            output = SNetParallelInst(input, info, location,
                                      tree->parallel.variant_lists,
                                      tree->parallel.branches);
          }
          break;
      case snet_split:
          if (tree->split.det && tree->split.loc) {
            output = SNetLocSplitDetInst(input, info, location, tree->split.box_a,
                                          tree->split.ltag, tree->split.utag);
          } else if (tree->split.det) {
            output = SNetSplitDetInst(input, info, location, tree->split.box_a,
                                      tree->split.ltag, tree->split.utag);
          } else if (tree->split.loc) {
            output = SNetLocSplitInst(input, info, location, tree->split.box_a,
                                      tree->split.ltag, tree->split.utag);
          } else {
            output = SNetSplitInst(input, info, location, tree->split.box_a,
                                    tree->split.ltag, tree->split.utag);
          }
          break;
      case snet_star:
          if (tree->star.det && tree->star.incarnate) {
            output = SNetStarDetIncarnateInst(input, info, location,
                                              tree->star.exit_patterns,
                                              tree->star.guards,
                                              tree->star.box_a,
                                              tree->star.box_b);
          } else if (tree->star.det) {
            output = SNetStarDetInst(input, info, location,
                                      tree->star.exit_patterns,
                                      tree->star.guards,
                                      tree->star.box_a,
                                      tree->star.box_b);
          } else if (tree->star.incarnate) {
            output = SNetStarIncarnateInst(input, info, location,
                                            tree->star.exit_patterns,
                                            tree->star.guards,
                                            tree->star.box_a,
                                            tree->star.box_b);
          } else {
            output = SNetStarInst(input, info, location,
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

snet_stream_t *SNetInstantiate(snet_ast_t *tree, snet_stream_t *input, snet_info_t *info)
{ return SNetInstantiatePlacement(tree, input, info, -1); }

void SNetASTCleanup(snet_ast_t *tree)
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
            SNetASTCleanup(tree->feedback.box_a);
            break;
        case snet_serial:
            SNetASTCleanup(tree->serial.box_a);
            SNetASTCleanup(tree->serial.box_b);
            break;
        case snet_parallel:
            {
              int num = SNetVariantListListLength(tree->parallel.variant_lists);
              for (int i = 0; i < num; i++) {
                SNetASTCleanup(tree->parallel.branches[i]);
              }

              SNetVariantListListDestroy(tree->parallel.variant_lists);
              SNetMemFree(tree->parallel.branches);
            }
            break;
        case snet_split:
            SNetASTCleanup(tree->split.box_a);
            break;
        case snet_star:
            SNetExprListDestroy(tree->star.guards);
            SNetVariantListDestroy(tree->star.exit_patterns);
            SNetASTCleanup(tree->star.box_a);
            if (!tree->star.incarnate) SNetASTCleanup(tree->star.box_b);
            break;
        default:
            SNetUtilDebugFatal("Combinator not implemented.");
            break;
    }
    SNetMemFree(tree);
}
