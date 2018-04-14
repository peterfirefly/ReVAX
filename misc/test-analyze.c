/* Copyright 2018  Peter Lund <firefly@vax64.dk>

   Licensed under GPL v2.

 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "vax-instr.h"
#include "vax-ucode.h"



/* worst-case is octo/h immediate operands: 1 opspec + 16 data bytes */
#define MAX_OPLEN	17

enum vax_format { FMT_VAX, FMT_SANE };
enum ifp	{ IFP_INT, IFP_F, IFP_D, IFP_G, IFP_H };


struct fields {
	int		Rn, Rx;
	int32_t		addr;
	int32_t		disp;
	int32_t		imm[4];	/* 4 if octo/h supported */
	int8_t		lit6;
};

/* operand class

   IMM/REG/ADDR

   anything >= 0 is interpreted as ADDR class (where the actual value is the
   microcode address).
 */
#define CLASS_IMM	-2
#define CLASS_REG	-1


#include "op-support.h"

#define STATIC	static

#include "op-sim.h"
#include "op-val.h"


struct analysis {
	int		bytes;
	int		precnt, postcnt;
	uint32_t	startpc;

	struct {
		uint32_t	startpc;
		int		prereg;
		unsigned	frag[2];	/* 0 = no fragment */
	}	pre[6];
	struct {
		unsigned	frag;
	}	exe;
	struct {
		unsigned	frag;
	}	post[2];
};


/* generate a fragment list for the instruction

   pre[1..6]:  startpc, frag[], prereg
   post[1..2]: prereg

   return instruction size
 */
struct analysis analyze(uint8_t b[100], uint32_t pc)
{
	struct analysis	analysis = {.precnt=0, .postcnt=0, .startpc=pc};

	/* opcode */

	/* run through ops */
	for (int i=0; i < 6; i++) {
#if 0
		sim_op_decode();
		val_op();
#endif
	}

	return analysis;
}

#include "dis-uop.h"


void dump_analysis(struct analysis analysis)
{
	/* instruction bytes */
	/* instruction name */
	/* dump fragments w/ startpc, prereg, µop disassembly */
}


struct {
	uint8_t	b[60];
} cases[] = {
	{.b = {0xC1,0x52,0x53,0x54}},	/* ADDL3   r2, r3, r4	*/
	{.b = {0xC3,0x52,0x53,0x54}},	/* SUBL3   r2, r3, r4	*/
	{.b = {0xC0,0x52,0x55}},	/* ADDL2   r2, r5	*/
	{.b = {0xC2,0x52,0x55}},	/* SUBL2   r2, r5	*/

	{.b = {0xC1,0x8F,0xBE,0xBA,0xFE,0xCA,0x53,0x54}},	/* ADDL3   0xCAFEBABE, r3, r4	*/
	{.b = {0xC3,0x8F,0xBE,0xBA,0xFE,0xCA,0x53,0x54}},	/* SUBL3   0xCAFEBABE, r3, r4	*/
	{.b = {0xC0,0x8F,0xBE,0xBA,0xFE,0xCA,0x55}},		/* ADDL2   0xCAFEBABE, r5	*/
	{.b = {0xC2,0x8F,0xBE,0xBA,0xFE,0xCA,0x5}},		/* SUBL2   0xCAFEBABE, r5	*/

	{.b = {0xC1,0x52,0x8F,0xBE,0xBA,0xFE,0xCA,0x54}},	/* ADDL3   r2, 0xCAFEBABE, r4	*/
	{.b = {0xC3,0x52,0x8F,0xBE,0xBA,0xFE,0xCA,0x54}},	/* SUBL3   r2, 0xCAFEBABE, r4	*/

	{.b = {0xC1,0x8F,0xBE,0xBA,0xFE,0xCA,0x8F,0xEF,0xBE,0xAD,0xDE,0x54}},	/* ADDL3   0xCAFEBABE, 0xDEADBEEF, r4	*/
	{.b = {0xC3,0x8F,0xBE,0xBA,0xFE,0xCA,0x8F,0xEF,0xBE,0xAD,0xDE,0x54}},	/* SUBL3   0xCAFEBABE, 0xDEADBEEF, r4	*/

	{.b = {0xC1,0x8F,0xBE,0xBA,0xFE,0xCA,0x56,0x57}},	/* ADDL3   0xCAFEBABE, r6, r7	*/

	{.b = {0xC1,0x64,0x52,0x57}},	/* ADDL3   r2, [r4], r7 */

	{.b = {0xFF}},			/* STOP			*/
};


int main(int argc, char *argv[argc])
{
	return EXIT_SUCCESS;
}

