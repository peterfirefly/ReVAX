/* Copyright 2018  Peter Lund <firefly@vax64.dk>\

   Licensed under GPL v2.

   ---

   test input/output of (IEEE) floating-point numbers using normal floating-
   point operations.

   I realized that I simply didn't understand the problem well enough so this
   is intended to give me more experience.

   Simple implementations of fp conversions to/from decimal will be bad -- the
   problem is much, much harder than it looks.

   We want the nearest possibly value (with the right rounding mode!) that
   also round-trips and has the shortest possible decimal representation.

   Many implementations don't round-trip, are over-long, don't give the
   nearest value, don't respect rounding mode -- or worse!

   Converting to decimal representation (output):
      Jon White & Guy Steele, Dragon4
      Florian Loitsch, Grisu (skip Dragon4 in 99.4% of cases)
   
   Converting from decimal representation (input):
      William Clinger, Bellerophon
      David Gay, strtod.c

   Dragon4 needs really big integers internally -- more than 1000 bits in some
   cases!

   Links/references:
     http://www.ryanjuckett.com/programming/printing-floating-point-numbers/
     http://www.exploringbinary.com/
     http://www.mpfr.org/          Multi-precision Floating-point Reliable library

   Keep this file around for explanatory purposes and to make further experimentation
   easy.

   ----

   gcc -O2 -W -Wall -Isrc misc/fp-ieee.c -lmpfr -lgmp -o fp-ieee

   (mpfr uses gmp internally, fp-ieee.c includes src/shared.h)

 */

#include <assert.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <mpfr.h>

#include "shared.h"


struct str_ret output_fp(double x)
{
	struct str_ret	buf = {};
	mpfr_t		bigx;

	mpfr_clear_flags();

	mpfr_init2(bigx, 53);
	mpfr_set_d(bigx, x, MPFR_RNDN);

	mpfr_sprintf(buf.str, "%Rf %Rg", bigx, bigx);
	mpfr_clear(bigx);
	return buf;
}


void input_fp(const char *s)
{
	mpfr_t		x;

	mpfr_init2(x, 53);
	mpfr_set_str(x, s, 10, MPFR_RNDN);

	mpfr_printf("val : %Rg\n", x);
	mpfr_printf("      %Rb\n", x); /* bin */
	mpfr_printf("      %Ra\n", x); /* hex */
	printf("sign: %d\n", mpfr_signbit(x));
	mpfr_printf("prec: %Pd\n", x[0]._mpfr_prec);
	mpfr_printf("exp:  %d\n", x[0]._mpfr_exp);
	mpfr_printf("limb: %Mx\n", *x[0]._mpfr_d);
mpfr_set_d(x, 1.0, MPFR_RNDN);
	mpfr_printf("limb: %Mx\n", *x[0]._mpfr_d);

	mpfr_clear(x);
}


int main()
{
	printf("MPFR library: %-12s\n", mpfr_get_version());
	printf("MPFR header:  %s (based on %d.%d.%d)\n",
		MPFR_VERSION_STRING,
		MPFR_VERSION_MAJOR, MPFR_VERSION_MINOR, MPFR_VERSION_PATCHLEVEL);
	printf("TLS: %d\n", mpfr_buildopt_tls_p());
	printf("buildopt_tune_case: %s\n", mpfr_buildopt_tune_case());
	printf("MPFR_PREC_MAX: %ld\n", MPFR_PREC_MAX);
	printf("bits per limb: %d\n", mp_bits_per_limb);

	printf("\n");
	mpfr_clear_flags();

	printf("%-8g - %20a: '%s'\n",      0.0,      0.0, output_fp(     0.0).str);
	printf("%-8g - %20a: '%s'\n",     -0.0,     -0.0, output_fp(    -0.0).str);
	printf("%-8g - %20a: '%s'\n",      1.0,      1.0, output_fp(     1.0).str);
	printf("%-8g - %20a: '%s'\n",      3.0,      3.0, output_fp(     3.0).str);
	printf("%-8g - %20a: '%s'\n",     -3.0,     -3.0, output_fp(    -3.0).str);
	printf("%-8g - %20a: '%s'\n",     10.0,     10.0, output_fp(    10.0).str);
	printf("%-8g - %20a: '%s'\n",    100.0,    100.0, output_fp(   100.0).str);
	printf("%-8g - %20a: '%s'\n",   1000.0,   1000.0, output_fp(  1000.0).str);
	printf("%-8g - %20a: '%s'\n",  10000.0,  10000.0, output_fp( 10000.0).str);
	printf("%-8g - %20a: '%s'\n", 100000.0, 100000.0, output_fp(100000.0).str);
	printf("%-8g - %20a: '%s'\n",      1.2,      1.2, output_fp(     1.2).str);
	printf("%-8g - %20a: '%s'\n",      1.9,      1.9, output_fp(     1.9).str);
	printf("%-8g - %20a: '%s'\n", 3.1415926535, 3.1415926535, output_fp(3.1415926535).str);
	printf("%-8g - %20a: '%s'\n",  100.123,  100.123, output_fp( 100.123).str);

	printf("\n");

	input_fp("100.123");

	printf("\n");
	printf("underflow: %d\n", mpfr_underflow_p());
	printf("overflow:  %d\n", mpfr_overflow_p());
	printf("divby0:    %d\n", mpfr_divby0_p());
	printf("nanflag:   %d\n", mpfr_nanflag_p());
	printf("inexflag:  %d\n", mpfr_inexflag_p());
	printf("erangeflag:%d\n", mpfr_erangeflag_p());	

	mpfr_free_cache();

	return EXIT_SUCCESS;
}

