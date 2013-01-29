#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include "htab.h"


htab_host_t *htab_host_alloc() {
  int i;
  htab_host_t *host = malloc(sizeof(htab_host_t));
  if (host) {
    host->host = malloc(sizeof(char) * HTAB_HNAME_LEN);
    host->bind = malloc(sizeof(char) * HTAB_HNAME_LEN);
    for (i = 0; i < HTAB_HNAME_LEN; i++) {
      host->host[i] = '\0';
      host->bind[i] = '\0';
    }
    host->data_port = 0;
    host->sync_port = 0;
  }
  return host;
}

htab_t *htab_alloc(size_t size) 
{
  int i;
  htab_t *table = malloc(sizeof(htab_t));
  if (table) {
    table->size = size;
    table->tab = malloc(sizeof(htab_host_t *) * size);
    if (table->tab) {
      for (i = 0; i < size; i++) {
        table->tab[i] = htab_host_alloc();
      }
    }
  }
  return table;
}

void htab_host_free(htab_host_t *host) {
  free(host->host);
  free(host->bind);
  free(host);
  host = NULL;
}

void htab_free(htab_t *table) 
{
  int i;
  for (i = 0; i < table->size; i++) {
    htab_host_free(table->tab[i]);
  }
  free(table->tab);
  free(table);
  table = NULL;
}

size_t htab_size(htab_t *table) 
{
  return table->size;
}

void htab_dump(htab_t *table) 
{
  size_t i;

  for (i = 0; i < table->size; i++)
    printf("%s %s %d %d\n", table->tab[i]->host,
                      table->tab[i]->bind,
                      table->tab[i]->data_port,
                      table->tab[i]->sync_port);
}

htab_host_t *htab_lookup(htab_t *table, size_t index) 
{
  if (!table || index > table->size)
    return NULL;
  
  return table->tab[index];
}

htab_t *htab_fread(char *tablefile) 
{
  int ch;
  size_t i = 0;
  htab_t *table = NULL;

  FILE *ifp = fopen(tablefile, "r");

  if (!ifp) 
    return NULL;

  while (!feof(ifp)) { //count lines
    ch = fgetc(ifp);
    if (ch == '\n') 
      i++;
  }

  table = htab_alloc(i);

  if (!table)
    return NULL;

  i = 0;
  rewind(ifp);

  while (!feof(ifp)) {
    //FIXME: do some decent parsing!
    fscanf(ifp, "%s %s %d %d\n", table->tab[i]->host,
                           table->tab[i]->bind,
                           &table->tab[i]->data_port,
                           &table->tab[i++]->sync_port);
  }

  fclose(ifp);
  return table;
}

void htab_host_pack(htab_host_t *host, void *buf,
    void (*packInt)(void *, int, int *),
    void (*packByte)(void *, int, char *))
{
  packInt(buf, 1, &host->data_port);
  packInt(buf, 1, &host->sync_port);
  packByte(buf, HTAB_HNAME_LEN, host->host);
  packByte(buf, HTAB_HNAME_LEN, host->bind);
}

htab_host_t *htab_host_unpack(void *buf,
    void (*unpackInt)(void *, int, int *), 
    void (*unpackByte)(void *, int, char *)) 
{
  htab_host_t *host = htab_host_alloc();

  unpackInt(buf, 1, &host->data_port);
  unpackInt(buf, 1, &host->sync_port);
  unpackByte(buf, HTAB_HNAME_LEN, host->host);
  unpackByte(buf, HTAB_HNAME_LEN, host->bind);

  return host;
}

void htab_pack(htab_t *table, void *buf,
    void (*packInt)(void *, int, int *), 
    void (*packByte)(void *, int, char *)) 
{
  int i;

  packInt(buf, 1, &table->size);
  for (i = 0; i < htab_size(table); i++) {
    htab_host_pack(table->tab[i], buf, packInt, packByte);
  }

}

htab_t *htab_unpack(void *buf,
    void (*unpackInt)(void *, int, int *),
    void (*unpackByte)(void *, int, char *))
{
  int size, i;
  htab_t *table;

  unpackInt(buf, 1, &size);
  table = htab_alloc(size);

  for (i = 0; i < size; i++) {
    unpackInt(buf, 1, &table->tab[i]->data_port);
    unpackInt(buf, 1, &table->tab[i]->sync_port);
    unpackByte(buf, HTAB_HNAME_LEN, table->tab[i]->host);
    unpackByte(buf, HTAB_HNAME_LEN, table->tab[i]->bind);
  }

  return table;
}

char *htab_gethostname()
{
  //FIXME: consider using getaddrinfo
  char *hostname = malloc(sizeof(char) * 256);
  struct hostent* host;

  hostname[255] = '\0';
  gethostname(hostname, 255);
  host = gethostbyname(hostname);
  strcpy(hostname, host->h_name);

  return hostname;
}
