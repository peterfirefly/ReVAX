/* Copyright 2018  Peter Lund <firefly@vax64.dk>

   Licensed under GPL v2.

   ---

   128-bit bigint routines + tests

 */

#ifndef BIG_INT__H
#define BIG_INT__H

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>


/* 128-bit value needed for octo/h
   (64-bit needed for quad/d/g)

   [0] = least-significant word
   [3] = most-significant word

 */
struct big_int {
	uint32_t	val[4];
};


void print_big(struct big_int x);

struct big_int big_add(struct big_int x, struct big_int y, bool *overflow)
{
	struct big_int	sum;
	uint32_t	cy;

	sum.val[0] = x.val[0] + y.val[0];
	cy = sum.val[0] < x.val[0] ;
	sum.val[1] = x.val[1] + y.val[1] + cy;
	cy = sum.val[1] < x.val[1];
	sum.val[2] = x.val[2] + y.val[2] + cy;
	cy = sum.val[2] < x.val[2];
	sum.val[3] = x.val[3] + y.val[3] + cy;
	cy = sum.val[3] < x.val[3];

	if (overflow)
		*overflow = cy;
	return sum;
}


struct big_int big_neg(struct big_int x)
{
	x.val[0] = ~x.val[0];
	x.val[1] = ~x.val[1];
	x.val[2] = ~x.val[2];
	x.val[3] = ~x.val[3];
	x = big_add(x, (struct big_int) {.val[0] = 1}, NULL);
	return x;
}


struct big_int big_shortmul(struct big_int x, uint32_t y, bool *overflow)
{
	/* ABCD and y and W are 32-bit "digits".

	    ABCD * y
	    --------
	      XX	all the partial sums are 64-bit
	     XX_
	    XX__
	   XX___
	    ----
	    WWWW
	*/

	uint64_t	partial_sum[4];

	partial_sum[0] = (uint64_t) x.val[0] * y;
	partial_sum[1] = (uint64_t) x.val[1] * y;
	partial_sum[2] = (uint64_t) x.val[2] * y;
	partial_sum[3] = (uint64_t) x.val[3] * y;

	bool		ovf3, ovf4;

	/* sum of partial sums */
	struct big_int	sum;

	/* 1st partial sum */
	sum = (struct big_int) {.val[0] = partial_sum[0],        /* low 32 bits  */
	                        .val[1] = partial_sum[0] >> 32}; /* high 32 bits */

	/* 2nd partial sum */
	sum = big_add(sum, (struct big_int) {.val[1] = partial_sum[1],        /* low 32 bits  */
	                                     .val[2] = partial_sum[1] >> 32}, /* high 32 bits */
	                                     NULL);			      /* can't overflow */

	/* 3rd partial sum */
	sum = big_add(sum, (struct big_int) {.val[2] = partial_sum[2],        /* low 32 bits  */
	                                     .val[3] = partial_sum[2] >> 32}, /* high 32 bits */
                                             &ovf3);

	/* 4th partial sum */
	sum = big_add(sum, (struct big_int) {.val[3] = partial_sum[3]      }, /* low 32 bits */
	              &ovf4);

	/* done! */
	if (overflow)
		*overflow = ovf3 || ovf4 || (partial_sum[3] >> 32);

	return sum;
}


static bool idx_ok(int idx)
{
	return (idx >= 0) && (idx <= 3);
}


/* positive = shift left
   negative = shift right
 */
struct big_int big_shl(struct big_int x, int shft)
{
	struct big_int	y;

	/* big shifts first */
	int	disp = -((shft) / 32);
	y.val[0] = idx_ok(0+disp) ? x.val[0+disp] : 0;
	y.val[1] = idx_ok(1+disp) ? x.val[1+disp] : 0;
	y.val[2] = idx_ok(2+disp) ? x.val[2+disp] : 0;
	y.val[3] = idx_ok(3+disp) ? x.val[3+disp] : 0;
	if (shft > 0)
		shft = shft % 32;
	else
		shft = -((-shft) % 32);	/* be *really* clear about the sign! */

	/* small shifts */
	if (shft > 0) {
		/* << */
		assert((shft > 0) && (shft < 32));
		int	back_shft = 32-shft;

		struct big_int	res;

		res.val[0] =                            y.val[0] << shft;
		res.val[1] = (y.val[0] >> back_shft) | (y.val[1] << shft);
		res.val[2] = (y.val[1] >> back_shft) | (y.val[2] << shft);
		res.val[3] = (y.val[2] >> back_shft) | (y.val[3] << shft);

		return res;
	} else if (shft < 0) {
		/* >> */
		shft = -shft;
		assert((shft > 0) && (shft < 32));
		int	back_shft = 32-shft;

		struct big_int	res;

		res.val[0] = (y.val[1] << back_shft) | (y.val[0] >> shft);
		res.val[1] = (y.val[2] << back_shft) | (y.val[1] >> shft);
		res.val[2] = (y.val[3] << back_shft) | (y.val[2] >> shft);
		res.val[3] =                            y.val[3] >> shft;

		return res;
	} else
		return y;
}


int uint32_clz(uint32_t x)
{
	for (int i=0; i < 32; i++)
		if (x & (0x80000000 >> i))
			return i;
	return 32;
}


/* count leading zeros */
int big_clz(struct big_int x)
{
	if ((x.val[3] == 0) && (x.val[2] == 0) && (x.val[1] == 0))
		return 32 + 32 + 32 + uint32_clz(x.val[0]);
	if ((x.val[3] == 0) && (x.val[2] == 0))
		return 32 + 32 +      uint32_clz(x.val[1]);
	if (x.val[3] == 0)
		return 32 +           uint32_clz(x.val[2]);
	return                        uint32_clz(x.val[3]);
}

#endif

