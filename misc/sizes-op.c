/* Copyright 2018  Peter Lund <firefly@vax64.dk>\

   Licensed under GPL v2.

   ---

   size tester for operand encoder/decoders

   supposed to be compiled to a .o just for the purpose of running 'size' and
   'objdump'.

   almost all the code is copied from src/test-ops.c.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shared.h"

#include "vax-ucode.h"

enum vax_format { FMT_VAX, FMT_SANE };
enum ifp	{ IFP_INT, IFP_F, IFP_D, IFP_G, IFP_H };


struct fields {
	int		Rn, Rx;
	int32_t		addr;
	int32_t		disp;
	int32_t		imm[2];
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

/* asm */
static bool parse_reg(int *reg)
{
	(void) reg;
	return true;
}

static bool parse_width(int width)
{
	(void) width;
	return true;
}

static bool parse_addr(int32_t *addr)
{
	(void) addr;
	return true;
}

static bool parse_imm(int32_t (*imm)[2])
{
	(void) imm;
	return true;
}


static bool parse_lit6(int8_t *lit6)
{
	(void) lit6;
	return true;
}


static bool parse_disp(int32_t *disp)
{
	(void) disp;
	return true;
}


/* dis */

/* immediate -- 1/2/4/8 bytes, may be int or f/d/g/h fp */
static struct str_ret str_imm(int32_t imm[2], int width, enum ifp ifp)
{
	struct str_ret	buf;

	switch (ifp) {
	case IFP_INT: if (width == 1) sprintf(buf.str, "%02X", imm[0] & 0xFF);
		 else if (width == 2) sprintf(buf.str, "%04X", imm[0] & 0xFFFF);
		 else if (width == 4) sprintf(buf.str, "%04X_%04X", SPLIT(imm[0]));
		 else if (width == 8) sprintf(buf.str, "%04X_%04X_%04X_%04X", SPLIT(imm[1]), SPLIT(imm[0]));
		 else assert(0);
		 break;
	case IFP_F: assert(width== 4); sprintf(buf.str, "sign=.., exp=.., fraction="); break;
	case IFP_D: assert(width== 8); break;
	case IFP_G: assert(width== 8); break;
	case IFP_H: assert(width==16); break;
	default:
		assert(0);
	}
	return buf;
}


/* register name (r0, r5, pc, ...) */
static struct str_ret str_reg(int reg, int width, enum ifp ifp)
{
	struct str_ret	buf;

	(void) width, (void) ifp;

	sprintf(buf.str, "r%d", reg);
	return buf;
}


static struct str_ret str_addr(int32_t addr, int width, enum ifp ifp)
{
	struct str_ret	buf;

	(void) addr, (void) width, (void) ifp;

	buf.str[0] = '\0';
	return buf;
}


static struct str_ret str_label(int32_t addr, int width, enum ifp ifp)
{
	struct str_ret	buf;

	(void) addr, (void) width, (void) ifp;

	buf.str[0] = '\0';
	return buf;
}


static struct str_ret str_disp(int32_t disp, int width, enum ifp ifp)
{
	struct str_ret	buf;

	(void) disp, (void) width, (void) ifp;

	buf.str[0] = '\0';
	return buf;
}


/* dis, sim */

static void expand_lit6(struct fields *fields, int width, enum ifp ifp)
{
	(void) fields, (void) width, (void) ifp;

	switch (ifp) {
	case IFP_INT: fields->imm[0] = fields->lit6;
		      fields->imm[1] = 0;
		      break;
	case IFP_F: assert(width== 4); break;
	case IFP_D: assert(width== 8); break;
	case IFP_G: assert(width== 8); break;
	case IFP_H: assert(width==16); break;
	default:
		assert(0);
	}
}

/* need the functions to be exported, i.e. not static */

#define STATIC

#include "op-asm.h"
#include "op-dis.h"
#include "op-sim.h"


