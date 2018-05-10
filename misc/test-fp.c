/* Copyright 2018  Peter Lund <firefly@vax64.dk>

   Licensed under GPL v2.

   ---

   Test VAX fp input/output conversion routines

   VAX fp uses a hidden bit.  MPFR doesn't, so we must make sure to set it to 1
   when converting in one direction and to ignore it when converting in the
   other.

   Neither VAX nor mpfr support denormal numbers.

   MPFR supports +/- INF, NaN, which the VAX didn't (it only supported a single
   kind of "reserved" value, when expfield = 0, s=1).

   VAX fp's are 0 if expfield = 0, S=0.  If fraction = 0, it is a clean zero,
   otherwise it is a "dirty" zero.

   MPFR fp's are 0 (or +/-INF or NaN) if the exponent field has a special value.
   We test that in a portable way with mpfr_zero_p() -- I think 'p' stands for
   'predicate'.

   ---

   gcc -O2 -W -Wall -fsanitize=undefined -Isrc misc/test-fp.c -lmpfr -lgmp -o test-fp

   (mpfr uses gmp internally)

 */

/* necessary for mrand48().

   (If the compiler is invoked with -std=gnu99 -- or if it defaults to that
   -- then this #define is unnecessary.
   It has to be there if the compiler is invoked with -std=c99.)
 */
#define _XOPEN_SOURCE

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <mpfr.h>

#include "macros.h"

#include "big-int.h"

#include "fp.h"

/***/

/***/

/* Lots of subtleties here.

   No need to check for 'from' incrementing too far and overflowing (or decrementing
   too far and underflowing) because if 'from' already has the max/min magnitude
   than 'to' has to be equal to 'from'... [is this correct?]

   Musl-libc's implementation for inspiration/reference:
   https://git.musl-libc.org/cgit/musl/tree/src/math/nextafter.c

   Things are a bit different here because we have no access to an underlying
   VAX implementation the way Musl can access an underlying IEEE implementation.
   VAX fp doesn't have +/-INF which helps.  It also doesn't have denorms, which
   in this case makes things easier for IEEE (there's no large jump from 0
   to the next lowest magnitude + there's only one 0 whereas VAX fp has lots of
   "dirty zeros".).

   ----

   from == to:  to (or from)		// doesn't catch reserved and 0


   from			to 		result
   ---------------------------------------------
   reserved		-		reserved		// more than one reserved
   -			reserved	reserved		// more than one reserved
   reserved		reserved	reserved		// more than one reserved


   from			to 		result
   ---------------------------------------------
   0			0		0			// more than one 0
   0			0.--------	0.00001.0000000		// more than one 0, note: no denorms
   0			1.--------	1.00001.0000000		// more than one 0, note: no denorms
   0.00001.00000000	0		0			// more than one 0, note: no denorms
   1.00001.00000000	0		0			// more than one 0, note: no denorms


   from			to 		result
   ---------------------------------------------
   x.---------	   >	x.---------	from--
   x.---------     <    x.---------	from++
   0.---------		1.---------	from--			// different sides of 0, move towards zero
   1.---------		0.---------	from--			// different sides of 0, move towards zero


   Careful with from--... if from is x.00001.000000 we have to return 0 instead
   of from--.

   ----

   This was an experiment.  Harder to write and harder to generalize to other
   sizes than 'f' than expected.  Not exactly code I can write (or test) late
   at night.

   The code for d/g/h was written quickly, based on copying from the f code.
   No errors discovered in d/g/h durings tests.

 */
struct big_int fp_nextafter_f(struct big_int from, struct big_int to)
{
	/* reserved values? */
	if (((from.val[0] & 0xFF80) == 0x8000) || ((to.val[0] & 0xFF80) == 0x8000))
		return (struct big_int) {.val[0] = 0x8000};	/* reserved */

	/* 0? */
	if ((from.val[0] & 0xFF80) == 0) { /* from == 0 */
		if ((to.val[0] & 0xFF80) == 0)	/* to == 0 */
			return (struct big_int) {.val[0] = 0};	/* 0 */
		else
			return (struct big_int) {.val[0] = (1 << 7) | (to.val[0] & 0x8000)}; /* x.00001.000000 */
	}

	if ((to.val[0] & 0xFF80) == 0) /* 0 */
		if ((from.val[0] & 0xFFFF7FFF) == 0x00000080)	/* x.00001.000000 */
			return (struct big_int) {.val[0] = 0};	/* 0 */

	/* normal numbers from here on... */

	/* this equality comparison compares *bit patterns* so it won't catch
	   all cases of two reserved values or two 0.0 values.
	 */
	if (from.val[0] == to.val[0])
		return to;

	/* proper word order like God intended... */
	from.val[0] = (from.val[0] >> 16) | (from.val[0] << 16);
	to.val[0]   = (to.val[0]   >> 16) | (to.val[0]   << 16);

	/* if the numbers are on different sides of zero, move 'from' towards
	   zero.  Since the numbers are sign-magnitude, that's a decrement.
	 */
	if (((from.val[0] ^ to.val[0]) & (1U << 31)) ||
	    ((from.val[0] & 0x7FFFFFFF) > (to.val[0] & 0x7FFFFFFF))) {
		if ((from.val[0] & 0x7FFFFFFF) == 0x00800000)	/* x.00001.000000 */
			return (struct big_int) {.val[0] = 0};	/* 0 */
		else
			from.val[0]--;
	} else
		from.val[0]++;

	/* swap back to silly PDP-11 order */
	from.val[0] = (from.val[0] >> 16) | (from.val[0] << 16);

	/* done! */
	return from;
}


struct big_int fp_nextafter_d(struct big_int from, struct big_int to)
{
	/* reserved values? */
	if (((from.val[0] & 0xFF80) == 0x8000) || ((to.val[0] & 0xFF80) == 0x8000))
		return (struct big_int) {.val[0] = 0x8000};	/* reserved */

	/* 0? */
	if ((from.val[0] & 0xFF80) == 0) { /* from == 0 */
		if ((to.val[0] & 0xFF80) == 0)	/* to == 0 */
			return (struct big_int) {.val[0] = 0};	/* 0 */
		else
			return (struct big_int) {.val[0] = (1 << 7) | (to.val[0] & 0x8000)}; /* x.00001.00000 */
	}

	if ((to.val[0] & 0xFF80) == 0) /* 0 */
		if ((from.val[0] & 0xFFFF7FFF) == 0x00000080)	/* x.00001.000000 */
			return (struct big_int) {.val[0] = 0};	/* 0 */

	/* normal numbers from here on... */

	/* this equality comparison compares *bit patterns* so it won't catch
	   all cases of two reserved values or two 0.0 values.
	 */
	if ((from.val[0] == to.val[0]) && (from.val[1] == to.val[1]))
		return to;

	/* proper word order like God intended... */
	from.val[0] = (from.val[0] >> 16) | (from.val[0] << 16);
	from.val[1] = (from.val[1] >> 16) | (from.val[1] << 16);
	to.val[0]   = (to.val[0]   >> 16) | (to.val[0]   << 16);
	to.val[1]   = (to.val[1]   >> 16) | (to.val[1]   << 16);

	/* if the numbers are on different sides of zero, move 'from' towards
	   zero.  Since the numbers are sign-magnitude, that's a decrement.
	 */
	if (((from.val[0] ^ to.val[0]) & (1 << 31)) ||		/* different sign */

	    ((from.val[0] & 0x7FFFFFFF) >  (to.val[0] & 0x7FFFFFFF)) ||	/* |from| > |to| */
	   (((from.val[0] & 0x7FFFFFFF) == (to.val[0] & 0x7FFFFFFF)) &&
	    ((from.val[1]               >   to.val[1]             )))) {
		if (((from.val[0] & 0x7FFFFFFF) == 0x00800000) &&	/* x.00001.000000 */
		     (from.val[1] == 0))
			return (struct big_int) {.val[0] = 0};	/* 0 */
		else {
			from.val[1]--;
			if (from.val[1] == 0xFFFFFFFF)
				from.val[0]--;
		}
	} else {
		from.val[1]++;
		if (from.val[1] == 0)
			from.val[0]++;
	}

	/* swap back to silly PDP-11 order */
	from.val[0] = (from.val[0] >> 16) | (from.val[0] << 16);
	from.val[1] = (from.val[1] >> 16) | (from.val[1] << 16);

	/* done! */
	return from;
}


struct big_int fp_nextafter_g(struct big_int from, struct big_int to)
{
	/* reserved values? */
	if (((from.val[0] & 0xFFF0) == 0x8000) || ((to.val[0] & 0xFFF0) == 0x8000))
		return (struct big_int) {.val[0] = 0x8000};	/* reserved */

	/* 0? */
	if ((from.val[0] & 0xFFF0) == 0) { /* from == 0 */
		if ((to.val[0] & 0xFFF0) == 0)	/* to == 0 */
			return (struct big_int) {.val[0] = 0};	/* 0 */
		else
			return (struct big_int) {.val[0] = (1 << 4) | (to.val[0] & 0x8000)}; /* x.00001.00000 */
	}

	if ((to.val[0] & 0xFFF0) == 0) /* 0 */
		if ((from.val[0] & 0xFFFF7FFF) == 0x00000010)	/* x.00001.000000 */
			return (struct big_int) {.val[0] = 0};	/* 0 */

	/* normal numbers from here on... */

	/* this equality comparison compares *bit patterns* so it won't catch
	   all cases of two reserved values or two 0.0 values.
	 */
	if ((from.val[0] == to.val[0]) && (from.val[1] == to.val[1]))
		return to;

	/* proper word order like God intended... */
	from.val[0] = (from.val[0] >> 16) | (from.val[0] << 16);
	from.val[1] = (from.val[1] >> 16) | (from.val[1] << 16);
	to.val[0]   = (to.val[0]   >> 16) | (to.val[0]   << 16);
	to.val[1]   = (to.val[1]   >> 16) | (to.val[1]   << 16);

	/* if the numbers are on different sides of zero, move 'from' towards
	   zero.  Since the numbers are sign-magnitude, that's a decrement.
	 */
	if (((from.val[0] ^ to.val[0]) & (1 << 31)) ||		/* different sign */

	    ((from.val[0] & 0x7FFFFFFF) >  (to.val[0] & 0x7FFFFFFF)) ||	/* |from| > |to| */
	   (((from.val[0] & 0x7FFFFFFF) == (to.val[0] & 0x7FFFFFFF)) &&
	    ((from.val[1]               >   to.val[1]             )))) {
		if (((from.val[0] & 0x7FFFFFFF) == 0x00100000) &&	/* x.00001.000000 */
		     (from.val[1] == 0))
			return (struct big_int) {.val[0] = 0};	/* 0 */
		else {
			from.val[1]--;
			if (from.val[1] == 0xFFFFFFFF)
				from.val[0]--;
		}
	} else {
		from.val[1]++;
		if (from.val[1] == 0)
			from.val[0]++;
	}

	/* swap back to silly PDP-11 order */
	from.val[0] = (from.val[0] >> 16) | (from.val[0] << 16);
	from.val[1] = (from.val[1] >> 16) | (from.val[1] << 16);

	/* done! */
	return from;
}


struct big_int fp_nextafter_h(struct big_int from, struct big_int to)
{
	/* reserved values? */
	if (((from.val[0] & 0xFFFF) == 0x8000) || ((to.val[0] & 0xFFFF) == 0x8000))
		return (struct big_int) {.val[0] = 0x8000};	/* reserved */

	/* 0? */
	if ((from.val[0] & 0xFFFF) == 0) { /* from == 0 */
		if ((to.val[0] & 0xFFFF) == 0)	/* to == 0 */
			return (struct big_int) {.val[0] = 0};	/* 0 */
		else
			return (struct big_int) {.val[0] = 1 | (to.val[0] & 0x8000)}; /* x.00001.00000 */
	}

	if ((to.val[0] & 0xFFFF) == 0) /* 0 */
		if ((from.val[0] & 0xFFFF7FFF) == 0x00000001)	/* x.00001.000000 */
			return (struct big_int) {.val[0] = 0};	/* 0 */

	/* normal numbers from here on... */

	/* this equality comparison compares *bit patterns* so it won't catch
	   all cases of two reserved values or two 0.0 values.
	 */
	if ((from.val[0] == to.val[0]) && (from.val[1] == to.val[1]) &&
	    (from.val[2] == to.val[2]) && (from.val[3] == to.val[3]))
		return to;

	/* proper word order like God intended... */
	from.val[0] = (from.val[0] >> 16) | (from.val[0] << 16);
	from.val[1] = (from.val[1] >> 16) | (from.val[1] << 16);
	from.val[2] = (from.val[2] >> 16) | (from.val[2] << 16);
	from.val[3] = (from.val[3] >> 16) | (from.val[3] << 16);
	to.val[0]   = (to.val[0]   >> 16) | (to.val[0]   << 16);
	to.val[1]   = (to.val[1]   >> 16) | (to.val[1]   << 16);
	to.val[2]   = (to.val[2]   >> 16) | (to.val[2]   << 16);
	to.val[3]   = (to.val[3]   >> 16) | (to.val[3]   << 16);

	/* if the numbers are on different sides of zero, move 'from' towards
	   zero.  Since the numbers are sign-magnitude, that's a decrement.
	 */
	if (((from.val[0] ^ to.val[0]) & (1 << 31)) ||		/* different sign */

	    ((from.val[0] & 0x7FFFFFFF) >  (to.val[0] & 0x7FFFFFFF)) ||	/* |from| > |to| */

	   (((from.val[0] & 0x7FFFFFFF) == (to.val[0] & 0x7FFFFFFF)) &&
	    ((from.val[1]               >   to.val[1]             ))) ||

	   (((from.val[0] & 0x7FFFFFFF) == (to.val[0] & 0x7FFFFFFF)) &&
	    ( from.val[1]               ==  to.val[1]              ) &&
	    ((from.val[2]               >   to.val[2]             ))) ||

	   (((from.val[0] & 0x7FFFFFFF) == (to.val[0] & 0x7FFFFFFF)) &&
	    ( from.val[1]               ==  to.val[1]              ) &&
	    ( from.val[2]               ==  to.val[2]              ) &&
	    ((from.val[3]               >   to.val[3]             )))

	   ) {
		if (((from.val[0] & 0x7FFFFFFF) == 0x00010000) &&	/* x.00001.000000 */
		     (from.val[1] == 0) &&
		     (from.val[2] == 0) &&
		     (from.val[3] == 0))
			return (struct big_int) {.val[0] = 0};	/* 0 */
		else {
			from.val[3]--;
			if (from.val[3] == 0xFFFFFFFF) {
				from.val[2]--;
				if (from.val[2] == 0xFFFFFFFF) {
					from.val[1]--;
					if (from.val[1] == 0xFFFFFFFF)
						from.val[0]--;
				}
			}
		}
	} else {
		from.val[3]++;
		if (from.val[3] == 0) {
			from.val[2]++;
			if (from.val[2] == 0) {
				from.val[1]++;
				if (from.val[1] == 0)
					from.val[0]++;
			}
		}
	}

	/* swap back to silly PDP-11 order */
	from.val[0] = (from.val[0] >> 16) | (from.val[0] << 16);
	from.val[1] = (from.val[1] >> 16) | (from.val[1] << 16);
	from.val[2] = (from.val[2] >> 16) | (from.val[2] << 16);
	from.val[3] = (from.val[3] >> 16) | (from.val[3] << 16);

	/* done! */
	return from;
}


struct big_int fp_nextafter(struct big_int from, struct big_int to, char type)
{
	switch (type) {
	case 'f':	return fp_nextafter_f(from, to);
	case 'd':	return fp_nextafter_d(from, to);
	case 'g':	return fp_nextafter_g(from, to);
	case 'h':	return fp_nextafter_h(from, to);
	default:
		UNREACHABLE();
	}
}



/* FIXME only has testcases for 'f'.

   Fixed most of the bugs using that.  The others were written/copied after 'f'
   was tested.  No errors were discovered during hundreds of millions of random
   tests of d/g/h so they are probably not entirely wrong.
 */
struct {
	struct big_int	from;
	struct big_int	to;
	struct big_int	res;
	char		type;
} nextafter_cases[] = {
	/* 0 --> 0: 0 */
	{.from.val[0] = 0x00000000, .to.val[0] = 0x00000000, .res.val[0] = 0x00000000, .type = 'f'},
	{.from.val[0] = 0x00000000, .to.val[0] = 0x00000001, .res.val[0] = 0x00000000, .type = 'f'},
	{.from.val[0] = 0x00000001, .to.val[0] = 0x00000000, .res.val[0] = 0x00000000, .type = 'f'},
	{.from.val[0] = 0x00000000, .to.val[0] = 0x00010000, .res.val[0] = 0x00000000, .type = 'f'},
	{.from.val[0] = 0x00010000, .to.val[0] = 0x00000000, .res.val[0] = 0x00000000, .type = 'f'},
	{.from.val[0] = 0x00000000, .to.val[0] = 0x00000000, .res.val[0] = 0x00000000, .type = 'f'},
	{.from.val[0] = 0xFFFF0000, .to.val[0] = 0x00000000, .res.val[0] = 0x00000000, .type = 'f'},
	{.from.val[0] = 0x00000000, .to.val[0] = 0xFFFF0000, .res.val[0] = 0x00000000, .type = 'f'},
	{.from.val[0] = 0xFFFF007F, .to.val[0] = 0x00000000, .res.val[0] = 0x00000000, .type = 'f'},
	{.from.val[0] = 0x00000000, .to.val[0] = 0xFFFF007F, .res.val[0] = 0x00000000, .type = 'f'},

	/* reserved */
	{.from.val[0] = 0x00008000, .to.val[0] = 0x00000000, .res.val[0] = 0x00008000, .type = 'f'},
	{.from.val[0] = 0x00000000, .to.val[0] = 0x00008000, .res.val[0] = 0x00008000, .type = 'f'},
	{.from.val[0] = 0xFFFF8000, .to.val[0] = 0x00008000, .res.val[0] = 0x00008000, .type = 'f'},
	{.from.val[0] = 0x0000807F, .to.val[0] = 0x00000000, .res.val[0] = 0x00008000, .type = 'f'},
	{.from.val[0] = 0x00000000, .to.val[0] = 0x0000807F, .res.val[0] = 0x00008000, .type = 'f'},
	{.from.val[0] = 0x00000000, .to.val[0] = 0xFFFF807F, .res.val[0] = 0x00008000, .type = 'f'},
	{.from.val[0] = 0xFFFF807F, .to.val[0] = 0x00000000, .res.val[0] = 0x00008000, .type = 'f'},
	{.from.val[0] = 0x0000807F, .to.val[0] = 0x0000807F, .res.val[0] = 0x00008000, .type = 'f'},
	{.from.val[0] = 0xFFFF8000, .to.val[0] = 0xFFFF8000, .res.val[0] = 0x00008000, .type = 'f'},
	{.from.val[0] = 0x00018000, .to.val[0] = 0x00018000, .res.val[0] = 0x00008000, .type = 'f'},

	/* from: 0.00001.00000  to: various 0's */
	{.from.val[0] = 0x00000080, .to.val[0] = 0x00000000, .res.val[0] = 0x00000000, .type = 'f'},
	{.from.val[0] = 0x00000080, .to.val[0] = 0x0000007F, .res.val[0] = 0x00000000, .type = 'f'},
	{.from.val[0] = 0x00000080, .to.val[0] = 0xFFFF0000, .res.val[0] = 0x00000000, .type = 'f'},
	{.from.val[0] = 0x00000080, .to.val[0] = 0x00010000, .res.val[0] = 0x00000000, .type = 'f'},
	{.from.val[0] = 0x00000080, .to.val[0] = 0x00000000, .res.val[0] = 0x00000000, .type = 'f'},

	/* from: 1.00001.00000 to: various 0's*/
	{.from.val[0] = 0x00008080, .to.val[0] = 0x00000000, .res.val[0] = 0x00000000, .type = 'f'},
	{.from.val[0] = 0x00008080, .to.val[0] = 0x0000007F, .res.val[0] = 0x00000000, .type = 'f'},
	{.from.val[0] = 0x00008080, .to.val[0] = 0xFFFF0000, .res.val[0] = 0x00000000, .type = 'f'},
	{.from.val[0] = 0x00008080, .to.val[0] = 0x00010000, .res.val[0] = 0x00000000, .type = 'f'},
	{.from.val[0] = 0x00008080, .to.val[0] = 0x00000000, .res.val[0] = 0x00000000, .type = 'f'},

	/* from: 0.00001.00000  to: various non-zeros */
	{.from.val[0] = 0x00000080, .to.val[0] = 0x00000080, .res.val[0] = 0x00000080, .type = 'f'},
	{.from.val[0] = 0x00000080, .to.val[0] = 0x00000081, .res.val[0] = 0x00010080, .type = 'f'},
	{.from.val[0] = 0x00000080, .to.val[0] = 0x00000300, .res.val[0] = 0x00010080, .type = 'f'},
	{.from.val[0] = 0x00000080, .to.val[0] = 0x00008080, .res.val[0] = 0x00000000, .type = 'f'},

	/* from: 1.00001.00000  to: various non-zeros */
	{.from.val[0] = 0x00008080, .to.val[0] = 0x00008080, .res.val[0] = 0x00008080, .type = 'f'},
	{.from.val[0] = 0x00008080, .to.val[0] = 0x00008081, .res.val[0] = 0x00018080, .type = 'f'},
	{.from.val[0] = 0x00008080, .to.val[0] = 0x00000300, .res.val[0] = 0x00000000, .type = 'f'},
	{.from.val[0] = 0x00008080, .to.val[0] = 0x00008300, .res.val[0] = 0x00018080, .type = 'f'},
	{.from.val[0] = 0x00008080, .to.val[0] = 0x00000080, .res.val[0] = 0x00000000, .type = 'f'},
	{.from.val[0] = 0x00008080, .to.val[0] = 0xFFFFFFFF, .res.val[0] = 0x00018080, .type = 'f'},


	/* 0.11111.11111 */
	{.from.val[0] = 0xFFFF7FFF, .to.val[0] = 0xFFFF7FFF, .res.val[0] = 0xFFFF7FFF, .type = 'f'},
	{.from.val[0] = 0xFFFF7FFF, .to.val[0] = 0x00000000, .res.val[0] = 0xFFFE7FFF, .type = 'f'},
	{.from.val[0] = 0xFFFE7FFF, .to.val[0] = 0xFFFF7FFF, .res.val[0] = 0xFFFF7FFF, .type = 'f'},
	{.from.val[0] = 0x00000000, .to.val[0] = 0xFFFF7FFF, .res.val[0] = 0x00000080, .type = 'f'},
	{.from.val[0] = 0xFFFF7FFF, .to.val[0] = 0xFFFE7FFF, .res.val[0] = 0xFFFE7FFF, .type = 'f'},

	/* 1.11111.11111 */
	{.from.val[0] = 0xFFFFFFFF, .to.val[0] = 0xFFFFFFFF, .res.val[0] = 0xFFFFFFFF, .type = 'f'},
	{.from.val[0] = 0xFFFFFFFF, .to.val[0] = 0x00000000, .res.val[0] = 0xFFFEFFFF, .type = 'f'},
	{.from.val[0] = 0xFFFEFFFF, .to.val[0] = 0xFFFFFFFF, .res.val[0] = 0xFFFFFFFF, .type = 'f'},
	{.from.val[0] = 0x00000000, .to.val[0] = 0xFFFFFFFF, .res.val[0] = 0x00008080, .type = 'f'},
	{.from.val[0] = 0xFFFFFFFF, .to.val[0] = 0xFFFEFFFF, .res.val[0] = 0xFFFEFFFF, .type = 'f'},

	/* pos --> neg: dec */
	{.from.val[0] = 0x00000080, .to.val[0] = 0x00018080, .res.val[0] = 0x00000000, .type = 'f'},
	{.from.val[0] = 0x00010080, .to.val[0] = 0x00008080, .res.val[0] = 0x00000080, .type = 'f'},
	{.from.val[0] = 0x12340080, .to.val[0] = 0x00008080, .res.val[0] = 0x12330080, .type = 'f'},
	{.from.val[0] = 0x12347FFF, .to.val[0] = 0x00008080, .res.val[0] = 0x12337FFF, .type = 'f'},
	{.from.val[0] = 0x12340080, .to.val[0] = 0xFFFFFFFF, .res.val[0] = 0x12330080, .type = 'f'},
	{.from.val[0] = 0x12340080, .to.val[0] = 0x1000FFFF, .res.val[0] = 0x12330080, .type = 'f'},

	/* neg --> pos: dec */
	{.from.val[0] = 0x00008080, .to.val[0] = 0x00010080, .res.val[0] = 0x00000000, .type = 'f'},
	{.from.val[0] = 0x00018080, .to.val[0] = 0x00000080, .res.val[0] = 0x00008080, .type = 'f'},
	{.from.val[0] = 0x12348080, .to.val[0] = 0x00000080, .res.val[0] = 0x12338080, .type = 'f'},
	{.from.val[0] = 0x1234FFFF, .to.val[0] = 0x00000080, .res.val[0] = 0x1233FFFF, .type = 'f'},
	{.from.val[0] = 0x12348080, .to.val[0] = 0xFFFF7FFF, .res.val[0] = 0x12338080, .type = 'f'},
	{.from.val[0] = 0x12348080, .to.val[0] = 0x10007FFF, .res.val[0] = 0x12338080, .type = 'f'},


	/* |from| > |to|: dec */
	{.from.val[0] = 0x00000300, .to.val[0] = 0x00000200, .res.val[0] = 0xFFFF02FF, .type = 'f'},
	{.from.val[0] = 0x00010300, .to.val[0] = 0x00000300, .res.val[0] = 0x00000300, .type = 'f'},
	{.from.val[0] = 0x00030080, .to.val[0] = 0x00020080, .res.val[0] = 0x00020080, .type = 'f'},

	{.from.val[0] = 0x00008300, .to.val[0] = 0x00008200, .res.val[0] = 0xFFFF82FF, .type = 'f'},
	{.from.val[0] = 0x00018300, .to.val[0] = 0x00008300, .res.val[0] = 0x00008300, .type = 'f'},
	{.from.val[0] = 0x00038080, .to.val[0] = 0x00028080, .res.val[0] = 0x00028080, .type = 'f'},

	/* from < to: inc */
	{.from.val[0] = 0x00000200, .to.val[0] = 0x00000300, .res.val[0] = 0x00010200, .type = 'f'},
	{.from.val[0] = 0x00000300, .to.val[0] = 0x00010300, .res.val[0] = 0x00010300, .type = 'f'},
	{.from.val[0] = 0x00020080, .to.val[0] = 0x00030080, .res.val[0] = 0x00030080, .type = 'f'},

	{.from.val[0] = 0x00008200, .to.val[0] = 0x00008300, .res.val[0] = 0x00018200, .type = 'f'},
	{.from.val[0] = 0x00008300, .to.val[0] = 0x00018300, .res.val[0] = 0x00018300, .type = 'f'},
	{.from.val[0] = 0x00028080, .to.val[0] = 0x00038080, .res.val[0] = 0x00038080, .type = 'f'},

};


void test_nextafter()
{
	printf("fp_nextafter\n");
	printf("------------\n");

	for (unsigned i=0; i < ARRAY_SIZE(nextafter_cases); i++) {
		struct big_int	res;
		struct big_int	expected = nextafter_cases[i].res;

		res = fp_nextafter(nextafter_cases[i].from, nextafter_cases[i].to, nextafter_cases[i].type);

		if ((res.val[0] != expected.val[0]) ||
		    (res.val[1] != expected.val[1]) ||
		    (res.val[2] != expected.val[2]) ||
		    (res.val[3] != expected.val[3])) {

			printf("%3u  %04X_%04X --> %04X_%04X\n"
			       "got  %04X_%04X\n"
			       "exp  %04X_%04X\n",
			       i,
			       SPLIT(nextafter_cases[i].from.val[0]),
			       SPLIT(nextafter_cases[i].to.val[0]),
			       SPLIT(res.val[0]),
			       SPLIT(expected.val[0]));

			printf("\n");
		}
	}

	printf("\n\n");
}


bool within_1ulp(struct big_int f, struct big_int g, char type)
{
	switch (type) {
	case 'f':
		{
		struct big_int	next = fp_nextafter(f, g, type);

		if (next.val[0] == g.val[0])
			return true;
		return false;
		}
	case 'd':
		{
		struct big_int	next = fp_nextafter(f, g, type);

		if ((next.val[0] == g.val[0]) && (next.val[1] == g.val[1]))
			return true;
		return false;
		}
	case 'g':
		{
		struct big_int	next = fp_nextafter(f, g, type);

		if ((next.val[0] == g.val[0]) && (next.val[1] == g.val[1]))
			return true;
		return false;
		}
	case 'h':
		{
		struct big_int	next = fp_nextafter(f, g, type);

		if ((next.val[0] == g.val[0]) && (next.val[1] == g.val[1]) &&
		    (next.val[2] == g.val[2]) && (next.val[3] == g.val[3]))
			return true;
		return false;
		}
	default:
		UNREACHABLE();
	}
}


void experiment_fp()
{
	struct big_int	fp = {{0}};

	fp.val[0] = 0x209B3F9A; /* 'f' for Log10(2) = 0.301029996 */
	printf(" log_10(2) = %e %f %.10g\n", log10f(2.0), log10f(2.0), log10f(2.0));
	printf(" log_10(2) = %s\n", fp_to_str("%g", fp, 'f').str);

	printf("\n\n");
	fp.val[0] = 0x21CA4029; /* 'g' for 3.1415 */
	fp.val[1] = 0x126FC083;
	printf(" x = %s\n", fp_to_str("%g", fp, 'g').str);

	printf("\n\n");
	fp.val[0] = 0x0E564149; /* 'd' for 3.1415 */
	fp.val[1] = 0x93750418;
	printf(" x = %s\n", fp_to_str("%g", fp, 'd').str);

	printf("\n\n");
	fp.val[0] = 0x5BF04002; /* 'h' for 2.71828 */
	fp.val[1] = 0xAF78995A;
	fp.val[2] = 0x5EC8FEEF;
	fp.val[3] = 0xABC90C73;
	printf(" x = %s\n", fp_to_str("%g", fp, 'h').str);

	printf("\n\n");

	printf("3.1415:\n");
	fp_from_str(&fp, "3.1415", 'f');
	printf("fp: %04X_%04X\n", SPLIT(fp.val[0]));
	printf("\n");
	printf("fp: %s\n", fp_to_str("%g", fp, 'f').str);

	fp_from_str(&fp, "3.1415", 'd');
	printf("fp: %04X_%04X %04X_%04X\n", SPLIT(fp.val[0]), SPLIT(fp.val[1]));
	printf("\n");
	printf("fp: %s\n", fp_to_str("%g", fp, 'd').str);

	fp_from_str(&fp, "3.1415", 'g');
	printf("fp: %04X_%04X %04X_%04X\n", SPLIT(fp.val[0]), SPLIT(fp.val[1]));
	printf("\n");
	printf("fp: %s\n", fp_to_str("%g", fp, 'g').str);

	fp_from_str(&fp, "3.1415", 'h');
	printf("fp: %04X_%04X %04X_%04X %04X_%04X %04X_%04X\n",
	       SPLIT(fp.val[0]), SPLIT(fp.val[1]), SPLIT(fp.val[2]), SPLIT(fp.val[3]));
	printf("\n");
	printf("fp: %s\n", fp_to_str("%g", fp, 'h').str);
}


void test_all_f()
{
	struct big_int	fp;

	/* all 2^32 f values */
	unsigned i=0;
	unsigned res_cnt = 0, cant_parse_cnt=0, wrong_cnt=0;
	do {
		fp.val[0] = i;
		struct str_ret str = fp_to_str("%.12g", fp, 'f');

		if (strcmp(str.str, "reserved") == 0) {
			res_cnt++;
		}

//		printf("%9d %04X_%04X |%s|\n", i, SPLIT(i), str.str);

		if ((i % 0x10000) == 0) {
			printf("i: %9u %04X_%04X\n", i, SPLIT(i));
		}

		if (strcmp(str.str, "reserved") != 0) {
			struct big_int	fp2;

			if (!fp_from_str(&fp2, str.str, 'f')) {
				printf("i: %9u %04X_%04X --> |%s|, which can't be parsed.\n", i, SPLIT(i), str.str);

				cant_parse_cnt++;
			} else if (fp2.val[0] != fp.val[0]) {
				if (((fp2.val[0] & 0x7F80) == 0) && ((fp.val[0] & 0x7F80) == 0)) {
					/* both represent 0.0 */
					goto NEXT;
				}

				if (!within_1ulp(fp2, fp, 'f')) {
					struct str_ret	str2 = fp_to_str("%.12g", fp2, 'f');

					printf("i: %9u %04X_%04X --> |%s|\n"
					       "      -->    %04X_%04X --> |%s|\n",
					       i, SPLIT(i), str.str,
					       SPLIT(fp2.val[0]), str2.str);
				}
				wrong_cnt++;
			}
		}

NEXT:
		i++;
	} while (i!=0);
}


void test_f()
{
#if 1
	const unsigned	CNT = 10*1000*1000;
#else
	const unsigned	CNT = 10*1000;
#endif
	unsigned	res_cnt=0, cant_parse_cnt=0, wrong_cnt=0;

	printf("0000_xxxx f\n");
	printf("-----------\n");

	for (unsigned i=0; i <= 0xFFFF; i++) {
		struct big_int	fp;

		fp.val[0] = 0x00000000 | i;

		struct str_ret str = fp_to_str("%.12g", fp, 'f');

		if (strcmp(str.str, "reserved") == 0) {
			res_cnt++;
		}

		if (strcmp(str.str, "reserved") != 0) {
			struct big_int	fp2;

			if (!fp_from_str(&fp2, str.str, 'f')) {
				printf("i: %9u %04X_%04X --> |%s|, which can't be parsed.\n", i, SPLIT(fp.val[0]), str.str);

				cant_parse_cnt++;
			} else if (fp2.val[0] != fp.val[0]) {
				if (((fp2.val[0] & 0x7F80) == 0) && ((fp.val[0] & 0x7F80) == 0)) {
					/* both represent 0.0 */
					continue;
				}

				if (!within_1ulp(fp, fp2, 'f')) {
					struct str_ret	str2 = fp_to_str("%.12g", fp2, 'f');

					printf("i: %9u %04X_%04X --> |%s|\n"
					       "      -->    %04X_%04X --> |%s|\n",
					       i, SPLIT(fp.val[0]), str.str,
					       SPLIT(fp2.val[0]), str2.str);
				}
				wrong_cnt++;
			}
		}
	}
	printf("\n\n");

	printf("FFFF_xxxx f\n");
	printf("-----------\n");

	for (unsigned i=0; i <= 0xFFFF; i++) {
		struct big_int	fp;

		fp.val[0] = 0xFFFF0000 | i;

		struct str_ret str = fp_to_str("%.12g", fp, 'f');

		if (strcmp(str.str, "reserved") == 0) {
			res_cnt++;
		}

		if (strcmp(str.str, "reserved") != 0) {
			struct big_int	fp2;

			if (!fp_from_str(&fp2, str.str, 'f')) {
				printf("i: %9u %04X_%04X --> |%s|, which can't be parsed.\n", i, SPLIT(fp.val[0]), str.str);

				cant_parse_cnt++;
			} else if (fp2.val[0] != fp.val[0]) {
				if (((fp2.val[0] & 0x7F80) == 0) && ((fp.val[0] & 0x7F80) == 0)) {
					/* both represent 0.0 */
					continue;
				}

				if (!within_1ulp(fp, fp2, 'f')) {
					struct str_ret	str2 = fp_to_str("%.12g", fp2, 'f');

					printf("i: %9u %04X_%04X --> |%s|\n"
					       "      -->    %04X_%04X --> |%s|\n",
					       i, SPLIT(fp.val[0]), str.str,
					       SPLIT(fp2.val[0]), str2.str);
				}
				wrong_cnt++;
			}
		}
	}
	printf("\n\n");

	printf("random f\n");
	printf("--------\n");
	printf("%u test cases.\n\n", CNT);

	for (unsigned i=0; i < CNT; i++) {
		struct big_int	fp;

		/* 32 random bits.

		   mrand48() returns 32 bits but with as a signed long.
		   lrand48() only returns 31 bits but always positive.

		   conversion of a 32-bit signed integer to a uint32_t is
		   automatic and silent.
		 */
		fp.val[0] = mrand48();	/* *32* bits */

		struct str_ret str = fp_to_str("%.12g", fp, 'f');

		if (strcmp(str.str, "reserved") == 0) {
			res_cnt++;
		}

		if ((i % 0x10000) == 0) {
			printf("i: %9u %04X_%04X\n", i, SPLIT(fp.val[0]));
		}

		if (strcmp(str.str, "reserved") != 0) {
			struct big_int	fp2;

			if (!fp_from_str(&fp2, str.str, 'f')) {
				printf("i: %9u %04X_%04X --> |%s|, which can't be parsed.\n", i, SPLIT(fp.val[0]), str.str);

				cant_parse_cnt++;
			} else if (fp2.val[0] != fp.val[0]) {
				if (((fp2.val[0] & 0x7F80) == 0) && ((fp.val[0] & 0x7F80) == 0)) {
					/* both represent 0.0 */
					continue;
				}

				if (!within_1ulp(fp, fp2, 'f')) {
					struct str_ret	str2 = fp_to_str("%.12g", fp2, 'f');

					printf("i: %9u %04X_%04X --> |%s|\n"
					       "      -->    %04X_%04X --> |%s|\n",
					       i, SPLIT(fp.val[0]), str.str,
					       SPLIT(fp2.val[0]), str2.str);
				}
				wrong_cnt++;
			}
		}
	}
	printf("\n\n");
}


void test_d()
{
#if 1
	const unsigned	CNT = 10*1000*1000;
#else
	const unsigned	CNT = 10*1000;
#endif
	unsigned	res_cnt=0, cant_parse_cnt=0, wrong_cnt=0;

	printf("0000_0000 0000_xxxx d\n");
	printf("---------------------\n");

	for (unsigned i=0; i <= 0xFFFF; i++) {
		struct big_int	fp;

		fp.val[0] = 0x00000000 | i;
		fp.val[1] = 0x00000000;

		struct str_ret str = fp_to_str("%.20g", fp, 'd');

		if (strcmp(str.str, "reserved") == 0) {
			res_cnt++;
		}

		if (strcmp(str.str, "reserved") != 0) {
			struct big_int	fp2;

			if (!fp_from_str(&fp2, str.str, 'd')) {
				printf("i: %9u %04X_%04X %04X_%04X --> |%s|, which can't be parsed.\n",
					i, SPLIT(fp.val[0]), SPLIT(fp.val[1]), str.str);

				cant_parse_cnt++;
			} else if ((fp2.val[0] != fp.val[0]) || (fp2.val[1] != fp.val[1])) {
				if (((fp2.val[0] & 0x7F80) == 0) && ((fp.val[0] & 0x7F80) == 0)) {
					/* both represent 0.0 */
					continue;
				}

				if (within_1ulp(fp2, fp, 'd')) {
					struct str_ret	str2 = fp_to_str("%.20g", fp2, 'd');

					printf("i: %9u %04X_%04X %04X_%04X --> |%s|\n"
					       "      -->    %04X_%04X %04X_%04X --> |%s|\n",
					    i, SPLIT(fp .val[0]), SPLIT(fp .val[1]), str.str,
					       SPLIT(fp2.val[0]), SPLIT(fp2.val[1]), str2.str);
				}
				wrong_cnt++;
			}
		}
	}
	printf("\n\n");

	printf("FFFF_FFFF FFFF_xxxx d\n");
	printf("---------------------\n");

	for (unsigned i=0; i <= 0xFFFF; i++) {
		struct big_int	fp;

		fp.val[0] = 0xFFFF0000 | i;
		fp.val[1] = 0xFFFFFFFF;

		struct str_ret str = fp_to_str("%.20g", fp, 'd');

		if (strcmp(str.str, "reserved") == 0) {
			res_cnt++;
		}

		if (strcmp(str.str, "reserved") != 0) {
			struct big_int	fp2;

			if (!fp_from_str(&fp2, str.str, 'd')) {
				printf("i: %9u %04X_%04X %04X_%04X --> |%s|, which can't be parsed.\n",
					i, SPLIT(fp.val[0]), SPLIT(fp.val[1]), str.str);

				cant_parse_cnt++;
			} else if ((fp2.val[0] != fp.val[0]) || (fp2.val[1] != fp.val[1])) {
				if (((fp2.val[0] & 0x7F80) == 0) && ((fp.val[0] & 0x7F80) == 0)) {
					/* both represent 0.0 */
					continue;
				}

				if (within_1ulp(fp2, fp, 'd')) {
					struct str_ret	str2 = fp_to_str("%.20g", fp2, 'd');

					printf("i: %9u %04X_%04X %04X_%04X --> |%s|\n"
					       "      -->    %04X_%04X %04X_%04X --> |%s|\n",
					    i, SPLIT(fp .val[0]), SPLIT(fp .val[1]), str.str,
					       SPLIT(fp2.val[0]), SPLIT(fp2.val[1]), str2.str);
				}
				wrong_cnt++;
			}
		}
	}
	printf("\n\n");

	printf("random d\n");
	printf("--------\n");
	printf("%u test cases.\n\n", CNT);

	for (unsigned i=0; i < CNT; i++) {
		struct big_int	fp;

		/* 64 random bits.

		   mrand48() returns 32 bits but with as a signed long.
		   lrand48() only returns 31 bits but always positive.

		   conversion of a 32-bit signed integer to a uint32_t is
		   automatic and silent.
		 */
		fp.val[0] = mrand48();	/* *32* bits */
		fp.val[1] = mrand48();	/* *32* bits */

		struct str_ret str = fp_to_str("%.20g", fp, 'd');

		if (strcmp(str.str, "reserved") == 0) {
			res_cnt++;
		}

		if ((i % 0x10000) == 0) {
			printf("i: %9u %04X_%04X %04X_%04X\n", i, SPLIT(fp.val[0]), SPLIT(fp.val[1]));
		}

		if (strcmp(str.str, "reserved") != 0) {
			struct big_int	fp2;

			if (!fp_from_str(&fp2, str.str, 'd')) {
				printf("i: %9u %04X_%04X %04X_%04X --> |%s|, which can't be parsed.\n",
					i, SPLIT(fp.val[0]), SPLIT(fp.val[1]), str.str);

				cant_parse_cnt++;
			} else if ((fp2.val[0] != fp.val[0]) || (fp2.val[1] != fp.val[1])) {
				if (((fp2.val[0] & 0x7F80) == 0) && ((fp.val[0] & 0x7F80) == 0)) {
					/* both represent 0.0 */
					continue;
				}

				if (within_1ulp(fp2, fp, 'd')) {
					struct str_ret	str2 = fp_to_str("%.20g", fp2, 'd');

					printf("i: %9u %04X_%04X %04X_%04X --> |%s|\n"
					       "      -->    %04X_%04X %04X_%04X --> |%s|\n",
					    i, SPLIT(fp .val[0]), SPLIT(fp .val[1]), str.str,
					       SPLIT(fp2.val[0]), SPLIT(fp2.val[1]), str2.str);
				}
				wrong_cnt++;
			}
		}
	}
	printf("\n\n");
}


void test_g()
{
#if 1
	const unsigned	CNT = 10*1000*1000;
#else
	const unsigned	CNT = 10*1000;
#endif
	unsigned	res_cnt=0, cant_parse_cnt=0, wrong_cnt=0;

	printf("0000_0000 0000_xxxx g\n");
	printf("---------------------\n");

	for (unsigned i=0; i <= 0xFFFF; i++) {
		struct big_int	fp;

		fp.val[0] = 0x00000000 | i;
		fp.val[1] = 0x00000000;

		struct str_ret str = fp_to_str("%.20g", fp, 'g');

		if (strcmp(str.str, "reserved") == 0) {
			res_cnt++;
		}

		if (strcmp(str.str, "reserved") != 0) {
			struct big_int	fp2;

			if (!fp_from_str(&fp2, str.str, 'g')) {
				printf("i: %9u %04X_%04X %04X_%04X --> |%s|, which can't be parsed.\n",
					i, SPLIT(fp.val[0]), SPLIT(fp.val[1]), str.str);

				cant_parse_cnt++;
			} else if ((fp2.val[0] != fp.val[0]) || (fp2.val[1] != fp.val[1])) {
				if (((fp2.val[0] & 0x7FF0) == 0) && ((fp.val[0] & 0x7FF0) == 0)) {
					/* both represent 0.0 */
					continue;
				}

				if (!within_1ulp(fp2, fp, 'g')) {
					struct str_ret	str2 = fp_to_str("%.20g", fp2, 'g');

					printf("i: %9u %04X_%04X %04X_%04X --> |%s|\n"
					       "      -->    %04X_%04X %04X_%04X --> |%s|\n",
					    i, SPLIT(fp .val[0]), SPLIT(fp .val[1]), str.str,
					       SPLIT(fp2.val[0]), SPLIT(fp2.val[1]), str2.str);
				}
				wrong_cnt++;
			}
		}
	}
	printf("\n\n");


	printf("FFFF_FFFF FFFF_xxxx g\n");
	printf("---------------------\n");

	for (unsigned i=0; i <= 0xFFFF; i++) {
		struct big_int	fp;

		fp.val[0] = 0xFFFF0000 | i;
		fp.val[1] = 0xFFFFFFFF;

		struct str_ret str = fp_to_str("%.20g", fp, 'g');

		if (strcmp(str.str, "reserved") == 0) {
			res_cnt++;
		}

		if (strcmp(str.str, "reserved") != 0) {
			struct big_int	fp2;

			if (!fp_from_str(&fp2, str.str, 'g')) {
				printf("i: %9u %04X_%04X %04X_%04X --> |%s|, which can't be parsed.\n",
					i, SPLIT(fp.val[0]), SPLIT(fp.val[1]), str.str);

				cant_parse_cnt++;
			} else if ((fp2.val[0] != fp.val[0]) || (fp2.val[1] != fp.val[1])) {
				if (((fp2.val[0] & 0x7FF0) == 0) && ((fp.val[0] & 0x7FF0) == 0)) {
					/* both represent 0.0 */
					continue;
				}

				if (!within_1ulp(fp2, fp, 'g')) {
					struct str_ret	str2 = fp_to_str("%.20g", fp2, 'g');

					printf("i: %9u %04X_%04X %04X_%04X --> |%s|\n"
					       "      -->    %04X_%04X %04X_%04X --> |%s|\n",
					    i, SPLIT(fp .val[0]), SPLIT(fp .val[1]), str.str,
					       SPLIT(fp2.val[0]), SPLIT(fp2.val[1]), str2.str);
				}
				wrong_cnt++;
			}
		}
	}
	printf("\n\n");


	printf("random g\n");
	printf("--------\n");
	printf("%u test cases.\n\n", CNT);

	for (unsigned i=0; i < CNT; i++) {
		struct big_int	fp;

		/* 64 random bits.

		   mrand48() returns 32 bits but with as a signed long.
		   lrand48() only returns 31 bits but always positive.

		   conversion of a 32-bit signed integer to a uint32_t is
		   automatic and silent.
		 */
		fp.val[0] = mrand48();	/* *32* bits */
		fp.val[1] = mrand48();	/* *32* bits */

		struct str_ret str = fp_to_str("%.20g", fp, 'g');

		if (strcmp(str.str, "reserved") == 0) {
			res_cnt++;
		}

		if ((i % 0x10000) == 0) {
			printf("i: %9u %04X_%04X %04X_%04X\n", i, SPLIT(fp.val[0]), SPLIT(fp.val[1]));
		}

		if (strcmp(str.str, "reserved") != 0) {
			struct big_int	fp2;

			if (!fp_from_str(&fp2, str.str, 'g')) {
				printf("i: %9u %04X_%04X %04X_%04X --> |%s|, which can't be parsed.\n",
					i, SPLIT(fp.val[0]), SPLIT(fp.val[1]), str.str);

				cant_parse_cnt++;
			} else if ((fp2.val[0] != fp.val[0]) || (fp2.val[1] != fp.val[1])) {
				if (((fp2.val[0] & 0x7FF0) == 0) && ((fp.val[0] & 0x7FF0) == 0)) {
					/* both represent 0.0 */
					continue;
				}

				if (!within_1ulp(fp2, fp, 'g')) {
					struct str_ret	str2 = fp_to_str("%.20g", fp2, 'g');

					printf("i: %9u %04X_%04X %04X_%04X --> |%s|\n"
					       "      -->    %04X_%04X %04X_%04X --> |%s|\n",
					    i, SPLIT(fp .val[0]), SPLIT(fp .val[1]), str.str,
					       SPLIT(fp2.val[0]), SPLIT(fp2.val[1]), str2.str);
				}
				wrong_cnt++;
			}
		}
	}
	printf("\n\n");
}


void test_h()
{
#if 1
	const unsigned	CNT = 100*1000*1000;
#else
	const unsigned	CNT = 100*1000;
#endif
	unsigned	res_cnt=0, cant_parse_cnt=0, wrong_cnt=0;




	printf("0000_0000 0000_0000 0000_0000 0000_xxxx h\n");
	printf("-----------------------------------------\n");

	for (unsigned i=0; i <= 0xFFFF; i++) {
		struct big_int	fp;

		fp.val[0] = 0 | i;
		fp.val[1] = 0;
		fp.val[2] = 0;
		fp.val[3] = 0;

		struct str_ret str = fp_to_str("%.40g", fp, 'h');

		if (strcmp(str.str, "reserved") == 0) {
			res_cnt++;
		}

		if (strcmp(str.str, "reserved") != 0) {
			struct big_int	fp2;

			if (!fp_from_str(&fp2, str.str, 'h')) {
				printf("i:%9u %04X_%04X %04X_%04X %04X_%04X %04X_%04X -> |%s|, which can't be parsed.\n",
					i, SPLIT(fp.val[0]), SPLIT(fp.val[1]), SPLIT(fp.val[2]), SPLIT(fp.val[3]), str.str);

				cant_parse_cnt++;
			} else if ((fp2.val[0] != fp.val[0]) || (fp2.val[1] != fp.val[1]) ||
			           (fp2.val[2] != fp.val[2]) || (fp2.val[3] != fp.val[3])) {
				if (((fp2.val[0] & 0x7FFF) == 0) && ((fp.val[0] & 0x7FFF) == 0)) {
					/* both represent 0.0 */
					continue;
				}

				if (!within_1ulp(fp2, fp, 'h')) {
					struct str_ret	str2 = fp_to_str("%.40g", fp2, 'h');

					printf("i:%9u %04X_%04X %04X_%04X %04X_%04X %04X_%04X -> |%s|\n"
					       "     -->    %04X_%04X %04X_%04X %04X_%04X %04X_%04X -> |%s|\n",
					    i, SPLIT(fp. val[0]), SPLIT(fp. val[1]), SPLIT(fp. val[2]), SPLIT(fp. val[3]), str.str,
					       SPLIT(fp2.val[0]), SPLIT(fp2.val[1]), SPLIT(fp2.val[2]), SPLIT(fp2.val[3]), str2.str);
				}
				wrong_cnt++;
			}
		}
	}
	printf("\n\n");





	printf("FFFF_FFFF FFFF_FFFF FFFF_FFFF FFFF_xxxx h\n");
	printf("-----------------------------------------\n");

	for (unsigned i=0; i <= 0xFFFF; i++) {
		struct big_int	fp;

		fp.val[0] = 0xFFFF0000 | i;
		fp.val[1] = 0xFFFFFFFF;
		fp.val[2] = 0xFFFFFFFF;
		fp.val[3] = 0xFFFFFFFF;

		struct str_ret str = fp_to_str("%.40g", fp, 'h');

		if (strcmp(str.str, "reserved") == 0) {
			res_cnt++;
		}

		if (strcmp(str.str, "reserved") != 0) {
			struct big_int	fp2;

			if (!fp_from_str(&fp2, str.str, 'h')) {
				printf("i:%9u %04X_%04X %04X_%04X %04X_%04X %04X_%04X -> |%s|, which can't be parsed.\n",
					i, SPLIT(fp.val[0]), SPLIT(fp.val[1]), SPLIT(fp.val[2]), SPLIT(fp.val[3]), str.str);

				cant_parse_cnt++;
			} else if ((fp2.val[0] != fp.val[0]) || (fp2.val[1] != fp.val[1]) ||
			           (fp2.val[2] != fp.val[2]) || (fp2.val[3] != fp.val[3])) {
				if (((fp2.val[0] & 0x7FFF) == 0) && ((fp.val[0] & 0x7FFF) == 0)) {
					/* both represent 0.0 */
					continue;
				}

				if (!within_1ulp(fp2, fp, 'h')) {
					struct str_ret	str2 = fp_to_str("%.40g", fp2, 'h');

					printf("i:%9u %04X_%04X %04X_%04X %04X_%04X %04X_%04X -> |%s|\n"
					       "     -->    %04X_%04X %04X_%04X %04X_%04X %04X_%04X -> |%s|\n",
					    i, SPLIT(fp. val[0]), SPLIT(fp. val[1]), SPLIT(fp. val[2]), SPLIT(fp. val[3]), str.str,
					       SPLIT(fp2.val[0]), SPLIT(fp2.val[1]), SPLIT(fp2.val[2]), SPLIT(fp2.val[3]), str2.str);
				}
				wrong_cnt++;
			}
		}
	}
	printf("\n\n");



	printf("random h\n");
	printf("--------\n");
	printf("%u test cases.\n\n", CNT);

	for (unsigned i=0; i < CNT; i++) {
		struct big_int	fp;

		/* 128 random bits.

		   mrand48() returns 32 bits but with as a signed long.
		   lrand48() only returns 31 bits but always positive.

		   conversion of a 32-bit signed integer to a uint32_t is
		   automatic and silent.
		 */
		fp.val[0] = mrand48();	/* *32* bits */
		fp.val[1] = mrand48();	/* *32* bits */
		fp.val[2] = mrand48();	/* *32* bits */
		fp.val[3] = mrand48();	/* *32* bits */

		struct str_ret str = fp_to_str("%.40g", fp, 'h');

		if (strcmp(str.str, "reserved") == 0) {
			res_cnt++;
		}

		if ((i % 0x10000) == 0) {
			printf("i:%9u %04X_%04X %04X_%04X %04X_%04X %04X_%04X\n",
			       i, SPLIT(fp.val[0]), SPLIT(fp.val[1]), SPLIT(fp.val[2]), SPLIT(fp.val[3]));
		}

		if (strcmp(str.str, "reserved") != 0) {
			struct big_int	fp2;

			if (!fp_from_str(&fp2, str.str, 'h')) {
				printf("i:%9u %04X_%04X %04X_%04X %04X_%04X %04X_%04X -> |%s|, which can't be parsed.\n",
					i, SPLIT(fp.val[0]), SPLIT(fp.val[1]), SPLIT(fp.val[2]), SPLIT(fp.val[3]), str.str);

				cant_parse_cnt++;
			} else if ((fp2.val[0] != fp.val[0]) || (fp2.val[1] != fp.val[1]) ||
			           (fp2.val[2] != fp.val[2]) || (fp2.val[3] != fp.val[3])) {
				if (((fp2.val[0] & 0x7FFF) == 0) && ((fp.val[0] & 0x7FFF) == 0)) {
					/* both represent 0.0 */
					continue;
				}

				if (!within_1ulp(fp2, fp, 'h')) {
					struct str_ret	str2 = fp_to_str("%.40g", fp2, 'h');

					printf("i:%9u %04X_%04X %04X_%04X %04X_%04X %04X_%04X -> |%s|\n"
					       "     -->    %04X_%04X %04X_%04X %04X_%04X %04X_%04X -> |%s|\n",
					    i, SPLIT(fp. val[0]), SPLIT(fp. val[1]), SPLIT(fp. val[2]), SPLIT(fp. val[3]), str.str,
					       SPLIT(fp2.val[0]), SPLIT(fp2.val[1]), SPLIT(fp2.val[2]), SPLIT(fp2.val[3]), str2.str);
				}
				wrong_cnt++;
			}
		}
	}
	printf("\n\n");
}


void test_fp()
{
	setvbuf(stdout, NULL, _IOLBF, BUFSIZ);

	test_nextafter();

	/* all 2^32 f values */
//	test_all_f();

	/* random f values */
	test_f();

	/* random d values */
	test_d();

	/* random g values */
	test_g();

	/* random h values */
	test_h();
}


/***/

#define TEST_DIR	"afl/fp/"

/* FIXME output tests for d/g/h floating-point values to convert to strings
         and for strings to convert to f/d/g/h floating-point values.

   FIXME run AFL tests.
 */

void help()
{
	fprintf(stderr, "./test-fp <mode>\n");
	fprintf(stderr, "\n");
	fprintf(stderr, " mode:\n");
	fprintf(stderr, "   --experiment  experiments with fp values\n");
	fprintf(stderr, "   --built-in    built-in test of fp<-->string conversions\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "MPFR version (according to header):  %s\n", MPFR_VERSION_STRING);
	fprintf(stderr, "MPFR version (according to library): %s\n", mpfr_get_version());
	if (strcmp(mpfr_get_version(), MPFR_VERSION_STRING) != 0)
		fprintf(stderr, "ERROR: header and library don't match!\n");
	fprintf(stderr, "mp_bits_per_limb: %d\n", mp_bits_per_limb);
	exit(EXIT_FAILURE);
}


int main(int argc, char *argv[argc])
{
#ifdef __AFL_HAVE_MANUAL_CONTROL
	while (__AFL_LOOP(1000)) {
#endif
	if (argc != 2) {
		help();
	}

	if        (strcmp(argv[1], "--experiment") == 0) {
		experiment_fp();

	} else if (strcmp(argv[1], "--built-in") == 0) {
		test_fp();

	} else {
		help();
	}
#ifdef __AFL_HAVE_MANUAL_CONTROL
	}
#endif

	return EXIT_SUCCESS;
}

