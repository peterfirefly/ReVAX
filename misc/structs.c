/* Copyright 2018  Peter Lund <firefly@vax64.dk>\

   Licensed under GPL v2.

   ---

   structure passing testbench

   struct returns are clearly nicer for int × int pairs.

   ---

   gcc-xxx/clang-xxx -c -g -W -Wall src/structs.c
   objdump -d -M intel structs.o

   gcc-xxx/clang-xxx -S -masm=intel -fverbose-asm -W -Wall src/structs.c
   less structs.s
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "vax-ucode.h"

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

#include "shared.h"

struct xxx {
	int	cnt;
	int	cl;
};

struct xxx t_struct(uint8_t b[MAX_OPLEN], struct fields *fields, int width, enum ifp ifp)
{
	/* 00xxxxxx */
	if ((b[0] & 0xC0) == 0x00) {
		fields->lit6 = b[0] & ~0xC0;
		expand_lit6(fields, width, ifp);
		return (struct xxx) {.cnt = 1, .cl = CLASS_IMM};
	}

	/* 0100xxxx 0110xxxx */
	if ((b[0] & 0xF0) == 0x40) {
		if ((b[1] & 0xF0) == 0x60) {
			fields->Rx = b[0] & ~0xF0;
			fields->Rn = b[1] & ~0xF0;
			return (struct xxx) {.cnt = 2, .cl = LBL_ADDR__RN_______XINDEX_RX__};
		}
	}

	/* 0100xxxx 0111xxxx */
	if ((b[0] & 0xF0) == 0x40) {
		if ((b[1] & 0xF0) == 0x70) {
			fields->Rx = b[0] & ~0xF0;
			fields->Rn = b[1] & ~0xF0;
			return (struct xxx) {.cnt = 2, .cl = LBL_ADDR____RN_____XINDEX_RX__};
		}
	}

	/* 0100xxxx 1000xxxx */
	if ((b[0] & 0xF0) == 0x40) {
		if ((b[1] & 0xF0) == 0x80) {
			fields->Rx = b[0] & ~0xF0;
			fields->Rn = b[1] & ~0xF0;
			return (struct xxx) {.cnt = 2, .cl = LBL_ADDR__RNXX_____XINDEX_RX__};
		}
	}

	/* 0100xxxx 10011111 <addr:32> */
	if ((b[0] & 0xF0) == 0x40) {
		if ((b[1] & 0xFF) == 0x9F) {
			fields->Rx = b[0] & ~0xF0;
			fields->addr = L(b[2]);
			return (struct xxx) {.cnt = 2, .cl = LBL_ADDR__ADDR_____XINDEX_RX__};
		}
	}
	return (struct xxx) {.cnt = -1};
}


int t_nostruct(uint8_t b[MAX_OPLEN], struct fields *fields, int width, enum ifp ifp, int *class_label)
{
	/* 00xxxxxx */
	if ((b[0] & 0xC0) == 0x00) {
		fields->lit6 = b[0] & ~0xC0;
		expand_lit6(fields, width, ifp);
		*class_label = CLASS_IMM;
		return 1;
	}

	/* 0100xxxx 0110xxxx */
	if ((b[0] & 0xF0) == 0x40) {
		if ((b[1] & 0xF0) == 0x60) {
			fields->Rx = b[0] & ~0xF0;
			fields->Rn = b[1] & ~0xF0;
			*class_label = LBL_ADDR__RN_______XINDEX_RX__;
			return 2;
		}
	}

	/* 0100xxxx 0111xxxx */
	if ((b[0] & 0xF0) == 0x40) {
		if ((b[1] & 0xF0) == 0x70) {
			fields->Rx = b[0] & ~0xF0;
			fields->Rn = b[1] & ~0xF0;
			*class_label = LBL_ADDR____RN_____XINDEX_RX__;
			return 2;
		}
	}

	/* 0100xxxx 1000xxxx */
	if ((b[0] & 0xF0) == 0x40) {
		if ((b[1] & 0xF0) == 0x80) {
			fields->Rx = b[0] & ~0xF0;
			fields->Rn = b[1] & ~0xF0;
			*class_label = LBL_ADDR__RNXX_____XINDEX_RX__;
			return 2;
		}
	}

	/* 0100xxxx 10011111 <addr:32> */
	if ((b[0] & 0xF0) == 0x40) {
		if ((b[1] & 0xFF) == 0x9F) {
			fields->Rx = b[0] & ~0xF0;
			fields->addr = L(b[2]);
			*class_label = LBL_ADDR__ADDR_____XINDEX_RX__;
			return 6;
		}
	}
	return -1;
}


/***/

/* Faking a string return in C99 with struct return.  Works great!

   gcc-5.4, no opt:
     Somewhat inefficient.   

   gcc-7, no opt:
     Still somewhat inefficient.  Better, though.

   gcc-7, -O2:
     Quite effifient.  Confusing code: it xor's with a thread local value and
     then does a jne a few instructions later?  I think it's stack protection
     code (with a canary).

   gcc-8, no opt::
     Not much different from gcc-7.

   clang-5.0, no opt:
     Somewhat inefficient.

   clang-5.0, -O2:
     Quite efficient.  No stack protection w/ canary.

   clang-3.8, -O2:
     Quite efficient.  No stack protection w/ canary.

 */

struct reg_str {
	char	str[10];
};

static struct reg_str struct_regstr(uint8_t reg)
{
	struct reg_str	buf;

	switch (reg) {
	/* 16 GPRs -- r0..r15 */
	case  0:  case  1:  case  2:  case  3:
	case  4:  case  5:  case  6:  case  7:
	case  8:  case  9:  case 10:  case 11:
	case 12:  case 13:  case 14:  case 15:
		sprintf(buf.str, "r%d", reg);
		break;

	/* p1..p7 (set in pre) */
	case 16:  case 17:  case 18:  case 19:
	case 20:  case 21:  case 22:
		sprintf(buf.str, "o%d", reg-15);
		break;

	/* e1..e3 (set in exe) */
	case 23:
	case 24:
	case 25:
		sprintf(buf.str, "e%d", reg-22);
		break;

	/* t0..t1 (temporaries) */
	case 26:  case 27:
		sprintf(buf.str, "t%d", reg-26);
		break;

	/* psl (program status long) */
	case 28:
		sprintf(buf.str, "psl");
		break;

	/* template stuff, these are placeholder values */
	case 40: sprintf(buf.str, "<Rn>");    break;
	case 41: sprintf(buf.str, "<Rx>");    break;
	case 42: sprintf(buf.str, "<pre>");   break;
	case 43: sprintf(buf.str, "<exe>");   break;
	case 44: sprintf(buf.str, "<reg>");   break;
	default:
		assert(0);
	};

	return buf;
}



static const char *ccstr(int cc)
{
	return ccname[cc];
}


static const char *lenstr(int len)
{
	switch (len) {
	case U_WIDTH_8:		return " 8";
	case U_WIDTH_16:	return "16";
	case U_WIDTH_32:	return "32";
	case U_WIDTH_TEMPL:	return "<width>";
	default:
		assert(0);
	}
}


static const char *flagstr(int flags)
{
	assert((flags == 0) || (flags == 1));
	return ((const char *[]) {"arch", "µ"})[flags];
}


void dis_uinstr(int i, int uop_cnt, struct uop uop[])
{
	while (uop_cnt--) {
		struct uop	u = uop[i];

		if (u.op == U_BCC_EXC) {
			if (u.utarget & U_EXC_MASK)
				printf("exc");
			else
				printf("bcc");
		} else {
			printf("%3d: %8s ", i, uopname[u.op]);
		}

		switch (u.op) {
		/* no operands */
		case U_NOP:
		case U_STOP:
		case U_COMMIT:
		case U_ROLLBACK:
			break;

		/* imm32, src */
		case U_MBZ:			
			printf("%04X_%04X, %s", SPLIT(u.imm), struct_regstr(u.s1).str);
			break;

		/* imm32, dst */
		case U_IMM:
			printf("%04X_%04X, %s", SPLIT(u.imm), struct_regstr(u.dst).str);
			break;

		/* cc, utarget -- flags */
		case U_BCC_EXC:
			printf("%s, %d    -- %s",
				ccstr(u.cc), u.utarget & ~ U_EXC_MASK, flagstr(u.flags));
			break;

		/* src */
		case U_JMP:
			printf("%s", struct_regstr(u.s1).str);
			break;

		/* dst */
		case U_READPC:
			printf("%s", struct_regstr(u.dst).str);
			break;

		/* src, src', dst -- len flags */
		case U_MOV:
		case U_MOV_:
			if (u.s1 == u.s2)
				printf("%s, %s  -- %s %s",
					struct_regstr(u.s1).str, struct_regstr(u.dst).str,
					lenstr(u.width), flagstr(u.flags));
			else
				printf("%s, %s, %s  -- %s %s",
					struct_regstr(u.s1).str, struct_regstr(u.s2).str, struct_regstr(u.dst).str,
					lenstr(u.width), flagstr(u.flags));
			break;
		}
	}
}

int main()
{
	return EXIT_SUCCESS;
}

