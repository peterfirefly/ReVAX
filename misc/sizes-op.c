/* Copyright 2018  Peter Lund <firefly@vax64.dk>

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

#include "macros.h"
#include "strret.h"

#include "vax-ucode.h"


#include "op-support.h"

/* asm */
static bool parse_reg(int *reg, uint32_t pc, int width, enum ifp ifp)
{
	(void) reg, (void) pc, (void) width, (void) ifp;
	return true;
}

static bool parse_width(int width)
{
	(void) width;
	return true;
}

static bool parse_addr(int32_t *addr, uint32_t pc, int width, enum ifp ifp)
{
	(void) addr, (void) pc, (void) width, (void) ifp;
	return true;
}

static bool parse_pcrel8(int32_t *disp, uint32_t pc, int width, enum ifp ifp)
{
	(void) disp, (void) pc, (void) width, (void) ifp;
	return true;
}

static bool parse_pcrel16(int32_t *disp, uint32_t pc, int width, enum ifp ifp)
{
	(void) disp, (void) pc, (void) width, (void) ifp;
	return true;
}

static bool parse_pcrel32(int32_t *disp, uint32_t pc, int width, enum ifp ifp)
{
	(void) disp, (void) pc, (void) width, (void) ifp;
	return true;
}

static bool parse_imm(struct big_int *imm, uint32_t pc, int width, enum ifp ifp)
{
	(void) imm, (void) pc, (void) width, (void) ifp;
	return true;
}


static bool parse_lit6(int8_t *lit6, uint32_t pc, int width, enum ifp ifp)
{
	(void) lit6, (void) pc, (void) width, (void) ifp;
	return true;
}


static bool parse_disp8(int32_t *disp, uint32_t pc, int width, enum ifp ifp)
{
	(void) disp, (void) pc, (void) width, (void) ifp;
	return true;
}

static bool parse_disp16(int32_t *disp, uint32_t pc, int width, enum ifp ifp)
{
	(void) disp, (void) pc, (void) width, (void) ifp;
	return true;
}

static bool parse_disp32(int32_t *disp, uint32_t pc, int width, enum ifp ifp)
{
	(void) disp, (void) pc, (void) width, (void) ifp;
	return true;
}


/* dis */

/* immediate -- 1/2/4/8 bytes, may be int or f/d/g/h fp */
static struct str_ret str_imm(struct big_int imm, uint32_t pc, int width, enum ifp ifp)
{
	struct str_ret	buf;

	(void) pc;

	switch (ifp) {
	case IFP_INT: if (width == 1) sprintf(buf.str, "%02X", imm.val[0] & 0xFF);
		 else if (width == 2) sprintf(buf.str, "%04X", imm.val[0] & 0xFFFF);
		 else if (width == 4) sprintf(buf.str, "%04X_%04X", SPLIT(imm.val[0]));
		 else if (width == 8) sprintf(buf.str, "%04X_%04X_%04X_%04X", SPLIT(imm.val[1]), SPLIT(imm.val[0]));
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
static struct str_ret str_reg(int reg, uint32_t pc, int width, enum ifp ifp)
{
	struct str_ret	buf;

	(void) pc, (void) width, (void) ifp;

	sprintf(buf.str, "r%d", reg);
	return buf;
}


static struct str_ret str_addr(int32_t addr, uint32_t pc, int width, enum ifp ifp)
{
	struct str_ret	buf;

	(void) addr, (void) pc, (void) width, (void) ifp;

	buf.str[0] = '\0';
	return buf;
}


static struct str_ret str_pcrel(int32_t disp, uint32_t pc, int width, enum ifp ifp)
{
	struct str_ret	buf;

	(void) disp, (void) pc, (void) width, (void) ifp;

	buf.str[0] = '\0';
	return buf;
}


static struct str_ret str_disp(int32_t disp, uint32_t pc, int width, enum ifp ifp)
{
	struct str_ret	buf;

	(void) disp, (void) pc, (void) width, (void) ifp;

	buf.str[0] = '\0';
	return buf;
}


/* dis, sim */

static void expand_lit6(struct fields *fields, int width, enum ifp ifp)
{
	(void) fields, (void) width, (void) ifp;

	switch (ifp) {
	case IFP_INT: fields->imm.val[0] = fields->lit6;
		      fields->imm.val[1] = 0;
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


