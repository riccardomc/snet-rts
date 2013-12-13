#ifndef _SNETENTITIES_H_
#define _SNETENTITIES_H_

#include <stdio.h>

#include "ast.h"
#include "handle.h"
#include "constants.h"
#include "expression.h"
#include "interface_functions.h"
#include "snettypes.h"

int SNetNetToId(snet_startup_fun_t fun);
snet_startup_fun_t SNetIdToNet(int id);

#include "stream.h"

/****************************************************************************/
/* Alias                                                                    */
/****************************************************************************/
snet_ast_t *SNetAlias(int location,
                      snet_startup_fun_t net);



/****************************************************************************/
/* Box                                                                      */
/****************************************************************************/
snet_ast_t *SNetBox(int location,
                    const char *boxname,
                    snet_box_fun_t fun,
                    snet_exerealm_create_fun_t exerealm_create,
                    snet_exerealm_update_fun_t exerealm_update,
                    snet_exerealm_destroy_fun_t exerealm_destroy,
                    snet_int_list_list_t *out_variants);

snet_stream_t *SNetBoxInst(snet_stream_t *input,
                        snet_info_t *info,
                        snet_locvec_t *locvec,
                        int location,
                        const char *boxname,
                        snet_box_fun_t boxfun,
                        snet_exerealm_create_fun_t er_create,
                        snet_exerealm_update_fun_t er_update,
                        snet_exerealm_destroy_fun_t er_destroy,
                        snet_int_list_list_t *output_variants);


/****************************************************************************/
/* Synchrocell                                                              */
/****************************************************************************/
snet_ast_t *SNetSync(int location,
                     snet_variant_list_t *patterns,
                     snet_expr_list_t *guards);

snet_stream_t *SNetSyncInst( snet_stream_t *input,
    snet_info_t *info,
    snet_locvec_t *locvec,
    int location,
    snet_variant_list_t *patterns,
    snet_expr_list_t *guard_exprs );

/****************************************************************************/
/* Filter                                                                   */
/****************************************************************************/
snet_ast_t *SNetFilter(int location,
                       snet_variant_t *in_variant,
                       snet_expr_list_t *guards, ... );

snet_stream_t* SNetFilterInst( snet_stream_t *instream,
    snet_info_t *info,
    snet_locvec_t *locvec,
    int location,
    snet_variant_t *input_variant,
    snet_expr_list_t *guard_exprs,
    snet_filter_instr_list_list_t **instr_list);

snet_ast_t *SNetTranslate(int location,
                          snet_variant_t *in_variant,
                          snet_expr_list_t *guards, ... );

snet_stream_t* SNetTranslateInst( snet_stream_t *instream,
    snet_info_t *info,
    snet_locvec_t *locvec,
    int location,
    snet_variant_t *input_variant,
    snet_expr_list_t *guard_exprs,
    snet_filter_instr_list_list_t **instr_list);

snet_ast_t *SNetNameShift(int location,
                          int offset,
                          snet_variant_t *untouched);

snet_stream_t *SNetNameShiftInst( snet_stream_t *instream,
    snet_info_t *info,
    snet_locvec_t *locvec,
    int location,
    int offset,
    snet_variant_t *untouched,
    snet_expr_list_t *guard_exprs);


/****************************************************************************/
/* Serial                                                                   */
/****************************************************************************/
snet_ast_t *SNetSerial(int location,
                       snet_startup_fun_t box_a,
                       snet_startup_fun_t box_b);

snet_stream_t *SNetSerialInst(snet_stream_t *input,
    snet_info_t *info,
    snet_locvec_t *locvec,
    int location,
    snet_ast_t *box_a,
    snet_ast_t *box_b);


/****************************************************************************/
/* Parallel                                                                 */
/****************************************************************************/
snet_ast_t *SNetParallel(int location,
                         snet_variant_list_list_t *types,
                         ...);

snet_stream_t *SNetParallelInst( snet_stream_t *instream,
    snet_info_t *info,
    snet_locvec_t *locvec,
    int location,
    snet_variant_list_list_t *variant_lists,
    snet_ast_t **ast);

snet_ast_t *SNetParallelDet(int location,
                            snet_variant_list_list_t *types,
                            ...);

snet_stream_t *SNetParallelDetInst( snet_stream_t *inbuf,
    snet_info_t *info,
    snet_locvec_t *locvec,
    int location,
    snet_variant_list_list_t *variant_lists,
    snet_ast_t **ast);


/****************************************************************************/
/* Star                                                                     */
/****************************************************************************/
snet_ast_t *SNetStar(int location,
                     snet_variant_list_t *type,
                     snet_expr_list_t *guards,
                     snet_startup_fun_t box_a,
                     snet_startup_fun_t box_b);

snet_stream_t *SNetStarInst( snet_stream_t *input,
    snet_info_t *info,
    snet_locvec_t *locvec,
    int location,
    snet_variant_list_t *exit_patterns,
    snet_expr_list_t *guards,
    snet_ast_t *box_a,
    snet_ast_t *box_b);

snet_ast_t *SNetStarIncarnate(int location,
                              snet_variant_list_t *type,
                              snet_expr_list_t *guards,
                              snet_startup_fun_t box_a,
                              snet_startup_fun_t box_b);

snet_stream_t *SNetStarIncarnateInst( snet_stream_t *input,
    snet_info_t *info,
    snet_locvec_t *locvec,
    int location,
    snet_variant_list_t *exit_patterns,
    snet_expr_list_t *guards,
    snet_ast_t *box_a,
    snet_ast_t *box_b);

snet_ast_t *SNetStarDet(int location,
                        snet_variant_list_t *type,
                        snet_expr_list_t *guards,
                        snet_startup_fun_t box_a,
                        snet_startup_fun_t box_b);

snet_stream_t *SNetStarDetInst(snet_stream_t *input,
    snet_info_t *info,
    snet_locvec_t *locvec,
    int location,
    snet_variant_list_t *exit_patterns,
    snet_expr_list_t *guards,
    snet_ast_t *box_a,
    snet_ast_t *box_b);

snet_ast_t *SNetStarDetIncarnate(int location,
                                 snet_variant_list_t *type,
                                 snet_expr_list_t *guards,
                                 snet_startup_fun_t box_a,
                                 snet_startup_fun_t box_b);

snet_stream_t *SNetStarDetIncarnateInst(snet_stream_t *input,
    snet_info_t *info,
    snet_locvec_t *locvec,
    int location,
    snet_variant_list_t *exit_patterns,
    snet_expr_list_t *guards,
    snet_ast_t *box_a,
    snet_ast_t *box_b);

/****************************************************************************/
/* Split                                                                    */
/****************************************************************************/
snet_ast_t *SNetSplit(int location,
                      snet_startup_fun_t box_a,
                      int ltag,
                      int utag);

snet_stream_t *SNetSplitInst( snet_stream_t *input,
    snet_info_t *info,
    snet_locvec_t *locvec,
    int location,
    snet_ast_t *box_a,
    int ltag, int utag);

snet_ast_t *SNetSplitDet(int location,
                         snet_startup_fun_t box_a,
                         int ltag,
                         int utag);

snet_stream_t *SNetSplitDetInst( snet_stream_t *input,
    snet_info_t *info,
    snet_locvec_t *locvec,
    int location,
    snet_ast_t *box_a,
    int ltag, int utag);

snet_ast_t *SNetLocSplit(int location,
                         snet_startup_fun_t box_a,
                         int ltag, int utag);

snet_stream_t *SNetLocSplitInst( snet_stream_t *input,
    snet_info_t *info,
    snet_locvec_t *locvec,
    int location,
    snet_ast_t *box_a,
    int ltag, int utag);

snet_ast_t *SNetLocSplitDet(int location,
                            snet_startup_fun_t box_a,
                            int ltag,
                            int utag);

snet_stream_t *SNetLocSplitDetInst( snet_stream_t *input,
    snet_info_t *info,
    snet_locvec_t *locvec,
    int location,
    snet_ast_t *box_a,
    int ltag, int utag);


/****************************************************************************/
/* Feedback                                                                 */
/****************************************************************************/
snet_ast_t *SNetFeedback(int location,
                         snet_variant_list_t *back_pattern,
                         snet_expr_list_t *guards,
                         snet_startup_fun_t box_a);

snet_stream_t *SNetFeedbackDet( snet_stream_t *inbuf,
    snet_info_t *info,
    int location,
    snet_variant_list_t *back_pattern,
    snet_expr_list_t *guards,
    snet_startup_fun_t box_a);


#endif /* _SNETENTITIES_H_ */
