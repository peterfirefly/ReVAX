/* Copyright 2018  Peter Lund <firefly@vax64.dk>\

   Licensed under GPL v2.

 */


/* operand handling support code

   called by autogenerated code for asm/dis/sim/val.
 */

/* packing/unpacking

   using '|' instead of '+' to combine the parts work better with the idiom
   recognizers in gcc/clang.

   casting the uint8_t bytes to unsigned/uint32_t before the shifts avoids
   the undefined behaviour with '<< 24' and ints (int promotion!) and also
   makes the idiom recognizers work better.
 */
#define BYTE(x, n)	(((x) >> (n*8)) & 0xFF)


/* double cast trick works in both gcc and clang.
   B   is recognized as an idiom by at least gcc 4.7+ and clang 3.9+
   W/L are recognized as idioms by at least clang 5.0+
 */
#define B(x)		((int32_t)(int8_t) ((&(x))[0]))

#define W(x)		((int32_t)(int16_t)(((unsigned) ((&(x))[0])     ) | \
				            ((unsigned) ((&(x))[1]) <<  8)))

#define L(x)		((int32_t)         ((uint32_t) ((&(x))[0])      ) | \
					   ((uint32_t) ((&(x))[1]) <<  8) | \
					   ((uint32_t) ((&(x))[2]) << 16) | \
					   ((uint32_t) ((&(x))[3]) << 24))


/* check functions */

/* check that Rn access won't spill over to r15 */
static bool check_reg(struct fields fields, int width)
{
//	fprintf(stderr, "[Rn=%d width=%d]\n", fields.Rn, width);
	return fields.Rn + (width + 3) / 4 + 1 < 15;
}


/* check Rn != Rx, check that Rx != r15 */
static bool check_idx(struct fields fields, int width)
{
	(void) width;

	return fields.Rn != fields.Rx;
}


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
		    			         ((fields->lit6 & 7) << 28);
		    fields->imm.val[1] = 0;
		    fields->imm.val[2] = 0;
		    fields->imm.val[3] = 0;
		    break;
	default:
		assert(0);
	}
}



