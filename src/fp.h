/* Copyright 2018  Peter Lund <firefly@vax64.dk>

   Licensed under GPL v2.

   ---

   The emulator doesn't support floating-point yet, so the vax_xxx_[fdgh]()
   functions are just dummy declarations.

   The assembler and disassembler do support floating-point, so they need a
   way to convert between VAX fp and (decimal) strings.

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

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <mpfr.h>

#include "macros.h"
#include "strret.h"
#include "big-int.h"

typedef uint32_t	vax_f;
typedef uint64_t	vax_d;
typedef uint64_t	vax_g;
typedef uint8_t		vax_fp;	/* flags */

/* FIXME The vax_xxx_x() functions are likely NOT how floating-point is going
         to be implemented in the emulator.

         Likely stages:
          0) at first, fp is just ignored.
          1) then fp instructions trap to emulation
          2) then fp is supported in µcode on a simple 32-bit datapath
          3) maybe some new instructions/new processing elements will be added
          4) maybe full hardware-support for floating-point -- maybe.
 */

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

/***/


/* the string is supposed to be pre-vetted, so it is in a format that
   mpfr_set_str() supports.

   true if the conversion was successful.

   false if not.  Shouldn't be due to unsupported floating-point format but
   could be too high/low exponent, for example.

   precision loss is not a problem and is not reported.
 */
static bool fp_from_str(struct big_int *x, const char *s, char type) __attribute__((unused));
static bool fp_from_str(struct big_int *x, const char *s, char type)
{
	mpfr_t		fp;
	int		expfield;
	bool		neg;

	switch (type) {
	case 'f':
		/* create mpfr_t from the string */
		mpfr_init2(fp, 23+1);
		fp[0]._mpfr_d[0] = 0;
		if (mpfr_set_str(fp, s, /* base */ 10, MPFR_RNDN) == -1) {
			/* not a valid number */
			mpfr_clear(fp);
			return false;
		}

		/* pick the mpfr_t apart */

		/* 0? */
		if (mpfr_zero_p(fp)) {
			/* MSB = 0 ==> 0.0 */
			x->val[0] = 0;	/* exp=0, s=0: 0.0 */
			x->val[1] = 0;
			x->val[2] = 0;
			x->val[3] = 0;
			mpfr_clear(fp);
			return true;
		}

		neg = !!mpfr_signbit(fp);
		expfield = mpfr_get_exp(fp) + 128;

		if ((expfield < 0) || (expfield > 255)) {
			/* exponent too small/large */
			mpfr_clear(fp);
			return false;
		}

		x->val[0] = ((fp[0]._mpfr_d[0] >> 56) &       0x7F) |   /* frac1 */
		             (neg              << 15)               |
		             (expfield         <<  7)               |
		            ((fp[0]._mpfr_d[0] >> 24) & 0xFFFF0000);    /* frac0 */
		x->val[1] = 0;
		x->val[2] = 0;
		x->val[3] = 0;
		break;

	case 'd':
		/* create mpfr_t from the string */
		mpfr_init2(fp, 57+1);
		int	res;
		if ((res = mpfr_set_str(fp, s, /* base */ 10, MPFR_RNDN)) == -1) {
			/* not a valid number */
			mpfr_clear(fp);
			return false;
		}

		/* pick the mpfr_t apart */

		/* 0? */
		if (mpfr_zero_p(fp)) {
			/* MSB = 0 ==> 0.0 */
			x->val[0] = 0;	/* exp=0, s=0: 0.0 */
			x->val[1] = 0;
			x->val[2] = 0;
			x->val[3] = 0;
			mpfr_clear(fp);
			return true;
		}

		neg = !!mpfr_signbit(fp);
		expfield = mpfr_get_exp(fp) + 128;

		if ((expfield < 0) || (expfield > 255)) {
			/* exponent too small/large */
			mpfr_clear(fp);
			return false;
		}

		x->val[0] = ((fp[0]._mpfr_d[0] >> 56) &       0x7F) |   /* frac3 */
		             (neg              << 15)               |
		             (expfield         <<  7)               |
		            ((fp[0]._mpfr_d[0] >> 24) & 0xFFFF0000);    /* frac2 */
		x->val[1] = ((fp[0]._mpfr_d[0] >> 24) &     0xFFFF) |   /* frac1 */
		            ((fp[0]._mpfr_d[0] <<  8) & 0xFFFF0000);    /* frac0 */
		x->val[2] = 0;
		x->val[3] = 0;
		break;

	case 'g':
		/* create mpfr_t from the string */
		mpfr_init2(fp, 53+1);
		if (mpfr_set_str(fp, s, /* base */ 10, MPFR_RNDN) == -1) {
			/* not a valid number */
			mpfr_clear(fp);
			return false;
		}

		/* pick the mpfr_t apart */

		/* 0? */
		if (mpfr_zero_p(fp)) {
			/* MSB = 0 ==> 0.0 */
			x->val[0] = 0;	/* exp=0, s=0: 0.0 */
			x->val[1] = 0;
			x->val[2] = 0;
			x->val[3] = 0;
			mpfr_clear(fp);
			return true;
		}

		neg = !!mpfr_signbit(fp);
		expfield = mpfr_get_exp(fp) + 1024;

		if ((expfield < 0) || (expfield > 2047)) {
			/* exponent too small/large */
			mpfr_clear(fp);
			return false;
		}

		x->val[0] = ((fp[0]._mpfr_d[0] >> 59) &        0xF) |   /* frac3 */
		             (neg              << 15)               |
		             (expfield         <<  4)               |
		            ((fp[0]._mpfr_d[0] >> 27) & 0xFFFF0000);    /* frac2 */
		x->val[1] = ((fp[0]._mpfr_d[0] >> 27) &     0xFFFF) |   /* frac1 */
		            ((fp[0]._mpfr_d[0] <<  5) & 0xFFFF0000);    /* frac0 */
		x->val[2] = 0;
		x->val[3] = 0;
		break;

	case 'h':
		/* create mpfr_t from the string */
		mpfr_init2(fp, 112+1);
		if (mpfr_set_str(fp, s, /* base */ 10, MPFR_RNDN) == -1) {
			/* not a valid number */
			mpfr_clear(fp);
			return false;
		}

		/* pick the mpfr_t apart */

		/* 0? */
		if (mpfr_zero_p(fp)) {
			/* MSB = 0 ==> 0.0 */
			x->val[0] = 0;	/* exp=0, s=0: 0.0 */
			x->val[1] = 0;
			x->val[2] = 0;
			x->val[3] = 0;
			mpfr_clear(fp);
			return true;
		}

		neg = !!mpfr_signbit(fp);
		expfield = mpfr_get_exp(fp) + 16384;

		if ((expfield < 0) || (expfield > 32767)) {
			/* exponent too small/large */
			mpfr_clear(fp);
			return false;
		}

		x->val[0] = ((fp[0]._mpfr_d[1] >> 31) & 0xFFFF0000) |	/* frac6 */
		             (neg              << 15)               |
		              expfield;
		x->val[1] = ((fp[0]._mpfr_d[1] >> 31) &     0xFFFF) |   /* frac5 */
		            ((fp[0]._mpfr_d[1] <<  1) & 0xFFFF0000);    /* frac4 */
		x->val[2] = ((fp[0]._mpfr_d[1] <<  1) &     0xFFFF) |   /* frac3 */
		             (fp[0]._mpfr_d[0] >> 63)               |   /* frac3 */
		            ((fp[0]._mpfr_d[0] >> 31) & 0xFFFF0000);    /* frac2 */
		x->val[3] = ((fp[0]._mpfr_d[0] >> 31) &     0xFFFF) |   /* frac1 */
		            ((fp[0]._mpfr_d[0] <<  1) & 0xFFFF0000);    /* frac0 */
		break;
	default:
		UNREACHABLE();
	}

	mpfr_clear(fp);
	return true;
}


/* convert a VAX fp value to a decimal string representation.

   reserved values are represented as 'reserved'.

   FIXME: take output format as argument
 */
static struct str_ret fp_to_str(const char *fmt, struct big_int x, char type) __attribute__((unused));
static struct str_ret fp_to_str(const char *fmt, struct big_int x, char type)
{
	mpfr_clear_flags();

	mpfr_t		fp;
	bool		neg;

	neg = !!(x.val[0] & 0x8000);

	switch (type) {
	case 'f':
		{
		int		expfield;
		uint32_t	fract;

		mpfr_init2(fp, 23+1);
		expfield =  (x.val[0] >> 7) & 0xFF;
		fract    = ((x.val[0] & 0x7F) << 25) | (x.val[0] >> 7);

		/* check special values (0.0, reserved operand) */
		if (expfield == 0) {
			if (neg) {
				/* reserved operand (ignore fraction) */
				mpfr_clear(fp);
				return (struct str_ret) {.str = "reserved"};
			} else {
				/* 0.0 (ignore fraction) */
				mpfr_set_d(fp, 0.0, MPFR_RNDN);
				break;
			}
		}

		mpfr_set_d(fp, 1.0, MPFR_RNDN);
		mpfr_set_exp(fp, expfield - 128);
		if (mp_bits_per_limb == 64) {
			fp[0]._mpfr_d[0] = ((uint64_t) fract << 31) | ((uint64_t) 1 << 63);
		}
		break;
		}

	case 'd':
		{
		int		expfield;
		uint64_t	fract;

		mpfr_init2(fp, 57+1);
		expfield =  (x.val[0] >> 7) & 0xFF;
		fract    = ((uint64_t) (x.val[0] &       0x7F) << 57) |
		           ((uint64_t) (x.val[0] & 0xFFFF0000) << 25) |
		           ((uint64_t) (x.val[1] &     0xFFFF) << 25) |
		           (           (x.val[1] & 0xFFFF0000) >>  7);

		/* check special values (0.0, reserved operand) */
		if (expfield == 0) {
			if (neg) {
				/* reserved operand (ignore fraction) */
				mpfr_clear(fp);
				return (struct str_ret) {.str = "reserved"};
			} else {
				/* 0.0 (ignore fraction) */
				mpfr_set_d(fp, 0.0, MPFR_RNDN);
				break;
			}
		}

		mpfr_set_d(fp, 1.0, MPFR_RNDN);
		mpfr_set_exp(fp, expfield - 128);
		if (mp_bits_per_limb == 64) {
			fp[0]._mpfr_d[0] = (fract >> 1) | ((uint64_t) 1 << 63);
		}
		break;
		}

	case 'g':
		{
		int		expfield;
		uint64_t	fract;

		mpfr_init2(fp, 53+1);
		expfield = (x.val[0] >> 4) & 0x7FF;
		fract    = ((uint64_t) (x.val[0] &        0xF) << 60) |
		           ((uint64_t) (x.val[0] & 0xFFFF0000) << 28) |
		           ((uint64_t) (x.val[1] &     0xFFFF) << 28) |
		           (           (x.val[1] & 0xFFFF0000) >>  4);

		/* check special values (0.0, reserved operand) */
		if (expfield == 0) {
			if (neg) {
				/* reserved operand (ignore fraction) */
				mpfr_clear(fp);
				return (struct str_ret) {.str = "reserved"};
			} else {
				/* 0.0 (ignore fraction) */
				mpfr_set_d(fp, 0.0, MPFR_RNDN);
				break;
			}
		}

		mpfr_set_d(fp, 1.0, MPFR_RNDN);
		mpfr_set_exp(fp, expfield - 1024);
		if (mp_bits_per_limb == 64) {
			fp[0]._mpfr_d[0] = (fract >> 1) | ((uint64_t) 1 << 63);
		}
		break;
		}

	case 'h':
		{
		int		expfield;
		uint64_t	fract[2];

		mpfr_init2(fp, 112+1);
		expfield = x.val[0] & 0x7FFF;
		fract[0] = ((uint64_t) (x.val[0] & 0xFFFF0000) << 0x20) |
		           ((uint64_t) (x.val[1] &     0xFFFF) << 0x20) |
		            (uint64_t) (x.val[1] & 0xFFFF0000)          |
		           (            x.val[2] &     0xFFFF);
		fract[1] = ((uint64_t) (x.val[2] & 0xFFFF0000) << 0x20) |
		           ((uint64_t) (x.val[3] &     0xFFFF) << 0x20) |
		           (            x.val[3] & 0xFFFF0000);

		/* check special values (0.0, reserved operand) */
		if (expfield == 0) {
			if (neg) {
				/* reserved operand (ignore fraction) */
				mpfr_clear(fp);
				return (struct str_ret) {.str = "reserved"};
			} else {
				/* 0.0 (ignore fraction) */
				mpfr_set_d(fp, 0.0, MPFR_RNDN);
				break;
			}
		}

		mpfr_set_d(fp, 1.0, MPFR_RNDN);
		mpfr_set_exp(fp, expfield - 16384);
		if (mp_bits_per_limb == 64) {
			fp[0]._mpfr_d[1] = (fract[0] >> 1) | ((uint64_t) 1 << 63);
			fp[0]._mpfr_d[0] = (fract[1] >> 1) | (fract[0] << 63);
		}
		break;
		}
	default:
		UNREACHABLE();
	}

	if (neg)
		mpfr_neg(fp, fp, MPFR_RNDN);

	/* convert fmt from "%...e", "%...f", "%...g" to "%...Re", "%...Rf",
	   "%...Rg".
	 */
	char		fmt2[100];
	assert(strlen(fmt) < sizeof(fmt2)-2);
	strcpy(fmt2, fmt);
	fmt2[strlen(fmt)-1] = 'R';
	fmt2[strlen(fmt)  ] = fmt[strlen(fmt)-1];
	fmt2[strlen(fmt)+1] = '\0';

	struct str_ret	buf;
	mpfr_sprintf(buf.str, fmt2, fp, fp, fp);

	mpfr_clear(fp);
	return buf;
}


#endif

