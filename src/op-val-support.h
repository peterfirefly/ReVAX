/* Copyright 2018  Peter Lund <firefly@vax64.dk>

   Licensed under GPL v2.

   ---

   operand handling support code -- validation

   called by autogenerated code for val.

 */

#ifndef OP_VAL_SUPPORT__H
#define OP_VAL_SUPPORT__H

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "op-support.h"


/***/



/* check functions, called from src/op-val.h */

/* check that Rn access won't spill over to r15 */
static bool check_reg(struct fields fields, int width)
{
//	fprintf(stderr, "[Rn=%d width=%d]\n", fields.Rn, width);
	return fields.Rn + (width + 3) / 4 + 1 < 15;
}


/* check Rn != Rx, check that Rx != r15 */
static bool check_idx(struct fields fields, int width)
{
	(void) width;

	return fields.Rn != fields.Rx;
}


#endif

