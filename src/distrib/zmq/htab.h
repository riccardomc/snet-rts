/**
 * Riccardo M. Cefala                       Tue Jan  8 17:09:08 CET 2013
 *
 * Naive host table for ZMQ distribution layer.
 *
 */

#ifndef DISTRIBZMQHTAB_H
#define DISTRIBZMQHTAB_H

#define HTAB_HNAME_LEN 256

typedef struct {
  int data_port;
  int sync_port;
  char *host;
  char *bind;
} htab_host_t;

typedef struct {
  int size;
  htab_host_t **tab;
} htab_t;

//Read htab from file. Returns NULL on failure.
htab_t *htab_fread(char *tablefile);

//Allocate an htab of size size. Returns NULL on failure.
htab_t *htab_alloc(size_t size);

//Destroy the htab.
void htab_free(htab_t *table);

//Returns the size of an htab.
size_t htab_size(htab_t *table);

//Print htab table on stdout.
void htab_dump(htab_t *table);

//Returns the host item at index in tab. Returns NULL on failure.
htab_host_t *htab_lookup(htab_t *table, size_t index);

//Serialise the host item host in buf.
void htab_host_pack(htab_host_t *host, void *buf,
    void (*packFun)(void *, void *, size_t)); 

//Deserialise an host item from buf and return it.
htab_host_t *htab_host_unpack(void *buf, 
    void (*unpackFun)(void *, void *, size_t));

//Serialise the host table table in buf.
  void htab_pack(htab_t *table, void *buf,
    void (*packFun)(void *, void *, size_t)); 

//Deserialise an host table from buf and return it.
htab_t *htab_unpack(void *buf,
    void (*unpackFun)(void *, void *, size_t)); 

char *htab_gethostname(void);
#endif

