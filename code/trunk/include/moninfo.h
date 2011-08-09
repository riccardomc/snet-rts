/*
 * moninfo.h
 *
 *  Created on: Jul 29, 2011
 *      Author: Raimund Kirner
 * This file includes data types and functions to maintain monitoring information.
 * The main purpose at creation (July 2011) was to have monitoring information for
 * each S-Net record to log the overall execution time in a (sub)network from reading
 * a record until the output of all of its descendant records at the exit of this
 * (sub)network.
 *
 */

#ifndef _MONINFO_H_
#define _MONINFO_H_

#include <stdio.h>

#include "bool.h"
#include "record.h"


#define MONINFO_USE_RECORD_EVENTS  /* enable processing of record events */

typedef enum moninfo_event snet_moninfo_event_t;
typedef enum moninfo_descr snet_moninfo_descr_t;
typedef union moninfo_types snet_moninfo_types_t;

/* macros for monitoring information */
#define MONINFO_DESCR( name) name->mon_descr
#define MONINFO_EVENT( name) name->mon_event
#define MONINFOPTR( name) name->mon_data

#define REC_MONINFO( name, component) MONINFOPTR( name) ->moninfo_rec.component


enum moninfo_event {
  EV_INPUT_ARRIVE=0,
  EV_BOX_START,
  EV_BOX_WRITE,
  EV_BOX_FINISH, /* not yet used, because its not a record event */
  EV_FILTER_START,
  EV_FILTER_WRITE,
  EV_SYNC_FIRST,
  EV_SYNC_NEXT,
  EV_SYNC_FIRE,
};

enum moninfo_descr {
  MON_RECORD=0,
};


/* data structure of additional monitoring information of records (e.g., shape, etc.) */
typedef char* snet_add_moninfo_rec_data_t;

/* data structure of monitoring information for records */
typedef struct {
  snet_record_id_t id;
  snet_recid_list_t *parent_ids;
  snet_add_moninfo_rec_data_t add_moninfo_rec_data; /* container for additional arbitrary data */
} snet_moninfo_record_t;


/* enum of different monitoring info structures */
union moninfo_types {
  snet_moninfo_record_t moninfo_rec;
};


/* data structure of monitoring information */
typedef struct snet_moninfo {
  snet_moninfo_event_t mon_event;
  snet_moninfo_descr_t mon_descr;
  snet_moninfo_types_t *mon_data;
} snet_moninfo_t;


/*****************************************************************************
 * Create monitoring information (entries depend on monitoring item)
 ****************************************************************************/
snet_moninfo_t *SNetMonInfoCreate ( snet_moninfo_event_t event, snet_moninfo_descr_t descr,... );


/*****************************************************************************
 * Delete monitoring information (entries depend on monitoring item)
 ****************************************************************************/
void SNetMonInfoDestroy( snet_moninfo_t *mon);




/*****************************************************************************
 * Create a copy of the additional data of record monitoring information
 ****************************************************************************/
snet_add_moninfo_rec_data_t SNetMonInfoRecCopyAdditionalData(snet_add_moninfo_rec_data_t add_data);



/*****************************************************************************
 *  Print the monitoring information to a file
 ****************************************************************************/
void SNetMonInfoPrint(FILE *f, snet_moninfo_t *moninfo);

#endif /* _MONINFO_H_ */