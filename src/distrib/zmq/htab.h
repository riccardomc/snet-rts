/**
 * Riccardo M. Cefala                       Tue Jan  8 17:09:08 CET 2013
 *
 * A very naive integer to host table. Not much more than an array.
 *
 */

#ifndef DISTRIBZMQHTAB_H
#define DISTRIBZMQHTAB_H

#define HNAME_LEN 256 

typedef struct {
  size_t tab_size;
  char tab[][HNAME_LEN];
} htab_t;

//Read htab from file. Returns NULL on failure.
htab_t *htab_fread(char *tablefile);

//Allocate an htab of size size. Returns NULL on failure.
htab_t *htab_alloc(size_t size);

//Destroy the htab.
void htab_free(htab_t *table);

//Returns the size of an htab.
size_t htab_size(htab_t *table);

//Returns the host at index in tab. Returns NULL on failure.
char *htab_lookup(htab_t *table, size_t index);

#endif
