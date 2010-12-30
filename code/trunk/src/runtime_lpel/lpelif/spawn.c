/* $Id$ */


#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include "snetentities.h"

#include "spawn.h"
#include "assignment.h"

#include "lpel.h"



static FILE *mapfile = NULL;



void SNetSpawnInit( int node)
{
  char fname[20+1];
  if (node < 0) {
    snprintf(fname, 20, "tasks.map");
  } else {
    snprintf(fname, 20, "n%02d_tasks.map", node);
  }
  /* create a map file */
  mapfile = fopen(fname, "w");
}

void SNetSpawnDestroy( void)
{
  (void) fclose( mapfile);
}


/**
 * Create a task on a wrapper thread
 */
void SNetSpawnWrapper( lpel_taskfunc_t taskfunc, void *arg,
    char *name)
{
  lpel_taskreq_t *t;
  int flags = LPEL_TASK_ATTR_NONE;
  int stacksize = 0;

  t = LpelTaskRequest(taskfunc, arg, flags, stacksize);
  return LpelWorkerWrapperCreate( t, name);
}



void SNetSpawnEntity( lpel_taskfunc_t fun, void *arg,
  snet_entity_id_t id, char *label)
{
  lpel_taskreq_t *t;
  int flags = LPEL_TASK_ATTR_NONE;
  int stacksize = 0;
  int wid;
  unsigned int tid;

  /* monitoring */
  if (id!=ENTITY_box) {
    flags &= ~LPEL_TASK_ATTR_MONITOR_OUTPUT;
  }

  /* stacksize */
  if (id==ENTITY_box) {
    stacksize = 8*1024*1024; /* 8 MB */
  } else {
    stacksize = 256*1024; /* 256 kB */
  }

  /* create task */
  t = LpelTaskRequest( fun, arg, flags, stacksize);
  tid = LpelTaskReqGetUID( t);

  
  switch(id) {
    case ENTITY_parallel_nondet:
    case ENTITY_parallel_det:
      label = "<parallel>"; break;
    case ENTITY_star_nondet:
    case ENTITY_star_det:
      label = "<star>"; break;
    case ENTITY_split_nondet:
    case ENTITY_split_det:
      label = "<split>"; break;
      break;
    case ENTITY_sync:
      label = "<sync>"; break;
    case ENTITY_filter:
      label = "<filter>"; break;
    case ENTITY_collect:
      label = "<collector>"; break;
    default:
      break;
  }

  if (flags & LPEL_TASK_ATTR_MONITOR_OUTPUT) {
    (void) fprintf(mapfile, "%u: %s\n", tid, label);
  }

  /* Query assignment module */
  wid = AssignmentGetWID(t, id==ENTITY_box);
  
  /* call worker assignment */
  LpelWorkerTaskAssign( t, wid);
}

