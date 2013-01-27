/**
 * Riccardo M. Cefala                       Tue Jan  8 17:09:08 CET 2013
 *
 * Naive host table for ZMQ distribution layer.
 *
 */

#ifndef DISTRIBZMQHTAB_H
#define DISTRIBZMQHTAB_H

#define HTAB_HNAME_LEN 128

typedef struct {
  char host[HTAB_HNAME_LEN];
  char bind[HTAB_HNAME_LEN];
} htab_host_t;

typedef struct {
  size_t tab_size;
  htab_host_t tab[];
} htab_t;

//Read htab from file. Returns NULL on failure.
htab_t *htab_fread(char *tablefile);

//Allocate an htab of size size. Returns NULL on failure.
htab_t *htab_alloc(size_t size);

//Destroy the htab.
void htab_free(htab_t *table);

//Returns the size of an htab.
size_t htab_size(htab_t *table);

//Returns the host item at index in tab. Returns NULL on failure.
htab_host_t *htab_lookup(htab_t *table, size_t index);

#endif

