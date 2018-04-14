/* Copyright 2018  Peter Lund <firefly@vax64.dk>

   Licensed under GPL v2.

   ---

   VAX instruction decoder.  Takes a list of VAX instructions as input and
   outputs a corresponding list of µops.

 */

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "string-utils.h"
#include "parse.h"

#include "vax-instr.h"

#include "fp.h"

#define STATIC static
#include "op-asm-support.h"
#include "op-asm.h"

#include "op-sim-support.h"
#include "op-sim.h"

#include "op-val-support.h"
#include "op-val.h"

/***/



/***/

static void help()
{
		fprintf(stderr,
"uop <source>\n"
"\n"
"  inputs a VAX assembly file.\n"
"\n"
"  outputs a list of µops.\n");
}


static void help_exit()
{
	help();
	exit(1);
}


int main(int argc, char *argv[argc])
{
}

