/* Copyright 2018  Peter Lund <firefly@vax64.dk>

   Licensed under GPL v2.

 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "macros.h"

#include "vax-instr.h"
#include "vax-ucode.h"
#include "dis-uop.h"


int main()
{
	dis_uinstr(0, ARRAY_SIZE(ucode), DIS_CONT, ucode);

	return EXIT_SUCCESS;
}

