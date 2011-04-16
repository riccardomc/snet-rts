#include <assert.h>

#include "snetentities.h"

#include "expression.h"
#include "memfun.h"
#include "collector.h"

#include "threading.h"
#include "distribution.h"

static bool MatchesExitPattern( snet_record_t *rec,
    snet_variant_list_t *exit_patterns, snet_expr_list_t *guards)
{
  int i = 0;
  snet_variant_t *pattern;

  LIST_FOR_EACH(exit_patterns, pattern)
    if( SNetEevaluateBool( SNetEgetExpr( guards, i), rec) &&
        SNetRecPatternMatches(pattern, rec)) {
      return true;
    }
    i++;
  END_FOR

  return false;
}



typedef struct {
  snet_stream_t *input, *output;
  snet_variant_list_t *exit_patterns;
  snet_startup_fun_t box, selffun;
  snet_expr_list_t *guards;
  snet_info_t *info;
  bool is_det, is_incarnate;
} star_arg_t;

/**
 * Star component task
 */
static void StarBoxTask( snet_entity_t *self, void *arg)
{
  star_arg_t *sarg = (star_arg_t *)arg;
  snet_stream_desc_t *instream;
  snet_stream_desc_t *outstream; /* the stream to the collector */
  /* The stream to the next instance;
     a non-null value indicates that the instance has been created. */
  snet_stream_desc_t *nextstream=NULL;
  bool terminate = false;
  snet_record_t *rec;
  /* for deterministic variant: */
  int counter = 0;

  instream  = SNetStreamOpen( self, sarg->input, 'r');
  outstream = SNetStreamOpen( self, sarg->output, 'w');

  /* MAIN LOOP */
  while( !terminate) {
    /* read from input stream */
    rec = SNetStreamRead( instream);

    switch( SNetRecGetDescriptor( rec)) {

      case REC_data:
        if( MatchesExitPattern( rec, sarg->exit_patterns, sarg->guards)) {
          /* send rec to collector */
          SNetStreamWrite( outstream, rec);
        } else {
          /* send to next instance */
          /* if instance has not been created yet, create it */
          if( nextstream == NULL) {
            /* The outstream to the collector from a newly created incarnate.
               Nothing is written to this stream, but it has to be registered
               at the collector. */
            snet_stream_t *starstream, *nextstream_addr;
            /* Create the stream to the instance */
            nextstream_addr = SNetStreamCreate(0);
            nextstream = SNetStreamOpen( self, nextstream_addr, 'w');
            /* register new buffer with dispatcher,
               starstream is returned by selffun, which is SNetStarIncarnate */
            starstream = SNetSerial(nextstream_addr, sarg->info, SNetNodeLocation , sarg->box, sarg->selffun);

            /* notify collector about the new instance */
            SNetStreamWrite( outstream, SNetRecCreate(REC_collect, starstream));
          }
          /* send the record to the instance */
          SNetStreamWrite( nextstream, rec);
        } /* end if not matches exit pattern */

        /* deterministic non-incarnate has to append control records */
        if (sarg->is_det && !sarg->is_incarnate) {
          /* send new sort record to collector */
          SNetStreamWrite( outstream,
              SNetRecCreate( REC_sort_end, 0, counter) );

          /* if has next instance, send new sort record */
          if (nextstream != NULL) {
            SNetStreamWrite( nextstream,
                SNetRecCreate( REC_sort_end, 0, counter) );
          }
        }
        break;

      case REC_sync:
        {
          snet_stream_t *newstream = SNetRecGetStream( rec);
          SNetStreamReplace( instream, newstream);
          SNetRecDestroy( rec);
        }
        break;

      case REC_collect:
        assert(0);
        /* if ignoring, at least destroy ... */
        SNetRecDestroy( rec);
        break;

      case REC_sort_end:
        {
          int rec_lvl = SNetRecGetLevel(rec);
          /* send a copy to the box, if exists */
          if( nextstream != NULL) {
            SNetStreamWrite(
                nextstream,
                SNetRecCreate( REC_sort_end,
                  (!sarg->is_incarnate)? rec_lvl+1 : rec_lvl,
                  SNetRecGetNum(rec) )
                );
          }

          /* send the original one to the collector */
          if (!sarg->is_incarnate) {
            /* if non-incarnate, we have to increase level */
            SNetRecSetLevel( rec, rec_lvl+1);
          }
          SNetStreamWrite( outstream, rec);
        }
        break;

      case REC_terminate:
        terminate = true;
        if( nextstream != NULL) {
          SNetStreamWrite( nextstream, SNetRecCopy( rec));
        }
        SNetStreamWrite( outstream, rec);
        /* note that no sort record has to be appended */
        break;

      default:
        assert(0);
        /* if ignore, at least destroy ... */
        SNetRecDestroy( rec);
    }
  } /* MAIN LOOP END */

  SNetStreamClose(instream, true);

  if( nextstream != NULL) {
    SNetStreamClose( nextstream, false);
  }
  SNetStreamClose( outstream, false);

  /* destroy the arg */
  SNetEdestroyList( sarg->guards);
  SNetInfoDestroy( sarg->info);

  snet_variant_t *variant;
  LIST_FOR_EACH(sarg->exit_patterns, variant)
    SNetVariantDestroy(variant);
  END_FOR
  SNetvariantListDestroy( sarg->exit_patterns);
  SNetMemFree( sarg);
} /* STAR BOX TASK END */




/*****************************************************************************/
/* CREATION FUNCTIONS                                                        */
/*****************************************************************************/


/**
 * Convenience function for creating
 * Star, DetStar, StarIncarnate or DetStarIncarnate,
 * dependent on parameters is_incarnate and is_det
 */
static snet_stream_t *CreateStar( snet_stream_t *input,
    snet_info_t *info,
    int location,
    snet_variant_list_t *exit_patterns,
    snet_expr_list_t *guards,
    snet_startup_fun_t box_a,
    snet_startup_fun_t box_b,
    bool is_incarnate,
    bool is_det
    )
{
  snet_stream_t *output;
  star_arg_t *sarg;
  snet_stream_t *newstream;

  input = SNetRouteUpdate(info, input, location);
  if(location == SNetNodeLocation) {
    sarg = (star_arg_t *) SNetMemAlloc( sizeof(star_arg_t));
    sarg->input = input;
    newstream = SNetStreamCreate(0);
    if (!is_incarnate) {
      /* the "top-level" star also creates a collector */
      snet_stream_t **star_output;
      star_output = (snet_stream_t **) SNetMemAlloc( sizeof(snet_stream_t *));
      star_output[0] = newstream;
      output = CollectorCreate( 1, star_output, true, info);
      sarg->output = newstream;
    } else {
      output = newstream;
      sarg->output = newstream;
    }
    sarg->box = box_a;
    sarg->selffun = box_b;
    sarg->exit_patterns = exit_patterns;
    sarg->guards = guards;
    sarg->info = SNetInfoCopy(info);
    sarg->is_incarnate = is_incarnate;
    sarg->is_det = is_det;

    SNetEntitySpawn( ENTITY_STAR, StarBoxTask, (void*)sarg );

  } else {
    SNetEdestroyList( guards);
    snet_variant_t *variant;
    LIST_FOR_EACH(exit_patterns, variant)
      SNetVariantDestroy(variant);
    END_FOR
    SNetvariantListDestroy( exit_patterns);
    output = input;
  }

  return( output);
}




/**
 * Star creation function
 */
snet_stream_t *SNetStar( snet_stream_t *input,
    snet_info_t *info,
    int location,
    snet_variant_list_t *exit_patterns,
    snet_expr_list_t *guards,
    snet_startup_fun_t box_a,
    snet_startup_fun_t box_b)
{
  return CreateStar( input, info, location, exit_patterns, guards, box_a, box_b,
      false, /* not incarnate */
      false /* not det */
      );
}



/**
 * Star incarnate creation function
 */
snet_stream_t *SNetStarIncarnate( snet_stream_t *input,
    snet_info_t *info,
    int location,
    snet_variant_list_t *exit_patterns,
    snet_expr_list_t *guards,
    snet_startup_fun_t box_a,
    snet_startup_fun_t box_b)
{
  return CreateStar( input, info, location, exit_patterns, guards, box_a, box_b,
      true, /* is incarnate */
      false /* not det */
      );
}



/**
 * Det Star creation function
 */
snet_stream_t *SNetStarDet(snet_stream_t *input,
    snet_info_t *info,
    int location,
    snet_variant_list_t *exit_patterns,
    snet_expr_list_t *guards,
    snet_startup_fun_t box_a,
    snet_startup_fun_t box_b)
{
  return CreateStar( input, info, location, exit_patterns, guards, box_a, box_b,
      false, /* not incarnate */
      true /* is det */
      );
}



/**
 * Det star incarnate creation function
 */
snet_stream_t *SNetStarDetIncarnate(snet_stream_t *input,
    snet_info_t *info,
    int location,
    snet_variant_list_t *exit_patterns,
    snet_expr_list_t *guards,
    snet_startup_fun_t box_a,
    snet_startup_fun_t box_b)
{
  return CreateStar( input, info, location, exit_patterns, guards, box_a, box_b,
      true, /* is incarnate */
      true /* is det */
      );
}
