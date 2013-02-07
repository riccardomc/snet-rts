#include "snetentities.h"

/* ------------------------------------------------------------------------- */
/*  SNetAlias                                                                */
/* ------------------------------------------------------------------------- */
snet_ast_t *SNetAlias(int location,
                      snet_startup_fun_t net)
{
  return net(location);
}
