#include <assert.h>

#include "ast.h"
#include "memfun.h"
#include "snetentities.h"
#include "distribution.h"

#include "locvec.h"

/* ------------------------------------------------------------------------- */
/*  SNetSerial                                                               */
/* ------------------------------------------------------------------------- */



/**
 * Serial connector creation function
 */
snet_stream_t *SNetSerialInst(snet_stream_t *input,
    snet_info_t *info,
    int location,
    snet_ast_t *box_a,
    snet_ast_t *box_b)
{
  snet_stream_t *internal_stream;
  snet_stream_t *output;
  snet_locvec_t *locvec;
  bool enterstate;

  locvec = SNetLocvecGet(info);
  enterstate = SNetLocvecSerialEnter(locvec);

  /* create operand A */
  internal_stream = SNetInstantiate(box_a, input, info);

  SNetLocvecSerialNext(locvec);

  /* create operand B */
  output = SNetInstantiate(box_b, internal_stream, info);

  SNetLocvecSerialLeave(locvec, enterstate);

  return(output);
}

snet_ast_t *SNetSerial(int location,
                       snet_startup_fun_t box_a,
                       snet_startup_fun_t box_b)
{
  snet_ast_t *result = SNetMemAlloc(sizeof(snet_ast_t));
  result->location = location;
  result->locvec.type = LOC_SERIAL;
  result->locvec.num = -1;
  result->locvec.parent = NULL;
  result->type = snet_serial;
  result->serial.box_a = box_a(location);
  result->serial.box_a->locvec.num = 1;
  result->serial.box_a->locvec.parent = &result->locvec;
  result->serial.box_b = box_b(location);
  result->serial.box_a->locvec.num = 2;
  result->serial.box_a->locvec.parent = &result->locvec;
  return result;
}
