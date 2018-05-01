/* Copyright 2018  Peter Lund <firefly@vax64.dk>

   Licensed under GPL v2.

   ---

   operand handling support code -- operand encoding

   called by autogenerated code for asm.

 */

#ifndef OP_ASM_SUPPORT__H
#define OP_ASM_SUPPORT__H

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "op-support.h"

#include "big-int.h"

#include "parse.h"

#include "fp.h"

#include "vax-instr.h"

/***/


/* asm */
static bool parse_reg(int *reg, uint32_t pc, int width, enum ifp ifp)
{
	(void) pc, (void) width, (void) ifp;

	if (!parse_ok)
		return false;

	char	*s;

	parse_begin();
	if (!parse_id(&s)) {
		parse_rollback();
		parse_ok = false;
		return false;
	}

	for (unsigned i=0; i < ARRAY_SIZE(regs); i++) {
		if (strcmp(s, regs[i].name) == 0) {
			free(s);
			parse_commit();
			*reg = regs[i].no;
			return true;
		}
	}

	free(s);
	parse_rollback();
	parse_ok = false;
	return false;
}

static bool parse_width(int width)
{
	/* don't call parse_skipws() -- it's up to the pattern in
	   operands.spec to decide whether to allow whitespace before
	   parse_width() or not.

           parse_ch() don't implicitly skip whitespace.
	 */
	switch (width) {
	case  1: return parse_ch('1');
	case  2: return parse_ch('2');
	case  4: return parse_ch('4');
	case  8: return parse_ch('8');
	case 16: return parse_ch('1') && parse_ch('6');
	default:
		UNREACHABLE();
	}
}

static unsigned fromhex(char ch)
{
	if     ((ch >= '0') && (ch <= '9'))
		return ch-'0';
	else if ((ch >= 'A') && (ch <= 'F'))
		return ch - 'A' + 10;
	else if ((ch >= 'a') && (ch <= 'f'))
		return ch - 'a' + 10;

	UNREACHABLE();
}



static bool all_high_ones(struct big_int x, int width)
{
	switch (width) {
	case  1:	return ( x.val[3]        == 0xFFFFFFFF) &&
			       ( x.val[2]        == 0xFFFFFFFF) &&
			       ( x.val[1]        == 0xFFFFFFFF) &&
			       ((x.val[0] >>  8) ==   0xFFFFFF);
	case  2:	return ( x.val[3]        == 0xFFFFFFFF) &&
			       ( x.val[2]        == 0xFFFFFFFF) &&
			       ( x.val[1]        == 0xFFFFFFFF) &&
			       ((x.val[0] >> 16) ==     0xFFFF);
	case  4:	return ( x.val[3]        == 0xFFFFFFFF) &&
			       ( x.val[2]        == 0xFFFFFFFF) &&
			       ( x.val[1]        == 0xFFFFFFFF);
	case  8:	return ( x.val[3]        == 0xFFFFFFFF) &&
			       ( x.val[2]        == 0xFFFFFFFF);
	case 16:	return true;
	default:
		UNREACHABLE();
	}
}


static bool all_high_zeros(struct big_int x, int width)
{
	switch (width) {
	case  1:	return ( x.val[3]        == 0) &&
			       ( x.val[2]        == 0) &&
			       ( x.val[1]        == 0) &&
			       ((x.val[0] >>  8) == 0);
	case  2:	return ( x.val[3]        == 0) &&
			       ( x.val[2]        == 0) &&
			       ( x.val[1]        == 0) &&
			       ((x.val[0] >> 16) == 0);
	case  4:	return ( x.val[3]        == 0) &&
			       ( x.val[2]        == 0) &&
			       ( x.val[1]        == 0);
	case  8:	return ( x.val[3]        == 0) &&
			       ( x.val[2]        == 0);
	case 16:	return true;
	default:
		UNREACHABLE();
	}
}


/* width = size in bytes */
static bool valid_bigint(struct big_int x, int width)
{
	/* valid bigints contain a signextended form of the lower 1/2/4/8/16
	   bytes, that is, the upper bits are either all 0 or all 1.

FIXME should check that the upper bits are copies of the sign bit?
	 */

	return all_high_ones(x, width) || all_high_zeros(x, width);
}


/* width = size in bytes */
static bool parse_bigint(struct big_int *x, int width)
{
	/* [+-][0-9][0-9_]*
	   [+-]0x[0-9A-Fa-f][0-9A-Fa-f_]*
	 */

	struct big_int	tmp = {{0}};
	bool		neg = false;
	char		ch;

	if (!parse_ok) {
		return false;
	}

	/* transaction for the whole integer */
	parse_begin();

	parse_skipws();

	/* sign? */
	parse_begin();
	if (parse_oneof_ch("+-", &ch)) {
		parse_commit();
		neg = (ch == '-');
	} else {
		parse_rollback();
	}

	/* hex? */
	parse_begin();			/* start hex transaction */
	if (parse_symbol("0x")) {
		goto try_hex;
	} else {
		parse_rollback();
	}

	parse_begin();			/* start another hex transaction */
	if (!parse_symbol("^X")) {
		parse_rollback();	/* end of hex transaction */
		goto try_dec;
	}

try_hex:
	if (!parse_oneof_ch("0123456789ABCDEFabcdef", &ch)) {
		parse_rollback();	/* end of hex transaction */
		parse_rollback();	/* end of integer transaction */
		parse_ok = false;
		return false;
	}

	tmp = (struct big_int) {.val[0] = fromhex(ch)};
	while (1) {
		parse_begin();
		if (parse_oneof_ch("0123456789ABCDEFabcdef_", &ch)) {
			parse_commit();	/* end of [0-9A-Fa-f] transaction */

			if (ch == '_')
				continue;

			if ((tmp.val[3] >> 28) != 0) {
				/* overflow */
				parse_rollback();	/* end of hex transaction */
				parse_rollback();	/* end of integer transaction */
				parse_ok = false;
				return false;
			}

			tmp = big_add(big_shl(tmp, 4), (struct big_int) {.val[0] = fromhex(ch)}, NULL);
		} else {
			parse_rollback();

			/* negative? */
			if (neg)
				tmp = big_neg(tmp);

			/* check for overflow */
			if (!valid_bigint(tmp, width)) {
				parse_rollback();	/* hex transaction */
				parse_rollback();	/* integer transaction */
				parse_ok = false;
				return false;
			}

			parse_commit();		/* hex transaction */
			parse_commit();		/* integer transaction */
			*x = tmp;
			return true;
		}
	}

	/***/

try_dec:
	parse_begin();
	if (!parse_oneof_ch("0123456789", &ch)) {
//printf("parse_bigint() -- a fail\n");
		parse_rollback();	/* end of dec transaction */
		parse_rollback();	/* end of integer transaction */
		parse_ok = false;
		return false;
	}


	tmp = (struct big_int) {.val[0] = ch - '0'};

	while (1) {
		parse_begin();
		if (parse_oneof_ch("0123456789_", &ch)) {
			parse_commit();

			if (ch == '_')
				continue;

			bool		ovf;

			struct big_int	x10 = big_shortmul(tmp, 10, &ovf);
			if (ovf) {
				parse_rollback();	/* dec transaction */
				parse_rollback();	/* integer transaction */
				parse_ok = false;
				return false;
			}

			struct big_int x10ch = big_add(x10, (struct big_int) {.val[0]= ch - '0'}, &ovf);
			if (ovf) {
				parse_rollback();	/* dec transaction */
				parse_rollback();	/* integer transaction */
				parse_ok = false;
				return false;
			}
			tmp = x10ch;
		} else {
			parse_rollback();

			/* negative? */
			if (neg)
				tmp = big_neg(tmp);

			/* overflow? */
			if (!valid_bigint(tmp, width)) {
//printf("parse_bigint() -- !valid bigint fail\n");
				parse_rollback();	/* dec transaction */
				parse_rollback();	/* integer transaction */
				parse_ok = false;
				return false;
			}

//printf("parse_bigint() -- yes!\n");
			parse_commit();		/* dec transaction */
			parse_commit();		/* integer transaction */
			*x = tmp;
			return true;
		}
	}
}


static bool parse_fp(struct big_int *imm, char type)
{
	(void) imm, (void) type;

	/* check format, then call fp_from_str() */

	/* snn
	   snn.nn
	   snn.

	   snnEsnn
	   snn.nnEsnn
	   snn.Esnn

	   s  = [+-]
	   nn = [0-9]+
	 */

	char	s[100] = {[0] = '\0'}, *p = s;
	char	ch;

	if (!parse_ok) {
		return false;
	}

	parse_skipws();

	/* s... */
	parse_begin();
	if (parse_oneof_ch("+-", &ch)) {
		*p++ = ch;  *p = '\0';
		parse_commit();
	} else
		parse_rollback();

	/* ...nn */
	if (!parse_oneof_ch("0123456789", &ch))
		return false;

	*p++ = ch;  *p = '\0';
	while (1) {
		parse_begin();
		if (parse_oneof_ch("0123456789", &ch)) {
			parse_commit();
			*p++ = ch;  *p = '\0';
		} else {
			parse_rollback();
			break;
		}
	}

	parse_begin();
	if (parse_oneof_ch(".", &ch)) {
		parse_commit();
		*p++ = ch;  *p = '\0';

		while (1) {
			parse_begin();
			if (parse_oneof_ch("0123456789", &ch)) {
				parse_commit();
				*p++ = ch;  *p = '\0';
			} else {
				parse_rollback();
				break;
			}
		}
	} else {
		parse_rollback();
	}

	parse_begin();
	if (parse_oneof_ch("E", &ch)) {
		parse_commit();
		*p++ = ch;  *p = '\0';

		parse_begin();
		if (parse_oneof_ch("+-", &ch)) {
			*p++ = ch;  *p = '\0';
			parse_commit();
		} else {
			parse_rollback();
		}

		if (!parse_oneof_ch("0123456789", &ch)) {
			return false;
		}

		*p++ = ch;  *p = '\0';

		while (1) {
			parse_begin();
			if (parse_oneof_ch("0123456789", &ch)) {
				parse_commit();
				*p++ = ch;  *p = '\0';
			} else {
				parse_rollback();
				break;
			}
		}
	} else {
		parse_rollback();
	}

//fprintf(stderr, "|%s|\n", s);
	return fp_from_str(imm, s, type);
}


static bool parse_imm(struct big_int *imm, uint32_t pc, int width, enum ifp ifp)
{
	(void) pc;

	switch (ifp) {
	case IFP_INT:
		{
		struct big_int	x;

		if (!parse_bigint(&x, width)) {
			return false;
		}

		*imm = x;
		return true;
		}
	case IFP_F:	return parse_fp(imm, 'f');
	case IFP_D:	return parse_fp(imm, 'd');
	case IFP_G:	return parse_fp(imm, 'g');
	case IFP_H:	return parse_fp(imm, 'h');
	default:
		UNREACHABLE();
	}
}


static bool parse_lit6(int8_t *lit6, uint32_t pc, int width, enum ifp ifp)
{
	struct big_int	x;
	(void) pc;

	if (!parse_imm(&x, pc, width, ifp))
		return false;

	/* check that the imm fits in a lit6, convert it if it does */
	switch (ifp) {
	case IFP_INT:
		switch (width) {
		case  1:
		case  2:
		case  4:
			if ((x.val[0] & ~0x3F) != 0) {
				parse_ok = false;
				return false;
			}
			*lit6 = x.val[0];
			return true;
		case  8:
			if (((x.val[0] & ~0x3F) != 0) || (x.val[1] != 0)) {
				parse_ok = false;
				return false;
			}
			*lit6 = x.val[0];
			return true;
		case 16:
			if (((x.val[0] & ~0x3F) != 0) || (x.val[1] != 0) || (x.val[2] != 0) || (x.val[2] != 0)) {
				parse_ok = false;
				return false;
			}
			*lit6 = x.val[0];
			return true;
		default:
			UNREACHABLE();
		}
		break;
	case IFP_F:
		assert(width == 4);
		if ((x.val[0] & ~0x3F0) != 0x00004000) {
			parse_ok = false;
			return false;
		}
		*lit6 = (x.val[0] >> 4) & 0x3F;
		return true;
	case IFP_D:
		assert(width == 8);
		if (((x.val[0] & ~0x3F0) != 0x00004000) || (x.val[1] != 0)) {
			parse_ok = false;
			return false;
		}
		*lit6 = (x.val[0] >> 4) & 0x3F;
		return true;
	case IFP_G:
		assert(width == 8);
		if (((x.val[0] & ~0x7E) != 0x00004000) || (x.val[1] != 0)) {
#if 0
printf("[0]: %04X_%04X\n", SPLIT(x.val[0]));
printf("[1]: %04X_%04X\n", SPLIT(x.val[1]));
printf("[2]: %04X_%04X\n", SPLIT(x.val[2]));
printf("[3]: %04X_%04X\n", SPLIT(x.val[3]));
#endif
			parse_ok = false;
			return false;
		}
		*lit6 = (x.val[0] >> 1) & 0x3F;
		return true;
	case IFP_H:
		assert(width == 16);
		if (((x.val[0] & ~0xE0000007) != 0x00004000) || (x.val[1] != 0) || (x.val[2] != 0) || (x.val[3] != 0)) {
			parse_ok = false;
			return false;
		}
		*lit6 = ((x.val[0] << 3) | (x.val[0] >> 29)) & 0x3F;
		return true;
	default:
		UNREACHABLE();
	}
}


/* PC-relative address */
static bool parse_pcrel8(int32_t *disp, uint32_t pc, int width, enum ifp ifp)
{
	(void) width, (void) ifp;
	struct big_int	x;

	if (parse_bigint(&x, 4)) {
		uint32_t	tmp;

		tmp = x.val[0] - pc;
		if (((tmp >> 8) == 0x0) || ((tmp >> 8) == 0xFFFFFF)) {
			*disp = tmp;
			return true;
		}
	}

	parse_ok = false;
	return false;
}


/* PC-relative address */
static bool parse_pcrel16(int32_t *disp, uint32_t pc, int width, enum ifp ifp)
{
	(void) width, (void) ifp;
	struct big_int	x;

	if (parse_bigint(&x, 4)) {
		uint32_t	tmp;

		tmp = x.val[0] - pc;
		if (((tmp >> 16) == 0x0) || ((tmp >> 16) == 0xFFFF)) {
			*disp = tmp;
			return true;
		}
	}

	parse_ok = false;
	return false;
}


/* PC-relative address */
static bool parse_pcrel32(int32_t *disp, uint32_t pc, int width, enum ifp ifp)
{
	(void) width, (void) ifp;
	struct big_int	x;

	if (parse_bigint(&x, 4)) {
		*disp = x.val[0] - pc;
		return true;
	}
	parse_ok = false;
	return false;
}


/* 'bb' operand (byte-sized relative branch) */
static bool parse_branch8(uint8_t *disp, uint32_t pc)
{
	struct big_int	x;

	if (parse_bigint(&x, 4)) {
		int32_t	tmp = x.val[0] - pc;
		if ((int32_t)(int8_t)(tmp & 0xFF) != tmp) {
			parse_ok = false;
			return false;
		}
		*disp = tmp;
		return true;
	}
	parse_ok = false;
	return false;
}


/* 'bw' operand (word-sized relative branch) */
static bool parse_branch16(uint16_t *disp, uint32_t pc)
{
	struct big_int	x;

	if (parse_bigint(&x, 4)) {
		int32_t	tmp = x.val[0] - pc;
		if ((int32_t)(int16_t)(tmp & 0xFFFF) != tmp) {
			parse_ok = false;
			return false;
		}
		*disp = tmp;
		return true;
	}
	parse_ok = false;
	return false;
}


/* absolute address */
static bool parse_addr(int32_t *addr, uint32_t pc, int width, enum ifp ifp)
{
	(void) pc, (void) width, (void) ifp;

	struct big_int	x;

	if (parse_bigint(&x, 4)) {
		*addr = x.val[0];
		return true;
	}

	return false;
}


/* displacement relative to a register (that isn't PC) */
static bool parse_disp8(int32_t *disp, uint32_t pc, int width, enum ifp ifp)
{
	(void) pc, (void) width, (void) ifp;
	struct big_int	x;

	if (!parse_bigint(&x, 1))
		return false;

	*disp = x.val[0];
	return true;
}

/* displacement relative to a register (that isn't PC) */
static bool parse_disp16(int32_t *disp, uint32_t pc, int width, enum ifp ifp)
{
	(void) pc, (void) width, (void) ifp;
	struct big_int	x;

	if (!parse_bigint(&x, 2))
		return false;

	*disp = x.val[0];
	return true;
}

/* displacement relative to a register (that isn't PC) */
static bool parse_disp32(int32_t *disp, uint32_t pc, int width, enum ifp ifp)
{
	(void) pc, (void) width, (void) ifp;
	struct big_int	x;

	if (!parse_bigint(&x, 4))
		return false;

	*disp = x.val[0];
	return true;
}

#endif

