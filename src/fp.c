/* Copyright 2018  Peter Lund <firefly@vax64.dk>

   Licensed under GPL v2.

   ---

   VAX floating-point operations

   The VAX had these 4 floating-point formats:

    f	"float"		 32-bit, inherited from PDP-11
    d	"double"	 64-bit, inherited from PDP-11
    g			 64-bit, new, increased exponent range over d
    h			128-bit, not on original VAX models, usually emulated

   IEEE floating-point (which everybody uses these days) was defined after the
   VAX was introduced.  The VAX never gained support for it but the Alpha
   supported IEEE 32-bit/64-bit (and VAX f/g) right from the start.

   VAX floating-point actually looks a lot like IEEE floating-point, except
   that there's no +/- INF, no NaNs, and no denormalized numbers.  There's
   also no rounding mode settings -- some instructions truncate, some round
   (and there's only one kind of rounding).

   The routines support the VAX floating-point formats in a platform-neutral
   way.  Conversion to/from decimal is quick and dirty, because it's actually
   a much harder problem than it looks.

   The POLY* instructions keep extra bits of precision internally and only
   rounds and normalizes at the end -- that's why there are routines for it
   here.

   Errors are reported through flags (and a false return value).

   Rounding modes?
   Flags?
   "Exceptions"?

   This module takes full advantage of modern C compilers' ability to inline
   functions and optimize the result, even across compilation units, thanks to
   link-time optimization.  In the old days, there would probably be a tool
   that combined the source code files into a single compilation unit -- this
   is how SQLite works today, because it has to work with lots of not-quite-
   modern C compilers on a wide variety of weird platforms.  Old skool code
   would probably also use macros more than this code does.

   F format
   D format
   G format

 */

#include "fp.h"


/* unpack f/d/g

   pack f/d/g

   Macros?  Functions?
 */


/* conversion to string -- similar to %e, %f, %g

   -/0 flags ?

   width must be > 1?
   prec = -1?
 */
struct str_ret vax_f_str_e(vax_f a, int width, int prec)
{
	struct str_ret	buf;
	double		tmp = vax_f_to_double(a);

	sprintf(buf.str, "%*.*f", width, prec, tmp);
	return buf;
}


struct str_ret vax_f_str_f(vax_f a, int width, int prec)
{
	struct str_ret	buf;

	return buf;
}


struct str_ret vax_f_str_g(vax_f a, int width, int prec)
{
	struct str_ret	buf;

	return buf;
}





