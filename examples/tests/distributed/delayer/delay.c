#include <delay.h>
#include <stdlib.h>
#include <bool.h>
#include <stdio.h>

void *delay( void *hnd, c4snet_data_t *x)
{
  int int_x;
  c4snet_data_t *result;

  int_x= *(int *)C4SNetGetData( x);

  sleep(int_x);

  result = C4SNetCreate( CTYPE_int, 1, &int_x);

  C4SNetFree(x);

  C4SNetOut( hnd, 1, result);
  return( hnd);
}

