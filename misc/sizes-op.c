/* Copyright 2018  Peter Lund <firefly@vax64.dk>

   Licensed under GPL v2.

   ---

   size tester for operand encoder/decoders

   supposed to be compiled to a .o just for the purpose of running 'size' and
   'objdump'.

   almost all the code is copied from src/test-ops.c.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "macros.h"
#include "strret.h"

#include "vax-ucode.h"

/* need the functions to be exported, i.e. not static */
#define STATIC

#include "op-sim-support.h"
#include "op-sim.h"

#include "op-asm-support.h"
#include "op-asm.h"

#include "op-dis-support.h"
#include "op-dis.h"

#include "op-val-support.h"
#include "op-val.h"


int main()
{
	uint8_t		disp8;
	uint16_t	disp16;

	(void) parse_branch8(&disp8, 0);
	(void) parse_branch16(&disp16, 0);
	return EXIT_SUCCESS;
}

