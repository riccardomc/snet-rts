#include <stdio.h>
#include <stdlib.h>
#include "htab.h"

htab_t *htab_alloc(size_t size) 
{
  htab_t *table = malloc(sizeof(htab_t) + size * sizeof(htab_host_t));

  if (table)
    table->tab_size = size;
  else
    table->tab_size = 0;

  return table;
}

void htab_free(htab_t *table) 
{
  free(table);
}

size_t htab_size(htab_t *table) 
{
  return table->tab_size;
}

void htab_dump(htab_t *table) 
{
  size_t i;

  for (i = 0; i < table->tab_size; i++)
    printf("%s %s\n", table->tab[i].host,
                        table->tab[i].bind);
}

htab_host_t *htab_lookup(htab_t *table, size_t index) 
{
  if (!table || index > table->tab_size)
    return NULL;
  
  return &table->tab[index];
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
    fscanf(ifp, "%s %s\n", table->tab[i].host,
                            table->tab[i++].bind);
  }

  fclose(ifp);
  return table;
}

