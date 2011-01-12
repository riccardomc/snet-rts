#include <stdlib.h>
#include <assert.h>

#include "collector.h"
#include "snetentities.h"
#include "memfun.h"
#include "record_p.h"
#include "bool.h"
#include "debug.h"

#include "spawn.h"
#include "lpel.h"


typedef struct {
  lpel_stream_t *output;
  lpel_stream_t **inputs;
  int num;
} coll_arg_t;



/**
 * @pre rec1 and rec2 are sort records
 */
static bool SortRecEqual( snet_record_t *rec1, snet_record_t *rec2)
{
  return (SNetRecGetLevel(rec1) == SNetRecGetLevel(rec2)) &&
         (SNetRecGetNum(  rec1) == SNetRecGetNum(  rec2)); 
}


/**
 * Collector Task
 */
void CollectorTask( lpel_task_t *self, void *arg)
{
  coll_arg_t *carg = (coll_arg_t *)arg;
  lpel_stream_list_t readyset, waitingset;
  lpel_stream_iter_t *iter, *wait_iter;
  lpel_stream_desc_t *outstream;
  snet_record_t *sort_rec = NULL;
  snet_record_t *term_rec = NULL;
  bool terminate = false;

  /* open outstream for writing */
  outstream = LpelStreamOpen( self, carg->output, 'w');

  readyset = waitingset = NULL;
  {
    int i;
    /* fill initial readyset of collector */
    for (i=0; i<carg->num; i++) {
      lpel_stream_desc_t *tmp;
      /* open each stream in listening set for reading */
      tmp = LpelStreamOpen( self, carg->inputs[i], 'r');
      /* add each stream instreams[i] to listening set of collector */
      LpelStreamListAppend( &readyset, tmp);
    }
    SNetMemFree( carg->inputs );
  }

  /* create an iterator for both sets, is reused within main loop*/
  iter = LpelStreamIterCreate( &readyset);
  wait_iter = LpelStreamIterCreate( &waitingset);
  
  /* MAIN LOOP */
  while( !terminate) {

    /* iterate through readyset: for each node (stream) */
    LpelStreamIterReset( &readyset, iter);
    while( LpelStreamIterHasNext(iter)) {
      lpel_stream_desc_t *cur_stream = LpelStreamIterNext(iter);
      bool do_next = false;
  
      /* process stream until empty or next sort record or empty */
      while ( !do_next && LpelStreamPeek( cur_stream) != NULL) {
        snet_record_t *rec = LpelStreamRead( cur_stream);

        switch( SNetRecGetDescriptor( rec)) {

          case REC_sort_end:
            /* sort record: place node in waitingset */
            /* remove node */
            LpelStreamIterRemove(iter);
            /* put in waiting set */
            LpelStreamListAppend( &waitingset, cur_stream);

            if (sort_rec!=NULL) {
              /* assure that level & counter match */
              if( !SortRecEqual(rec, sort_rec) ) {
                SNetUtilDebugNotice(
                    "[COLL] Warning: Received sort records do not match!");
              }
              /* destroy record */
              SNetRecDestroy( rec);
            } else {
              sort_rec = rec;
            }
            /* end processing this stream */
            do_next = true;
            break;

          case REC_data:
            /* data record: forward to output */
            LpelStreamWrite( outstream, rec);
            break;

          case REC_sync:
            /* sync: replace stream */
            LpelStreamReplace( cur_stream, (lpel_stream_t*) SNetRecGetStream( rec));
            /* destroy record */
            SNetRecDestroy( rec);
            break;

          case REC_collect:
            /* collect: add new stream to ready set */
#if 1
            /* but first, check if we can free resources by checking the
               waiting set for arrival of termination records */
            if ( !LpelStreamListIsEmpty( &waitingset)) {
              LpelStreamIterReset( &waitingset, wait_iter);
              while( LpelStreamIterHasNext( wait_iter)) {
                lpel_stream_desc_t *sd = LpelStreamIterNext( wait_iter);
                snet_record_t *wait_rec = LpelStreamPeek( sd);

                /* for this stream, check if there is a termination record next */
                if ( wait_rec != NULL &&
                    SNetRecGetDescriptor( wait_rec) == REC_terminate ) {
                  /* consume, remove from waiting set and free the stream */
                  (void) LpelStreamRead( sd);
                  LpelStreamIterRemove( wait_iter);
                  LpelStreamClose( sd, true);
                  /* destroy record */
                  SNetRecDestroy( wait_rec);
                }
                /* else do nothing */
              }
            }
#endif
            /* finally, add new stream to ready set */
            {
              /* new stream */
              lpel_stream_desc_t *new_sd = LpelStreamOpen( self,
                  (lpel_stream_t*) SNetRecGetStream( rec), 'r');
              /* add to readyset (via iterator: after current) */
              LpelStreamIterAppend( iter, new_sd);
            }
            /* destroy collect record */
            SNetRecDestroy( rec);
            break;

          case REC_terminate:
            /* termination record: close stream and remove from ready set */
            if ( !LpelStreamListIsEmpty( &waitingset)) {
              SNetUtilDebugNotice("[COLL] Warning: Termination record "
                  "received while waiting on sort records!\n");
            }
            LpelStreamIterRemove( iter);
            LpelStreamClose( cur_stream, true);
            if (term_rec==NULL) {
              term_rec = rec;
            } else {
              /* destroy record */
              SNetRecDestroy( rec);
            }
            /* stop processing this stream */
            do_next = true;
            break;

          default:
            assert(0);
            /* if ignore, at least destroy ... */
            SNetRecDestroy( rec);
        } /* end switch */
      } /* end current stream*/
    } /* end for each stream in ready */

    /* one iteration finished through readyset */
    
    /* check if all sort records are received */
    if ( LpelStreamListIsEmpty( &readyset)) {
      
      if ( !LpelStreamListIsEmpty( &waitingset)) {
        /* "swap" sets */
        readyset = waitingset;
        waitingset = NULL;
        /* check sort record: if level > 0,
           decrement and forward to outstream */
        assert( sort_rec != NULL);
        {
          int level = SNetRecGetLevel( sort_rec);
          if( level > 0) {
            SNetRecSetLevel( sort_rec, level-1);
            LpelStreamWrite( outstream, sort_rec);
          } else {
            SNetRecDestroy( sort_rec);
          }
          sort_rec = NULL;
        }
      } else {
        /* send a terminate record */
        LpelStreamWrite( outstream, term_rec);
        /* waitingset is also empty: terminate*/
        terminate = true;
      }

    } else {
      /* readyset is not empty: continue "collecting" sort records */
      /* wait on new input */
      (void) LpelStreamPoll( &readyset);
    }

  } /* MAIN LOOP END*/

  /* close outstream */
  LpelStreamClose( outstream, false);

  /* destroy iterator */
  LpelStreamIterDestroy( iter);

  SNetMemFree( carg);

} /* Collector task end*/




/**
 * Collector creation function
 * @pre num >= 1
 */
snet_stream_t *CollectorCreate( int num, snet_stream_t **instreams, snet_info_t *info)
{
  snet_stream_t *outstream;
  coll_arg_t *carg;

  /* create outstream */
  outstream = (snet_stream_t*) LpelStreamCreate();

  /* create collector handle */
  carg = (coll_arg_t *) SNetMemAlloc( sizeof( coll_arg_t));
  carg->inputs = (lpel_stream_t**) instreams;
  carg->output = (lpel_stream_t* ) outstream;
  carg->num = num;

  /* spawn collector task */
  SNetSpawnEntity( CollectorTask, (void *)carg, ENTITY_collect, NULL);
  return outstream;
}

