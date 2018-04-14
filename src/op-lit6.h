/* Copyright 2018  Peter Lund <firefly@vax64.dk>

   Licensed under GPL v2.

   ---

   operand handling support code -- shared by dis and sim

 */

#ifndef OP_LIT6__H
#define OP_LIT6__H


/* lit6 expander, used by dis/sim */
static void expand_lit6(struct fields *fields, int width, enum ifp ifp)
{
	(void) width;

	switch (ifp) {
	case IFP_INT: fields->imm.val[0] = fields->lit6;
		      fields->imm.val[1] = 0;
		      break;

	case IFP_F: assert(width== 4);
		    assert((fields->lit6 &~ 0x3F) == 0);
		    fields->imm.val[0] = 0x4000 | (fields->lit6 << 4);
		    break;
	case IFP_D: assert(width== 8);
		    assert((fields->lit6 &~ 0x3F) == 0);
		    fields->imm.val[0] = 0x4000 | (fields->lit6 << 4);
		    fields->imm.val[1] = 0;
		    break;
	case IFP_G: assert(width== 8);
		    assert((fields->lit6 &~ 0x3F) == 0);
		    fields->imm.val[0] = 0x4000 | (fields->lit6 << 1);
		    fields->imm.val[1] = 0;
		    break;
	case IFP_H: assert(width==16);
		    assert((fields->lit6 &~ 0x3F) == 0);
		    fields->imm.val[0] = 0x4000 | (fields->lit6      >>  3) |
		    			         ((uint32_t) fields->lit6 << 29);
		    fields->imm.val[1] = 0;
		    fields->imm.val[2] = 0;
		    fields->imm.val[3] = 0;
		    break;
	default:
		UNREACHABLE();
	}
}

#endif

