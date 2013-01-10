#include <stdio.h>
#include <stdlib.h>
#include "htab.h"

htab_t *htab_alloc(size_t size) {
  htab_t *table = malloc(sizeof(htab_t) + size * HNAME_LEN * sizeof(char *));
  
  if (table)
    table->tab_s = size;
  else
    table->size = 0;

  return table;
}

void htab_free(htab_t *table) {
  free(table);
}

size_t htab_size(htab_t *table) {
  return table->tab_s;
}

void htab_dump(htab_t *table) {
  size_t i;

  for (i = 0; i < table->tab_s; i++)
    printf("%s\n", table->tab[i]);
}

char *htab_lookup(htab_t *table, size_t index) {
  if (!table || index > table->tab_s)
    return NULL;
  
  return table->tab[index];
}

/**
 * Build host table from file.
 * File format is assumed to be one host per file.
 * However, no check is done!
 */
htab_t *htab_fread(char *tablefile) {
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

  while (!feof(ifp)) //read lines
    fscanf(ifp, "%s", table->tab[i++]);

  fclose(ifp);
  return table;
}

#ifdef HTABTEST
int main(int argc, char ** argv) {
  htab_t *table;
  size_t i;

  table = htab_fread(argv[1]);

  if (table) {
    htab_dump(table);
    
    for (i = 0; i < htab_size(table) + 2 ; i++)
      printf("lookup: %s\n", htab_lookup(table, i));

    htab_free(table);
  } else {
    printf("Unable to allocate table from file.\n");
    exit(EXIT_FAILURE);
  }

  exit(EXIT_SUCCESS);
}
#endif
