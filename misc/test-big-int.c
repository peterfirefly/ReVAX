/* Copyright 2018  Peter Lund <firefly@vax64.dk>

   Licensed under GPL v2.

   ---

   128-bit bigint routines + tests

 */


#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "shared.h"

#include "big-int.h"

/***/

#define WEAK	"\x1B[2m"
#define NORM	"\x1B[0m"

void print_big(struct big_int x)
{
	printf(WEAK
	       "    3         2         1         0\n"
	       NORM);
	printf("%04X_%04X %04X_%04X %04X_%04X %04X_%04X\n",
		SPLIT(x.val[3]),
		SPLIT(x.val[2]),
		SPLIT(x.val[1]),
		SPLIT(x.val[0]));
}


struct {
	struct big_int	s1, s2;
} testcases_add[] = {
	{{.val[0] = 0xDEADBEEF,
	  .val[1] = 0xCAFEBABE,
	  .val[2] = 0xFACEFEED,
	  .val[3] = 0xFEDCBA90},
	 {.val[0] = 0x21524110,
	  .val[1] = 0,
	  .val[2] = 0,
	  .val[3] = 0}},

	{{.val[0] = 0xDEADBEEF,
	  .val[1] = 0xCAFEBABE,
	  .val[2] = 0xFACEFEED,
	  .val[3] = 0xFEDCBA90},
	 {.val[0] = 0x21524111,
	  .val[1] = 0,
	  .val[2] = 0,
	  .val[3] = 0}},

	{{.val[0] = 0xDEADBEEF,
	  .val[1] = 0xCAFEBABE,
	  .val[2] = 0xFACEFEED,
	  .val[3] = 0xFEDCBA90},
	 {.val[0] = 0x21524111,
	  .val[1] = 0x35014540,
	  .val[2] = 0,
	  .val[3] = 0}},

	{{.val[0] = 0xDEADBEEF,
	  .val[1] = 0xCAFEBABE,
	  .val[2] = 0xFACEFEED,
	  .val[3] = 0xFEDCBA90},
	 {.val[0] = 0x21524111,
	  .val[1] = 0x35014541,
	  .val[2] = 0,
	  .val[3] = 0}},

	{{.val[0] = 0xDEADBEEF,
	  .val[1] = 0xCAFEBABE,
	  .val[2] = 0xFACEFEED,
	  .val[3] = 0xFEDCBA90},
	 {.val[0] = 0x21524111,
	  .val[1] = 0x35014541,
	  .val[2] = 0x05310111,
	  .val[3] = 0}},

	{{.val[0] = 0xDEADBEEF,
	  .val[1] = 0xCAFEBABE,
	  .val[2] = 0xFACEFEED,
	  .val[3] = 0xFEDCBA90},
	 {.val[0] = 0x21524111,
	  .val[1] = 0x35014541,
	  .val[2] = 0x05310112,
	  .val[3] = 0}},

	{{.val[0] = 0xDEADBEEF,
	  .val[1] = 0xCAFEBABE,
	  .val[2] = 0xFACEFEED,
	  .val[3] = 0xFEDCBA90},
	 {.val[0] = 0x21524111,
	  .val[1] = 0x35014541,
	  .val[2] = 0x05310112,
	  .val[3] = 0x0123456E}},

	{{.val[0] = 0xDEADBEEF,
	  .val[1] = 0xCAFEBABE,
	  .val[2] = 0xFACEFEED,
	  .val[3] = 0xFEDCBA90},
	 {.val[0] = 0x21524111,
	  .val[1] = 0x35014541,
	  .val[2] = 0x05310112,
	  .val[3] = 0x0123456F}},
};


void test_add()
{
	printf("\n");
	printf("test_add\n");
	printf("--------\n");

	/* FIXME check overflow */

	printf("All add tests use this addend:\n");
	print_big((struct big_int) {.val[0] = 0xDEADBEEF,
	                            .val[1] = 0xCAFEBABE,
	                            .val[2] = 0xFACEFEED,
	                            .val[3] = 0xFEDCBA90});

	for (unsigned i=0; i < ARRAY_SIZE(testcases_add); i++) {
		struct big_int	res;
		bool		ovf;

		res = big_add(testcases_add[i].s1, testcases_add[i].s2, &ovf);
		print_big(res);
		printf("overflow: %d\n", ovf);
	}

	printf("\n\n");
}


struct big_int	testcases_neg[] = {
	{.val[0] = 0x00000000,
	 .val[1] = 0x00000000,
	 .val[2] = 0x00000000,
	 .val[3] = 0x00000000},

	{.val[0] = 0x00000001,
	 .val[1] = 0x00000000,
	 .val[2] = 0x00000000,
	 .val[3] = 0x00000000},

	{.val[0] = 0x00000002,
	 .val[1] = 0x00000000,
	 .val[2] = 0x00000000,
	 .val[3] = 0x00000000},

	{.val[0] = 0xFFFFFFFF,
	 .val[1] = 0xFFFFFFFF,
	 .val[2] = 0xFFFFFFFF,
	 .val[3] = 0xFFFFFFFF},
};


void test_neg()
{
	printf("\n");
	printf("test_neg\n");
	printf("--------\n");
	for (unsigned i=0; i < ARRAY_SIZE(testcases_neg); i++) {
		print_big(big_neg(testcases_neg[i]));
	}

	printf("\n\n");
}


struct {
	const char	*text;
	struct big_int	x;
	int		shft;
} testcases_shl[] = {
	{.text = "small shifts <<",
	 .x.val[0] = 0x00000000,
	 .x.val[1] = 0x00000000,
	 .x.val[2] = 0x00000000,
	 .x.val[3] = 0x00000000,
	 .shft= 0},

	{.x.val[0] = 0x11111111,
	 .x.val[1] = 0x44444444,
	 .x.val[2] = 0x11111111,
	 .x.val[3] = 0x44444444,
	 .shft = 1},

	{.x.val[0] = 0x11111111,
	 .x.val[1] = 0x44444444,
	 .x.val[2] = 0x11111111,
	 .x.val[3] = 0x22222222,
	 .shft = 2},



	{.text = "byte shifts <<",
	 .x.val[0] = 0x12345678,
	 .x.val[1] = 0xFACEFEED,
	 .x.val[2] = 0xDEADBEEF,
	 .x.val[3] = 0xCAFEBABE,
	 .shft =  0},
	{.x.val[0] = 0x12345678,
	 .x.val[1] = 0xFACEFEED,
	 .x.val[2] = 0xDEADBEEF,
	 .x.val[3] = 0xCAFEBABE,
	 .shft =  8},
	{.x.val[0] = 0x12345678,
	 .x.val[1] = 0xFACEFEED,
	 .x.val[2] = 0xDEADBEEF,
	 .x.val[3] = 0xCAFEBABE,
	 .shft = 16},
	{.x.val[0] = 0x12345678,
	 .x.val[1] = 0xFACEFEED,
	 .x.val[2] = 0xDEADBEEF,
	 .x.val[3] = 0xCAFEBABE,
	 .shft = 24},
	{.x.val[0] = 0x12345678,
	 .x.val[1] = 0xFACEFEED,
	 .x.val[2] = 0xDEADBEEF,
	 .x.val[3] = 0xCAFEBABE,
	 .shft = 32},
	{.x.val[0] = 0x12345678,
	 .x.val[1] = 0xFACEFEED,
	 .x.val[2] = 0xDEADBEEF,
	 .x.val[3] = 0xCAFEBABE,
	 .shft = 40},
	{.x.val[0] = 0x12345678,
	 .x.val[1] = 0xFACEFEED,
	 .x.val[2] = 0xDEADBEEF,
	 .x.val[3] = 0xCAFEBABE,
	 .shft = 48},
	{.x.val[0] = 0x12345678,
	 .x.val[1] = 0xFACEFEED,
	 .x.val[2] = 0xDEADBEEF,
	 .x.val[3] = 0xCAFEBABE,
	 .shft = 56},
	{.x.val[0] = 0x12345678,
	 .x.val[1] = 0xFACEFEED,
	 .x.val[2] = 0xDEADBEEF,
	 .x.val[3] = 0xCAFEBABE,
	 .shft = 64},
	{.x.val[0] = 0x12345678,
	 .x.val[1] = 0xFACEFEED,
	 .x.val[2] = 0xDEADBEEF,
	 .x.val[3] = 0xCAFEBABE,
	 .shft = 72},
	{.x.val[0] = 0x12345678,
	 .x.val[1] = 0xFACEFEED,
	 .x.val[2] = 0xDEADBEEF,
	 .x.val[3] = 0xCAFEBABE,
	 .shft = 80},
	{.x.val[0] = 0x12345678,
	 .x.val[1] = 0xFACEFEED,
	 .x.val[2] = 0xDEADBEEF,
	 .x.val[3] = 0xCAFEBABE,
	 .shft = 88},
	{.x.val[0] = 0x12345678,
	 .x.val[1] = 0xFACEFEED,
	 .x.val[2] = 0xDEADBEEF,
	 .x.val[3] = 0xCAFEBABE,
	 .shft = 96},
	{.x.val[0] = 0x12345678,
	 .x.val[1] = 0xFACEFEED,
	 .x.val[2] = 0xDEADBEEF,
	 .x.val[3] = 0xCAFEBABE,
	 .shft = 104},
	{.x.val[0] = 0x12345678,
	 .x.val[1] = 0xFACEFEED,
	 .x.val[2] = 0xDEADBEEF,
	 .x.val[3] = 0xCAFEBABE,
	 .shft = 112},
	{.x.val[0] = 0x12345678,
	 .x.val[1] = 0xFACEFEED,
	 .x.val[2] = 0xDEADBEEF,
	 .x.val[3] = 0xCAFEBABE,
	 .shft = 120},
	{.x.val[0] = 0x12345678,
	 .x.val[1] = 0xFACEFEED,
	 .x.val[2] = 0xDEADBEEF,
	 .x.val[3] = 0xCAFEBABE,
	 .shft = 128},



	{.text = "boundary tests",
	 .x.val[0] = 0x12345678,
	 .x.val[1] = 0xFACEFEED,
	 .x.val[2] = 0xDEADBEEF,
	 .x.val[3] = 0xCAFEBABE,
	 .shft = 127},

	{.x.val[0] = 0x12345678,
	 .x.val[1] = 0xFACEFEED,
	 .x.val[2] = 0xDEADBEEF,
	 .x.val[3] = 0xCAFEBABE,
	 .shft = 129},

	{.x.val[0] = 0xFFFFFFFF,
	 .x.val[1] = 0xFFFFFFFF,
	 .x.val[2] = 0xFFFFFFFF,
	 .x.val[3] = 0xFFFFFFFF,
	 .shft = 127},

	{.x.val[0] = 0xFFFFFFFF,
	 .x.val[1] = 0xFFFFFFFF,
	 .x.val[2] = 0xFFFFFFFF,
	 .x.val[3] = 0xFFFFFFFF,
	 .shft = 129},

	{.x.val[0] = 0xFFFFFFFF,
	 .x.val[1] = 0xFFFFFFFF,
	 .x.val[2] = 0xFFFFFFFF,
	 .x.val[3] = 0xFFFFFFFF,
	 .shft = 32},



	{.text = "small shifts",
	 .x.val[0] = 0x11111111,
	 .x.val[1] = 0x44444444,
	 .x.val[2] = 0x11111111,
	 .x.val[3] = 0x44444444,
	 .shft = -1},

	{.x.val[0] = 0x11111111,
	 .x.val[1] = 0x44444444,
	 .x.val[2] = 0x11111111,
	 .x.val[3] = 0x22222222,
	 .shft = -2},



	{.text = "byte shifts >>",
	 .x.val[0] = 0x12345678,
	 .x.val[1] = 0xFACEFEED,
	 .x.val[2] = 0xDEADBEEF,
	 .x.val[3] = 0xCAFEBABE,
	 .shft =  -0},
	{.x.val[0] = 0x12345678,
	 .x.val[1] = 0xFACEFEED,
	 .x.val[2] = 0xDEADBEEF,
	 .x.val[3] = 0xCAFEBABE,
	 .shft =  -8},
	{.x.val[0] = 0x12345678,
	 .x.val[1] = 0xFACEFEED,
	 .x.val[2] = 0xDEADBEEF,
	 .x.val[3] = 0xCAFEBABE,
	 .shft = -16},
	{.x.val[0] = 0x12345678,
	 .x.val[1] = 0xFACEFEED,
	 .x.val[2] = 0xDEADBEEF,
	 .x.val[3] = 0xCAFEBABE,
	 .shft = -24},
	{.x.val[0] = 0x12345678,
	 .x.val[1] = 0xFACEFEED,
	 .x.val[2] = 0xDEADBEEF,
	 .x.val[3] = 0xCAFEBABE,
	 .shft = -32},
	{.x.val[0] = 0x12345678,
	 .x.val[1] = 0xFACEFEED,
	 .x.val[2] = 0xDEADBEEF,
	 .x.val[3] = 0xCAFEBABE,
	 .shft = -40},
	{.x.val[0] = 0x12345678,
	 .x.val[1] = 0xFACEFEED,
	 .x.val[2] = 0xDEADBEEF,
	 .x.val[3] = 0xCAFEBABE,
	 .shft = -48},
	{.x.val[0] = 0x12345678,
	 .x.val[1] = 0xFACEFEED,
	 .x.val[2] = 0xDEADBEEF,
	 .x.val[3] = 0xCAFEBABE,
	 .shft = -56},
	{.x.val[0] = 0x12345678,
	 .x.val[1] = 0xFACEFEED,
	 .x.val[2] = 0xDEADBEEF,
	 .x.val[3] = 0xCAFEBABE,
	 .shft = -64},
	{.x.val[0] = 0x12345678,
	 .x.val[1] = 0xFACEFEED,
	 .x.val[2] = 0xDEADBEEF,
	 .x.val[3] = 0xCAFEBABE,
	 .shft = -72},
	{.x.val[0] = 0x12345678,
	 .x.val[1] = 0xFACEFEED,
	 .x.val[2] = 0xDEADBEEF,
	 .x.val[3] = 0xCAFEBABE,
	 .shft = -80},
	{.x.val[0] = 0x12345678,
	 .x.val[1] = 0xFACEFEED,
	 .x.val[2] = 0xDEADBEEF,
	 .x.val[3] = 0xCAFEBABE,
	 .shft = -88},
	{.x.val[0] = 0x12345678,
	 .x.val[1] = 0xFACEFEED,
	 .x.val[2] = 0xDEADBEEF,
	 .x.val[3] = 0xCAFEBABE,
	 .shft = -96},
	{.x.val[0] = 0x12345678,
	 .x.val[1] = 0xFACEFEED,
	 .x.val[2] = 0xDEADBEEF,
	 .x.val[3] = 0xCAFEBABE,
	 .shft = -104},
	{.x.val[0] = 0x12345678,
	 .x.val[1] = 0xFACEFEED,
	 .x.val[2] = 0xDEADBEEF,
	 .x.val[3] = 0xCAFEBABE,
	 .shft = -112},
	{.x.val[0] = 0x12345678,
	 .x.val[1] = 0xFACEFEED,
	 .x.val[2] = 0xDEADBEEF,
	 .x.val[3] = 0xCAFEBABE,
	 .shft = -120},
	{.x.val[0] = 0x12345678,
	 .x.val[1] = 0xFACEFEED,
	 .x.val[2] = 0xDEADBEEF,
	 .x.val[3] = 0xCAFEBABE,
	 .shft = -128},



	{.text = "boundary tests",
	 .x.val[0] = 0x12345678,
	 .x.val[1] = 0xFACEFEED,
	 .x.val[2] = 0xDEADBEEF,
	 .x.val[3] = 0xCAFEBABE,
	 .shft = -127},

	{.x.val[0] = 0x12345678,
	 .x.val[1] = 0xFACEFEED,
	 .x.val[2] = 0xDEADBEEF,
	 .x.val[3] = 0xCAFEBABE,
	 .shft = -129},

	{.x.val[0] = 0xFFFFFFFF,
	 .x.val[1] = 0xFFFFFFFF,
	 .x.val[2] = 0xFFFFFFFF,
	 .x.val[3] = 0xFFFFFFFF,
	 .shft = -127},

	{.x.val[0] = 0xFFFFFFFF,
	 .x.val[1] = 0xFFFFFFFF,
	 .x.val[2] = 0xFFFFFFFF,
	 .x.val[3] = 0xFFFFFFFF,
	 .shft = -129},

	{.x.val[0] = 0xFFFFFFFF,
	 .x.val[1] = 0xFFFFFFFF,
	 .x.val[2] = 0xFFFFFFFF,
	 .x.val[3] = 0xFFFFFFFF,
	 .shft = -32},
};


void test_shl()
{
	printf("\n");
	printf("test_shl\n");
	printf("--------\n");

	for (unsigned i=0; i < ARRAY_SIZE(testcases_shl); i++) {
		if (testcases_shl[i].text) {
			printf("%s\n", testcases_shl[i].text);
		}

		if (testcases_shl[i].shft >= 0)
			printf(" << %d\n", testcases_shl[i].shft);
		else
			printf(" >> %d\n", -testcases_shl[i].shft);

		print_big(big_shl(testcases_shl[i].x, testcases_shl[i].shft));

		if (testcases_shl[i].text)
			printf("\n\n");
	}

	printf("\n\n");
}


uint32_t	testcases_clz_uint32[] = {
	0,
	1,
	2,
	3,
	4,
	0x80000000,
	0x4FFFFFFF,
	0x2FFFFFFF,
	0x0000FFFF,
	0x00001000,
	0x00000100,
	0x00000010,
	0x00000001,
};


struct {
	const char	*fmt;
	struct big_int	 x;
} testcases_clz_big[] = {
	{.fmt = "clz all zeros: %d\n",
	 .x.val[0] = 0x00000000,
	 .x.val[1] = 0x00000000,
	 .x.val[2] = 0x00000000,
	 .x.val[3] = 0x00000000},
	{.fmt = "clz all ones: %d\n",
	 .x.val[0] = 0xFFFFFFFF,
	 .x.val[1] = 0xFFFFFFFF,
	 .x.val[2] = 0xFFFFFFFF,
	 .x.val[3] = 0xFFFFFFFF},


	{.fmt = "clz top word 0x0000_0001, all else zeros: %d\n",
	 .x.val[0] = 0x00000000,
	 .x.val[1] = 0x00000000,
	 .x.val[2] = 0x00000000,
	 .x.val[3] = 0x00000001},
	{.fmt = "clz top word 0x0000_0001, all else ones: %d\n",
	 .x.val[0] = 0xFFFFFFFF,
	 .x.val[1] = 0xFFFFFFFF,
	 .x.val[2] = 0xFFFFFFFF,
	 .x.val[3] = 0x00000001},


	{.fmt = "clz top word zero, next 0x0000_0001, all else zeros: %d\n",
	 .x.val[0] = 0x00000000,
	 .x.val[1] = 0x00000000,
	 .x.val[2] = 0x00000001,
	 .x.val[3] = 0x00000000},
	{.fmt = "clz top word zero, next 0x0000_0001, all else ones: %d\n",
	 .x.val[0] = 0xFFFFFFFF,
	 .x.val[1] = 0xFFFFFFFF,
	 .x.val[2] = 0x00000001,
	 .x.val[3] = 0x00000000},


	{.fmt = "clz top two words zero, next 0x0000_0001, all else zeros: %d\n",
	 .x.val[0] = 0x00000000,
	 .x.val[1] = 0x00000001,
	 .x.val[2] = 0x00000000,
	 .x.val[3] = 0x00000000},
	{.fmt = "clz top two words zero, next 0x0000_0001, all else ones: %d\n",
	 .x.val[0] = 0xFFFFFFFF,
	 .x.val[1] = 0x00000001,
	 .x.val[2] = 0x00000000,
	 .x.val[3] = 0x00000000},


	{.fmt = "clz top three words zero, next 0x0000_1000: %d\n",
	 .x.val[0] = 0x00001000,
	 .x.val[1] = 0x00000000,
	 .x.val[2] = 0x00000000,
	 .x.val[3] = 0x00000000},
	{.fmt = "clz top three words zero, next 0x0000_0001: %d\n",
	 .x.val[0] = 0x00000001,
	 .x.val[1] = 0x00000000,
	 .x.val[2] = 0x00000000,
	 .x.val[3] = 0x00000000},
};


void test_clz()
{
	printf("\n");
	printf("test_clz\n");
	printf("--------\n");

	for (unsigned i=0; i < ARRAY_SIZE(testcases_clz_uint32); i++)
		printf("uint32_clz(0x%04X_%04X): %2d\n",
		       SPLIT(testcases_clz_uint32[i]),
		       uint32_clz(testcases_clz_uint32[i]));

	printf("\n");

	for (unsigned i=0; i < ARRAY_SIZE(testcases_clz_big); i++)
		printf(testcases_clz_big[i].fmt, big_clz(testcases_clz_big[i].x));

	printf("\n\n");
}



void test_shortmul()
{
	printf("\n");
	printf("test_shortmul\n");
	printf("-------------\n");

	printf("multiply by powers of 2\n");
	for (int i=0; i < 31; i++) {
		printf("2^%d\n", i);

		struct big_int	x;
		x.val[0] = 0xFFFFFFFF;
		x.val[1] = 0xFFFFFFFF;
		x.val[2] = 0xFFFFFFFF;
		x.val[3] = 0xFFFFFFFF;

		uint32_t y;
		y = 1 << i;

		struct big_int mul_res = big_shortmul(x, y, NULL);
		struct big_int shl_res = big_shl(x, i);

		if ((mul_res.val[0] != shl_res.val[0]) ||
		    (mul_res.val[1] != shl_res.val[1]) ||
		    (mul_res.val[2] != shl_res.val[2]) ||
		    (mul_res.val[3] != shl_res.val[3])) {
			printf("*** ERROR!\n");
			printf("x:\n");
			print_big(x);
			printf("multiplicand: %d\n", y);
			printf("mul_res:\n");
			print_big(mul_res);
			printf("shl_res:\n");
			print_big(shl_res);
			printf("\n\n");
		}
	}

	printf("\n");


	printf("multiply by powers of 10\n");
	uint32_t	y = 1;
	for (int i=1; i <= 9; i++) {
		y = y * 10;
		
		printf("123456789012345678901234567901 x 10^%d (%d, 0x%04X_%04X)\n", i, y, SPLIT(y));

		struct big_int	x = {
			.val[0] = 0x0E766C35,
			.val[1] = 0xA286C94F,
			.val[2] = 0x951A9FA3,
			.val[3] = 0x0000000F
		};

		bool	ovf;

		print_big(big_shortmul(x, y, &ovf));
		printf("overflow: %d\n", ovf);
	}

	printf("\n\n");
}


/***/


#define TEST_DIR	"afl/big-int/"


void output_add()
{
	/* the dumb output process automatically takes care of duplicates
	   because all but the last of them will be overwritten.
	 */

	/* create directory */
	mkdir("afl/", 0775);   /* ignore errors */
	mkdir(TEST_DIR, 0775); /* ignore errors -- it's fine if it exists */
	mkdir(TEST_DIR "add/", 0775); /* ignore errors */

	for (unsigned i=0; i < ARRAY_SIZE(testcases_add); i++) {
		char	fname[100];
		sprintf(fname, TEST_DIR "add/%d", i);
		FILE	*f = fopen(fname, "w");

		fwrite(&testcases_add[i].s1, sizeof(struct big_int), 1, f);
		fwrite(&testcases_add[i].s2, sizeof(struct big_int), 1, f);

		fclose(f);
	}
}


void output_neg()
{
	/* the dumb output process automatically takes care of duplicates
	   because all but the last of them will be overwritten.
	 */

	/* create directory */
	mkdir("afl/", 0775);   /* ignore errors */
	mkdir(TEST_DIR, 0775); /* ignore errors -- it's fine if it exists */
	mkdir(TEST_DIR "neg/", 0775); /* ignore errors */

	for (unsigned i=0; i < ARRAY_SIZE(testcases_neg); i++) {
		char	fname[100];
		sprintf(fname, TEST_DIR "neg/%d", i);
		FILE	*f = fopen(fname, "w");

		fwrite(&testcases_neg[i], sizeof(struct big_int), 1, f);

		fclose(f);
	}
}


void output_shl()
{
	/* the dumb output process automatically takes care of duplicates
	   because all but the last of them will be overwritten.
	 */

	/* create directory */
	mkdir("afl/", 0775);   /* ignore errors */
	mkdir(TEST_DIR, 0775); /* ignore errors -- it's fine if it exists */
	mkdir(TEST_DIR "shl/", 0775); /* ignore errors */

	for (unsigned i=0; i < ARRAY_SIZE(testcases_shl); i++) {
		char	fname[100];
		sprintf(fname, TEST_DIR "shl/%d", i);
		FILE	*f = fopen(fname, "w");

		fwrite(&testcases_shl[i].x,    sizeof(struct big_int), 1, f);
		fwrite(&testcases_shl[i].shft, sizeof(int), 1, f);

		fclose(f);
	}
}


void output_clz()
{
	/* the dumb output process automatically takes care of duplicates
	   because all but the last of them will be overwritten.
	 */

	/* create directory */
	mkdir("afl/", 0775);   /* ignore errors */
	mkdir(TEST_DIR, 0775); /* ignore errors -- it's fine if it exists */
	mkdir(TEST_DIR "clz/", 0775); /* ignore errors */

	for (unsigned i=0; i < ARRAY_SIZE(testcases_clz_big); i++) {
		char	fname[100];
		sprintf(fname, TEST_DIR "clz/%d", i);
		FILE	*f = fopen(fname, "w");

		fwrite(&testcases_clz_big[i].x, sizeof(struct big_int), 1, f);

		fclose(f);
	}
}


void output_shortmul()
{
	/* the dumb output process automatically takes care of duplicates
	   because all but the last of them will be overwritten.
	 */

	/* create directory */
	mkdir("afl/", 0775);   /* ignore errors */
	mkdir(TEST_DIR, 0775); /* ignore errors -- it's fine if it exists */
	mkdir(TEST_DIR "shortmul/", 0775); /* ignore errors */

	for (int i=0; i < 31; i++) {
		char	fname[100];
		sprintf(fname, TEST_DIR "shortmul/mul-pow2-%d", i);
		FILE	*f = fopen(fname, "w");

		struct big_int	x = {
			.val[0] = 0xFFFFFFFF,
			.val[1] = 0xFFFFFFFF,
			.val[2] = 0xFFFFFFFF,
			.val[3] = 0xFFFFFFFF
		};

		uint32_t	y = (1 << i);

		fwrite(&x, sizeof(struct big_int), 1, f);
		fwrite(&y, sizeof(uint32_t), 1, f);

		fclose(f);
	}


	uint32_t	y = 1;
	for (int i=1; i < 10; i++) {
		char	fname[100];
		sprintf(fname, TEST_DIR "shortmul/mul-pow10-%d", i);
		FILE	*f = fopen(fname, "w");

		y = y * 10;

		struct big_int	x = {
			.val[0] = 0x0E766C35,
			.val[1] = 0xA286C94F,
			.val[2] = 0x951A9FA3,
			.val[3] = 0x0000000F
		};

		fwrite(&x, sizeof(struct big_int), 1, f);
		fwrite(&y, sizeof(uint32_t), 1, f);

		fclose(f);
	}
}


/***/

void test_add_afl()
{
	struct big_int		x, y;

	if ((fread(&x, sizeof(x), 1, stdin) != 1) ||
	    (fread(&y, sizeof(y), 1, stdin) != 1)) {
		perror("fread");
		fprintf(stderr, "test_add_afl expected two 128-bit numbers.\n");
		exit(EXIT_SUCCESS);	/* lie to AFL */
	}

	bool		ovf;
	struct big_int	res = big_add(x, y, &ovf);

	/* printing the result prevents optimizer from removing big_add() call */
	print_big(res);
	printf("ovf: %d\n", ovf);
}


void test_neg_afl()
{
	struct big_int		x;

	if (fread(&x, sizeof(x), 1, stdin) != 1) {
		perror("fread");
		fprintf(stderr, "test_neg_afl expected a 128-bit number.\n");
		exit(EXIT_SUCCESS);	/* lie to AFL */
	}

	struct big_int	res = big_neg(x);

	/* printing the result prevents optimizer from removing big_add() call */
	print_big(res);
}


void test_shl_afl()
{
	struct big_int		x;
	int			shft;

	if ((fread(&x,    sizeof(x),    1, stdin) != 1) ||
	    (fread(&shft, sizeof(shft), 1, stdin) != 1)) {
		perror("fread");
		fprintf(stderr, "test_shl_afl expected a 128-bit number and a (signed) 32-bit number.\n");
		exit(EXIT_SUCCESS);	/* lie to AFL */
	}

	struct big_int	res = big_shl(x, shft);

	/* printing the result prevents optimizer from removing big_add() call */
	print_big(res);
}


void test_clz_afl()
{
	struct big_int		x;

	if (fread(&x, sizeof(x), 1, stdin) != 1) {
		perror("fread");
		fprintf(stderr, "test_clz_afl expected a 128-bit number.\n");
		exit(EXIT_SUCCESS);	/* lie to AFL */
	}

	int	cnt = big_clz(x);

	/* printing the result prevents optimizer from removing big_add() call */
	printf("%d", cnt);
}


void test_shortmul_afl()
{
	struct big_int		x;
	uint32_t		y;

	if ((fread(&x, sizeof(x), 1, stdin) != 1) ||
	    (fread(&y, sizeof(y), 1, stdin) != 1)) {
		perror("fread");
		fprintf(stderr, "test_shortmul_afl expected a 128-bit number and a 32-bit number.\n");
		exit(EXIT_SUCCESS);	/* lie to AFL */
	}

	bool		ovf;
	struct big_int	res = big_shortmul(x, y, &ovf);

	/* printing the result prevents optimizer from removing big_add() call */
	print_big(res);
	printf("ovf: %d\n", ovf);
}

/***/


void help()
{
	fprintf(stderr, "./test-big-int <mode>\n");
	fprintf(stderr, "\n");
	fprintf(stderr, " mode:\n");
	fprintf(stderr, "   --built-in      built-in test of big ints\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "   --test-add      test add with two 128-bit numbers\n");
	fprintf(stderr, "   --test-neg      test neg with a 128-bit number\n");
	fprintf(stderr, "   --test-shl      test shl with a 128-bit number and a (signed) 16-bit shift\n");
	fprintf(stderr, "   --test-clz      test clz with a 128-bit number\n");
	fprintf(stderr, "   --test-shortmul test mul with a 128-bit number and a 32-bit number\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "   --output-add      write add tests cases to %s\n", TEST_DIR "add/");
	fprintf(stderr, "   --output-neg      write neg tests cases to %s\n", TEST_DIR "neg/");
	fprintf(stderr, "   --output-shl      write shl tests cases to %s\n", TEST_DIR "shl/");
	fprintf(stderr, "   --output-clz      write clz tests cases to %s\n", TEST_DIR "clz/");
	fprintf(stderr, "   --output-shortmul write mul tests cases to %s\n", TEST_DIR "shortmul/");
	fprintf(stderr, "\n");
	fprintf(stderr, "test cases are read from stdin, except for built-in tests.\n");
	fprintf(stderr, "all modes except for --built-in are for testing with American Fuzzy Lop.\n");
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

	if (strcmp(argv[1], "--built-in") == 0) {
		test_add();
		test_neg();
		test_shl();
		test_clz();
		test_shortmul();

	} else if (strcmp(argv[1], "--test-add") == 0) {
		test_add_afl();
	} else if (strcmp(argv[1], "--test-neg") == 0) {
		test_neg_afl();
	} else if (strcmp(argv[1], "--test-shl") == 0) {
		test_shl_afl();
	} else if (strcmp(argv[1], "--test-clz") == 0) {
		test_clz_afl();
	} else if (strcmp(argv[1], "--test-shortmul") == 0) {
		test_shortmul_afl();

	} else if (strcmp(argv[1], "--output-add") == 0) {
		output_add();
	} else if (strcmp(argv[1], "--output-neg") == 0) {
		output_neg();
	} else if (strcmp(argv[1], "--output-shl") == 0) {
		output_shl();
	} else if (strcmp(argv[1], "--output-clz") == 0) {
		output_clz();
	} else if (strcmp(argv[1], "--output-shortmul") == 0) {
		output_shortmul();
	} else {
		help();
	}
#ifdef __AFL_HAVE_MANUAL_CONTROL
	}
#endif

	return EXIT_SUCCESS;
}

