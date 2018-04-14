/* Copyright 2018  Peter Lund <firefly@vax64.dk>

   Licensed under GPL v2.

   ---

   ALU test

 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


//#include "vax-ucode.h"

/***/


/* flags -- reading */
#define N(x)	(((x) >> 3) & 1)
#define Z(x)	(((x) >> 2) & 1)
#define V(x)	(((x) >> 1) & 1)
#define C(x)	(((x) >> 0) & 1)

/* flags -- writing */
#define NZVC(N,Z,V,C)	((!!(N) << 3) | (!!(Z) << 2) | (!!(V) << 1) | (!!(C) << 0))

/* clear C */
#define NZV0(x)	((x) &~ 1)


/* The ALU uses mostly unsigned values internally to avoid situations where
   operations on signed integers are undefined behaviour in C.

   Most importantly, it is undefined what happens if signed integers overflow/
   underflow during addition, subtraction, multiplication, division.

   Dividing a signed integer with the value INT_MIN with -1 is -INT_MIN -- which
   is INT_MAX+1 so it can't be represented.  It is undefined what will happen
   if we try.
 */

enum alu_op {
	/* move */
	ALU_MOV,

	/* arith */
	ALU_ADC,
	ALU_SBB,
	ALU_CMP,

	/* logic */
	ALU_AND,
	ALU_BIC,	/* and not */
	ALU_BIS,	/* or      */
	ALU_XOR,
};

/* A straight-forward simple ALU, like we would implement it in TTL

   8/16/32 bit operations -- don't care what the upper bits in dst are

   C input

   NZVC output  -- set based on the lower 8/16/32 bits

   ADC/SBB
   AND/BIC/BIS/XOR (BIC=AND NOT, BIS=OR)
 */
struct alu_res {
	uint32_t	dst;
	uint8_t		NZVC;
	bool		ovf, div0;
};

struct alu_res basic_alu(enum alu_op op, int width, uint32_t src1, uint32_t src2, int cin)
{
	struct basic_alu_res	res;

	assert((width == 1) || (width == 2) || (width == 4));

	switch (op) {
	/* move */
	case ALU_MOV:	res.dst  = src1;
			res.NZVC = ...
			break;

	/* arith */
	case ALU_ADC:	res.dst  = src1 + src2 + cin;
			res.NZVC = ...
			res.ovf  = ...
			break;
	case ALU_SBB:	res.dst  = src1 - src2 - cin;
			res.NZVC = ...
			res.ovf  = ...
			break;
	case ALU_CMP:	res.dst  = src1 - src2 - cin;
			res.NZVC = ...
			res.ovf  = ...
			break;

	/* logic */
	case ALU_AND:	res.dst  = src1 & src2;
			res.NZVC = ...
			break;
	case ALU_BIC:	res.dst  = src1 & ~src2;
			res.NZVC = ...
			break;
	case ALU_BIS:	res.dst  = src1 | src2;
			res.NZVC = ...
			break;
	case ALU_XOR:	res.dst  = src1 ^ src2;
			res.NZVC = ...
			break;
	default:
		assert(0);
	}

	return res;
}


/* 64-bit barrel/funnel shifter

   Implements SHL/ROL

   FIXME: Flags?
 */
uint32_t shifter(uint64_t x, int shft)
{
	assert(shift >= 0);
	assert(shift < 32);

	/* low 32 bits of a 64-bit shift */
	return x << shft;
}


/* Multiplication/division are not implemented in a circuit-realistic way.

   MUL/DIV	b/w/l  f/d/g

   special: *three* inputs
   EMUL

   special: *two* outputs
   EMUL, EDIV

   FIXME: flags?
 */
void muldiv()
{
}


struct alu_res exe_uop(int op, int width, uint32_t src1, uint32_t src2, uint8_t nzvc)
{
	switch (u.op) {
	/* move  */
	case U_MOV:	{
			struct alu_res	res = basic_alu(ALU_MOV, width, src1, src2, 0);
			return res;
			}
	case U_MOVX:	{
			struct alu_res	res = basic_alu(ALU_MOV, width, src1, src2, 0);
			res.nzvc = (res.nzvc & ...) | (nzvc & ...);
			return res;
			}

	/* width */
	case U_SIGNBW:	{
			struct alu_res	res = {.dst = (int32_t)(int8_t)(src1 & 0xFF), .nzvc = ...};
			return res;
			}
	case U_SIGNBL:	{
			struct alu_res	res = {.dst = (int32_t)(int8_t)(src1 & 0xFF), .nzvc = ...};
			return res;
			}
	case U_SIGNWL:	{
			struct alu_res	res = {.dst = (int32_t)(int16_t)(src1 & 0xFFFF), .nzvc = ...};
			return res;
			}
	case U_ZEROBW:	{
			struct alu_res	res = {.dst = (src1 & 0xFF), .nzvc = ...};
			return res;
			}
	case U_ZEROBL:	{
			struct alu_res	res = {.dst = (src1 & 0xFF), .nzvc = ...};
			return res;
			}
	case U_ZEROWL:	{
			struct alu_res	res = {.dst = (src1 & 0xFFFF), .nzvc = ...};
			return res;
			}
	case U_TRUNCWB:	{
			struct alu_res	res = {.dst = (src1 & 0xFF), .nzvc = ..., .ovf=};
			return res;
			}
	case U_TRUNCLB:	{
			struct alu_res	res = {.dst = (src1 & 0xFF), .nzvc = ..., .ovf=};
			return res;
			}
	case U_TRUNCLW:	{
			struct alu_res	res = {.dst = (src1 & 0xFFFF), .nzvc = ..., .ovf=};
			return res;
			}

	/* arith */
	case U_ADD:	{
			return basic_alu(ALU_ADC, width, src1, src2, 0);
			return res;
			}
	case U_SUB:	{
			return basic_alu(ALU_SBB, width, src1, src2, 0);
			return res;
			}
	case U_ADC:	{
			return basic_alu(ALU_ADC, width, src1, src2, 0);
			return res;
			}
	case U_SBB:	{
			return basic_alu(ALU_SBB, width, src1, src2, 0);
			return res;
			}
	case U_CMP:	{
			return basic_alu(ALU_SBB, width, src1, src2, 0);
			return res;
			}

	/* logic */
	case U_AND:	return basic_alu(ALU_AND, width, src1, src2, 0);
	case U_BIC:	return basic_alu(ALU_BIC, width, src1, src2, 0);
	case U_BIS:	return basic_alu(ALU_BIS, width, src1, src2, 0);
	case U_XOR:	return basic_alu(ALU_XOR, width, src1, src2, 0);

	/* shift */
	case U_ASHL:	{
			struct alu_res	res;
			res.dst = shifter(src1, src2);
			res.nzvc = ...
			res.ovf = ...
			return res;
			}
	case U_ASHQ:	{
			struct alu_res	res;
			res.dst = shifter(src1, src2);
			res.nzvc = ...
			res.ovf = ...
			return res;
			}
	case U_ROTL:	{
			struct alu_res	res;
			res.dst = shifter(((uint64_t) src1 << 32) | src1, src2);
			res.nzvc = ...
			return res;
			}

	/* mul/div */
	case U_MUL:	{
			/* 8×8=8, 16×16=16, 32×32=32 */
			struct alu_res	res;
			res.dst = ...
			res.nzvc = ...
			res.ovf  = ...
			return res;
			}
	case U_DIV:	{
			/* 8/8=8, 16/16=16, 32/32=32 */
			struct alu_res	res;
			res.dst = ...
			res.nzvc = ...
			res.ovf = ...
			res.div0 = ...
			return res;
			}

#if 0
/* EMUL, EDIV both produce 2 32-bit results
   EDIV uses 3 32-bit inputs.

   This doesn't fit into the normal ALU datapath.
 */
	case U_EMUL:	{
			/* 32×32 = 64 */
			struct alu_res	res;
			res.dst = ...
			res.nzvc = ...
			return res;
			}
	case U_EDIV:	{
			/* 64/32=32, 64%32=32 */
			struct alu_res	res;
			res.dst = ...
			res.nzvc = ...
			res.div0 = ...
			return res;
			}
#endif
	default:
		assert(0);
	}
}


/***/


/* test basic_alu */
struct alu_test_case {
	enum alu_op	op;
	int32_t		src1, src2;
	uint8_t		nzvc;
	struct alu_res	res;
};


struct alu_test_case test_basic[] = {
	{.op=..., .src1=..., .src2=..., .nzvc=..., .res={.dst=..., .nzvc=...}},
	{.op=..., .src1=..., .src2=..., .nzvc=..., .res={.dst=..., .nzvc=...}},
	{.op=..., .src1=..., .src2=..., .nzvc=..., .res={.dst=..., .nzvc=...}},
	{.op=..., .src1=..., .src2=..., .nzvc=..., .res={.dst=..., .nzvc=...}},
	{.op=..., .src1=..., .src2=..., .nzvc=..., .res={.dst=..., .nzvc=...}},
};


uint32_t width_mask(int width)
{
	return (uint32_t[]) {0, 0x0000_00FF, 0x0000_FFFF, 0, 0xFFFF_FFFF}[width];
}


void test_alu()
{
	for (int i=0; i < ARRAY_SIZE(test_basic); i++) {
		/* ignore higher bits of 8/16-bit ADC/SBC/AND/BIC/BIS/XOR/MOV results */
		struct alu_test_case	test = test_basic[i];
		struct alu_res		res;

		res = basic_alu(test.op, test.width, test.src1, test.src2, test.nzvc);
		if (res.nzvc != test.nzvc) {
			fprintf(stderr, "");
		}
		if ((res.dst & width_mask(test.width)) != test.dst) {
			fprintf(stderr, "");
		}
	}
}


/* test exe_uop */
struct exe_test_case {
	int		op;
	int32_t		src1, src2;
	uint8_t		nzvc;
	struct alu_res	res;
};


struct exe_test_case test_exe[] = {
	{.op=..., .src1=..., .src2=..., .nzvc=..., .res={.dst=..., .nzvc=...}},
	{.op=..., .src1=..., .src2=..., .nzvc=..., .res={.dst=..., .nzvc=...}},
	{.op=..., .src1=..., .src2=..., .nzvc=..., .res={.dst=..., .nzvc=...}},
	{.op=..., .src1=..., .src2=..., .nzvc=..., .res={.dst=..., .nzvc=...}},
	{.op=..., .src1=..., .src2=..., .nzvc=..., .res={.dst=..., .nzvc=...}},
};


void test_exe()
{
	for (int i=0; i < ARRAY_SIZE(test_exe); i++) {
		struct exe_test_case	test = test_exe[i];
		struct alu_res		res;

		res = exe_uop(test.op, test.width, test.src1, test.src2, test.nzvc);
		if (res.nzvc != test.nzvc) {
			fprintf(stderr, "");
		}
		if (res.dst != test.dst) {
			fprintf(stderr, "");
		}
	}
}


/**/


#define TEST_DIR	"afl-op/"

void alu_output()
{
	/* the dumb output process automatically takes care of duplicates
	   because all but the last of them will be overwritten.
	 */

	/* create directory */
	mkdir(TEST_DIR, 0775); /* ignore errors -- it's fine if it exists */


	/* create files */
	for (unsigned i=0; i < ARRAY_SIZE(test_alu); i++) {
		/* file name = hex bytes + '.auto' */
		char	fname[1000] = {};

		strcpy(fname, TEST_DIR);
		if (tests[i].cnt == 0) {
			sprintf(fname+strlen(fname), "zero-bytes");
		} else {
			for (int j=0; j < tests[i].cnt; j++) {
				assert(strlen(fname) + 100 < sizeof(fname));
				sprintf(fname+strlen(fname), "%02X", tests[i].b[j]);
			}
		}
		sprintf(fname+strlen(fname), ".alu");

		/* create file */
		printf("Writing to %s\n", fname);

		FILE	*f;
		if ((f = fopen(fname, "w")) == NULL) {
			fprintf(stderr, "Can't write to '%s'.\n", fname);
			perror("fopen");
			exit(1);
		}

		/* write */
		for (int j=0; j < tests[i].cnt; j++)
			fprintf(f, "%c", tests[i].b[j]);

		/* close */
		fclose(f);
	}
}


void exe_output()
{
	/* the dumb output process automatically takes care of duplicates
	   because all but the last of them will be overwritten.
	 */

	/* create directory */
	mkdir(TEST_DIR, 0775); /* ignore errors -- it's fine if it exists */


	/* create files */
	for (unsigned i=0; i < ARRAY_SIZE(test_exe); i++) {
		/* file name = hex bytes + '.auto' */
		char	fname[1000] = {};

		strcpy(fname, TEST_DIR);
		if (test_exe[i].cnt == 0) {
			sprintf(fname+strlen(fname), "zero-bytes");
		} else {
			for (int j=0; j < tests[i].cnt; j++) {
				assert(strlen(fname) + 100 < sizeof(fname));
				sprintf(fname+strlen(fname), "%02X", tests[i].b[j]);
			}
		}
		sprintf(fname+strlen(fname), ".exe");

		/* create file */
		printf("Writing to %s\n", fname);

		FILE	*f;
		if ((f = fopen(fname, "w")) == NULL) {
			fprintf(stderr, "Can't write to '%s'.\n", fname);
			perror("fopen");
			exit(1);
		}

		/* write */
		for (int j=0; j < tests[i].cnt; j++)
			fprintf(f, "%c", tests[i].b[j]);

		/* close */
		fclose(f);
	}
}


/**/


struct alu_input {
	int		width;
};


struct alu_input alu_input_bin()
{
	struct afl_input	afl = {};

	afl.cnt = fread(afl.b, 1, MAX_OPLEN, stdin);

	afl.width = 1;
	afl.ifp   = IFP_INT;

	return afl;
}


struct exe_input {
	int		width;
};


struct exe_input exe_input_bin()
{
	struct afl_input	afl = {};

	afl.cnt = fread(afl.b, 1, MAX_OPLEN, stdin);

	afl.width = 1;
	afl.ifp   = IFP_INT;

	return afl;
}


void test_alu_afl()
{
}


void test_exe_afl()
{
}


/***/


void help()
{
	fprintf(stderr, "./test-alu <mode>\n");
	fprintf(stderr, "\n");
	fprintf(stderr, " mode:\n");
	fprintf(stderr, "   --built-in    built-in test of alu/exe\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "   --test-alu    test basic ALU with a byte sequence from stdin\n");
	fprintf(stderr, "   --test-exe    test exe with a byte sequence from stdin\n");
	fprintf(stderr, "   --output-alu  write alu tests cases to %s directory\n", TEST_DIR);
	fprintf(stderr, "   --output-exe  write exe tests cases to %s directory\n", TEST_DIR);
	fprintf(stderr, "\n");
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
		test_alu();
		test_exe();
	} else if (strcmp(argv[1], "--test-alu") == 0) {
		test_alu_afl();
	} else if (strcmp(argv[1], "--test-exe") == 0) {
		test_exe_afl();
	} else if (strcmp(argv[1], "--output-alu") == 0) {
		output_alu();
	} else if (strcmp(argv[1], "--output-exe") == 0) {
		output_exe();
	} else {
		help();
	}
#ifdef __AFL_HAVE_MANUAL_CONTROL
	}
#endif

	return EXIT_SUCCESS;
}

