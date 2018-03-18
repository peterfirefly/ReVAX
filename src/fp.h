/* Copyright 2018  Peter Lund <firefly@vax64.dk>\

   Licensed under GPL v2.

 */

/* VAX floating-point operations

   The VAX had these 4 floating-point formats:

    f	"float"		 32-bit, inherited from PDP-11
    d	"double"	 64-bit, inherited from PDP-11
    g			 64-bit, new, increased exponent range over d
    h			128-bit, not on original VAX models, usually emulated

   IEEE floating-point (which everybody uses these days) was defined after the
   VAX was introduced.  The VAX never gained support for it but the Alpha
   supported IEEE 32-bit/64-bit (and VAX f/g) right from the start.

   The routines support the VAX floating-point formats in a platform-neutral
   way.  Conversion to/from decimal is quick and dirty, because it's actually
   a much harder problem than it looks.

   Errors are reported through flags (and a false return value).

   There are separate add/sub/mul/div functions for each floating-point type
   in order to get rounding and overflow/underflow correct.

   Rounding modes?
   Flags?
   "Exceptions"?

   The 128-bit h format is not supported (yet?).  It would be natural to use
   uint128_t for it, but it is not defined by a typical stdint.h, even if the
   compiler actually has a similar type (unsigned __int128_t and __uint128_t).

 */

#ifndef FP__H
#define FP__H

#include <stdbool.h>
#include <stdint.h>

typedef uint32_t	vax_f;
typedef uint64_t	vax_d;
typedef uint64_t	vax_g;
typedef uint8_t		vax_fp;	/* flags */

/* basic operations */
bool vax_add_f(vax_f a, vax_f b, vax_f *sum, vax_fp *flags);	/* ADDF2/ADDF3 */
bool vax_sub_f(vax_f a, vax_f b, vax_f *sum, vax_fp *flags);	/* SUBF2/SUBF3 */
bool vax_mul_f(vax_f a, vax_f b, vax_f *sum, vax_fp *flags);	/* MULF2/MULF3 */
bool vax_div_f(vax_f a, vax_f b, vax_f *sum, vax_fp *flags);	/* DIVF2/DIVF3 */

bool vax_add_d(vax_d a, vax_d b, vax_d *sum, vax_fp *flags);	/* ADDD2/ADDD3 */
bool vax_sub_d(vax_d a, vax_d b, vax_d *sum, vax_fp *flags);	/* SUBD2/SUBD3 */
bool vax_mul_d(vax_d a, vax_d b, vax_d *sum, vax_fp *flags);	/* MULD2/MULD3 */
bool vax_div_d(vax_d a, vax_d b, vax_d *sum, vax_fp *flags);	/* DIVD2/DIVD3 */

bool vax_add_g(vax_g a, vax_g b, vax_g *sum, vax_fp *flags);	/* ADDG2/ADDG3 */
bool vax_sub_g(vax_g a, vax_g b, vax_g *sum, vax_fp *flags);	/* SUBG2/SUBG3 */
bool vax_mul_g(vax_g a, vax_g b, vax_g *sum, vax_fp *flags);	/* MULG2/MULG3 */
bool vax_div_g(vax_g a, vax_g b, vax_g *sum, vax_fp *flags);	/* DIVG2/DIVG3 */

/* CMPF
   CMPD
   CMPG

   MNEGF
   MNEGD
   MNEGG

   EMODF
   EMODD
   EMODG

   POLYF	repeated multiply-accummulate, high-precision accumulator?
   POLYD
   POLYG

   TSTF/TSTD/TSTG  are just CMPF/CMPD/CMPG with 0.

   Why MOVF/MOVD/MOVG?
 */


/* conversion between floats -- f <=> d, f<=> g (no d <=> g) */
bool vax_cvt_f_to_d(vax_f a, vax_d *res);	/* CVTFD */
bool vax_cvt_d_to_f(vax_d a, vax_f *res);	/* CVTDF */
bool vax_cvt_g_to_f(vax_g a, vax_f *res);	/* CVTGF */
bool vax_cvt_f_to_g(vax_f a, vax_g *res);	/* CVTFG */

/* conversion between integers and floats -- f/d/g <=> 32-bit

   32-bit is enough to handle 8-bit/16-bit in the simulator.
 */
bool vax_cvt_f_to_int(vax_f a, int32_t *res);	/* CVTFB, CVTFW, CVTFL */
bool vax_cvt_d_to_int(vax_d a, int32_t *res);	/* CVTDB, CVTDW, CVTDL */
bool vax_cvt_g_to_int(vax_g a, int32_t *res);	/* CVTGB, CVTGW, CVTGL */

bool vax_cvt_f_from_int(int32_t a, vax_f *res);	/* CVTBF, CVTWF, CVTLF */
bool vax_cvt_d_from_int(int64_t a, vax_d *res);	/* CVTBD, CVTWD, CVTLD */
bool vax_cvt_g_from_int(int64_t a, vax_g *res);	/* CVTBG, CVTWG, CVTLG */

/* CVTRFL, CVTRDL, CVTRGL */


/* conversion to string -- similar to %e, %f, %g

   -/0 flags ?

   width must be > 1?
   prec = -1?
 */
const char *vax_f_str_e(vax_f a, int width, int prec);
const char *vax_f_str_f(vax_f a, int width, int prec);
const char *vax_f_str_g(vax_f a, int width, int prec);

const char *vax_d_str_e(vax_d a, int width, int prec);
const char *vax_d_str_f(vax_d a, int width, int prec);
const char *vax_d_str_g(vax_d a, int width, int prec);

const char *vax_g_str_e(vax_g a, int width, int prec);
const char *vax_g_str_f(vax_g a, int width, int prec);
const char *vax_g_str_g(vax_g a, int width, int prec);

// const char *vax_f_str("%g", vax_f a);


/* conversion from string */
bool vax_f_from_str(const char *s, vax_f *a);
bool vax_d_from_str(const char *s, vax_d *a);
bool vax_g_from_str(const char *s, vax_g *a);

#endif

