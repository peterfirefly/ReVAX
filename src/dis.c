/* Copyright 2018  Peter Lund <firefly@vax64.dk>\

   Licensed under GPL v2.

   ---

     VAX disassembler -- inspired by Sourcer by V Communications for DOS.
     (Sourcer was a commercial product 1988-2000)

     https://en.wikibooks.org/wiki/X86_Disassembly/Disassemblers_and_Decompilers

     https://en.wikipedia.org/wiki/Executable_and_Linkable_Format
     https://en.wikipedia.org/wiki/A.out

     https://corexor.wordpress.com/2015/12/09/sourcer-and-windows-source/


     Inputs a raw binary or an a.out binary (+ possibly a ctrl file) and outputs
     .asm, .lst, .info, plus a subdirectory with HTML.



     Internals:

     Reads the whole input file in one go into blob[] -- memory is cheap, these
     days.
     There are two sets of code that parse opcodes and operands, which seems
     like a bad idea.  But one of them spends most of its code formatting
     output (it is the disassembler) and the other just counts bytes so it is
     a lot shorter and simpler.  Furthermore, they don't actually have to be
     kept entirely in sync.  The disassembler code has to be correct but the
     other code is "just" used to analyze the code for branches, jumps, and
     calls.  Most errors in that code would be of little consequence and can
     easily be overridden by a .ctrl file.

     A flexible output for .asm/.lst/.html is easily made by having the
     disassembler code generate a handful of strings (for address, raw bytes,
     instruction, and comments).  These strings can easily be written to the
     three output files with the raw bytes string being reflown into a column
     if it is too long.



     Useful HTML/CSS links:

     http://diveintohtml5.info/
     https://developer.mozilla.org/en-US/docs/Web/HTML
     https://developer.mozilla.org/en-US/docs/Web/CSS
     https://www.w3.org/TR/html5/
     https://www.w3.org/TR/CSS/#css
     https://www.w3.org/Style/CSS/Overview.en.html

 */

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <sys/stat.h>

#include <elf.h>

#include "shared.h"

#include "vax-instr.h"


/***/


uint32_t	blob_start;
uint8_t		blob[1024*1024];
size_t		blob_size;


/***/

/* fetch block


  bool fetch(uint32_t addr, unsigned bytecnt, uint8_t dst[bytecnt])

  alternative:

    fetch n bytes from addr into dstbuf
    if that is impossible, fill remaining bytes with 0x00

    return no of valid bytes?
    return last valid addr?
 */

/* this routine handles all normal fetches from the code blob -- it makes
   sense to use a single function that returns a 32-bit number because the VAX
   is such a 32-bit machine through and through.

   64-bit fetches *are* used in a few places, because the VAX also supports a
   64-bit integer (quad) and the 64-bit floating-point types (d/g).

   The 64-bit types are only visible to the disassembler as immediate operands
   to ASHQ, MOVQ, EDIV and (ADD|SUB|MUL|DIV)[DG][23], CVT[DG][BWL], CVTRDL,
   CVTDF, CVTGF, CVTRGL, (ACB|MOV|CMP|MNEG|TST|EMOD|POLY)[DG].

   That's a lot more 64-bit floating-point operations than I expected!

   The 128-bit types defined in the VAX architecture (octo for integers and h
   for floating-point) are not supported by this disassembler.

   Used for both integers (byte/word/long) and floating-point (f).
*/
int32_t fetch(unsigned idx, int width)
{
	/* FIXME range check idx */
	switch (width) {
	case 1:		return (int32_t)(int8_t)  (blob[idx  ]);
	case 2:		return (int32_t)(int16_t) (blob[idx  ]      + (blob[idx+1]<< 8));
	case 4:		return (int32_t)( blob[idx  ]      + (blob[idx+1]<< 8) +
				         (blob[idx+2]<<16) + ((uint32_t)(blob[idx+3])<<24));
	default:
		fprintf(stderr, "fetch width: %d\n", width);
		assert(0);
	}
}


/* used for both integers (quad) and floating-point (d, g) */
int64_t fetchq(unsigned idx)
{
	/* FIXME range check idx */

	return ((int64_t) blob[idx  ]    ) + ((int64_t) blob[idx+1]<< 8) +
	       ((int64_t) blob[idx+2]<<16) + ((int64_t) blob[idx+3]<<24) +
	       ((int64_t) blob[idx+4]<<32) + ((int64_t) blob[idx+5]<<40) +
	       ((int64_t) blob[idx+6]<<48) + ((int64_t) blob[idx+7]<<48);
}


/* FIXME conversion routines for f/d/g floating-point to ASCII as decimal
   numbers.


   The "cheat" would be to use IEEE but they don't quite have the same range or
   set of INF/NAN.
 */

enum { FMT_VAX, FMT_SANE } format = FMT_SANE;


/* Adding a 32-bit displacement to a 32-bit address may cause a wrap-around.

   This is perfectly legal C behaviour but such wrap-arounds are in general
   a symptom of something going wrong and may cause security holes.
   Clang can check for such overflows (which is generally a good idea) but in
   this particular case it is just fine.  The addition is pulled out into a
   function of its own so the warning can be selectively disabled.
 */
#if defined(__clang__)
/* gcc doesn't have the no_sanitize attribute (yet?) */
static uint32_t addr_disp(uint32_t addr, uint32_t disp)
		__attribute__((no_sanitize("unsigned-integer-overflow")));
#endif
static uint32_t addr_disp(uint32_t addr, uint32_t disp)
{
	return addr + disp;
}


/* FIXME: two-char operand code, so we can handle bitfields and floats correctly */
/* returns operand size in bytes */
unsigned dis_op(char *buf, unsigned idx, unsigned end_idx, char access, char type, int width)
{
	/* FIXME always use fetch()! */

	/* FIXME better solution to "reserved operand" handling */
	/* FIXME register name translation/comments */

	unsigned	Rn = blob[idx] & 0x0F;

	(void) end_idx;

	if (access == 'b') {
		/* 'b' = branch */
		switch (type) {
		case 'b':
			{
			uint32_t	target = addr_disp(blob_start + idx + 1, fetch(idx, 1));

			sprintf(buf, "0x%04X_%04X", SPLIT(target));
			return 1;
			}
		case 'w':
			{
			uint32_t	target = addr_disp(blob_start + idx + 2, fetch(idx, 2));

			sprintf(buf, "0x%04X_%04X", SPLIT(target));
			return 2;
			}
		default:
			fprintf(stderr, "optype: %c%c\n", access, type);
			assert(0);
		}
	}


	switch (blob[idx] & 0xF0) {
	case 0x00:
	case 0x10:
	case 0x20:
	case 0x30: /* 6-bit literal */

		/* FIXME: floating-point 6-bit literals are different! */
		switch (format) {
		case FMT_VAX:
			sprintf(buf, "S^#%d", blob[idx] & 0x3F);
			break;
		case FMT_SANE:
			sprintf(buf, "%d",    blob[idx] & 0x3F);
			break;
		}
		return 1;

	case 0x40:
		/* indexed */
		{
		unsigned	Rx = Rn;
		Rn = blob[idx+1] & 0x0F;

		if (Rn == Rx) {
			sprintf(buf, "resop [%02X %02X]", blob[idx], blob[idx+1]);
			return 2;
		}

		switch (blob[idx+1] & 0xF0) {
		case 0x60:
			/* register def idx -- (Rn)[Rx] | [Rn + Rx*sze] */
			switch (format) {
			case FMT_VAX:
				sprintf(buf, "(r%d)[r%d]", Rn, Rx);
				break;

			case FMT_SANE:
				if (width == 1)
					sprintf(buf, "[r%d + r%d]", Rn, Rx);
				else
					sprintf(buf, "[r%d + r%d*%d]", Rn, Rx, width);
				break;
			}
			return 2;

		case 0x70:
			/* autodec idx     -- -(Rn)[Rx]  | [--Rn + Rx*sze]   */
			switch (format) {
			case FMT_VAX:
				sprintf(buf, "-(r%d)[r%d]", Rn, Rx);
				break;

			case FMT_SANE:
				if (width == 1)
					sprintf(buf, "[--r%d + r%d]", Rn, Rx);
				else
					sprintf(buf, "[--r%d + r%d*%d]", Rn, Rx, width);
				break;
			}
			return 2;

		case 0x80:
			/* FIXME 8F = undefined */
			if (Rn == 0xF) {
				sprintf(buf, "resop [%02X %02X]", blob[idx], blob[idx+1]);
				return 2;
			}

			/* autoinc idx     -- (Rn)+[Rx] | [Rn++ + Rx*sze] */
			switch (format) {
			case FMT_VAX:
				sprintf(buf, "(r%d)+[r%d]", Rn, Rx);
				break;

			case FMT_SANE:
				if (width == 1)
					sprintf(buf, "[r%d++ + r%d]", Rn, Rx);
				else
					sprintf(buf, "[r%d++ + r%d*%d]", Rn, Rx, width);
				break;
			}
			return 2;

		case 0x90:
			/* autoinc def idx -- @(Rn)+[Rx] | [[Rn++] + Rx*sze]   */

			if (Rn == 0xF) {
				/* absolute idx -- @#addr[Rx] | [addr + Rx*sze] */
				int32_t		addr = fetch(idx+2, 4);

				switch (format) {
				case FMT_VAX:
					sprintf(buf, "@#%04X_%04X[r%d]", SPLIT(addr), Rx);
					break;

				case FMT_SANE:
					if (width == 1)
						sprintf(buf, "[%04X_%04X + r%d]",
							SPLIT(addr), Rx);
					else
						sprintf(buf, "[%04X_%04X + r%d*%d]",
							SPLIT(addr), Rx, width);
					break;
				}
				return 2+4;
			} else {
				switch (format) {
				case FMT_VAX:
					sprintf(buf, "@(r%d)+[r%d]", Rn, Rx);
					break;

				case FMT_SANE:
					if (width == 1)
						sprintf(buf, "[[r%d++] + r%d]", Rn, Rx);
					else
						sprintf(buf, "[[r%d++] + r%d*%d]", Rn, Rx, width);
					break;
				}
				return 2;
			}

		case 0xA0:
			/* byte disp idx     -- B^D(Rn)[Rx]  | [Rn + disp + Rx*sze] */
			{
			int32_t		disp = fetch(idx+2, 1);

			switch (format) {
			case FMT_VAX:
				sprintf(buf, "B^%d(r%d)[r%d]", disp, Rn, Rx);
				break;

			case FMT_SANE:
				if (width == 1)
					sprintf(buf, "[r%d + %d + r%d]", Rn, disp, Rx);
				else
					sprintf(buf, "[r%d + %d + r%d*%d]", Rn, disp, Rx, width);
				break;
			}
			return 2+1;
			}
		case 0xB0:
			/* byte disp def idx -- @B^D(Rn)[Rx] | [[Rn + disp] + Rx<<idx] */
			{
			int32_t		disp = fetch(idx+2, 1);

			switch (format) {
			case FMT_VAX:
				sprintf(buf, "@B^%d(r%d)[r%d]", disp, Rn, Rx);
				break;

			case FMT_SANE:
				if (width == 1)
					sprintf(buf, "[[r%d + %d] + r%d]", Rn, disp, Rx);
				else
					sprintf(buf, "[[r%d + %d] + r%d*%d]", Rn, disp, Rx, width);
				break;
			}
			return 2+1;
			}
		case 0xC0:
			/* word disp idx     -- W^D(Rn)[Rx]  | [Rn + disp + Rx<<idx] */
			{
			int32_t		disp = fetch(idx+2, 2);

			switch (format) {
			case FMT_VAX:
				sprintf(buf, "W^%d(r%d)[r%d]", disp, Rn, Rx);
				break;

			case FMT_SANE:
				if (width == 1)
					sprintf(buf, "[r%d + %d + r%d]", Rn, disp, Rx);
				else
					sprintf(buf, "[r%d + %d + r%d*%d]", Rn, disp, Rx, width);
				break;
			}
			return 2+2;
			}
		case 0xD0:
			/* word disp def idx -- @W^D(Rn)[Rx] | [[Rn + disp] + Rx<<idx] */
			{
			int32_t		disp = fetch(idx+2, 2);

			switch (format) {
			case FMT_VAX:
				sprintf(buf, "@W^%d(r%d)[r%d]", disp, Rn, Rx);
				break;

			case FMT_SANE:
				if (width == 1)
					sprintf(buf, "[[r%d + %d] + r%d]", Rn, disp, Rx);
				else
					sprintf(buf, "[[r%d + %d] + r%d*%d]", Rn, disp, Rx, width);
				break;
			}
			return 2+2;
			}
		case 0xE0:
			/* lword disp idx    -- L^D(Rn)[Rx]  | [Rn + disp + Rx<<idx] */
			{
			int32_t		disp = fetch(idx+2, 4);

			switch (format) {
			case FMT_VAX:
				sprintf(buf, "L^%d(r%d)[r%d]", disp, Rn, Rx);
				break;

			case FMT_SANE:
				if (width == 1)
					sprintf(buf, "[r%d + %d + r%d]", Rn, disp, Rx);
				else
					sprintf(buf, "[r%d + %d + r%d*%d]", Rn, disp, Rx, width);
				break;
			}
			return 2+4;
			}
		case 0xF0:
			/* lword disp def idx --@L^D(Rn)[Rx] | [[Rn + disp] + Rx<<idx] */
			{
			int32_t		disp = fetch(idx+2, 4);

			switch (format) {
			case FMT_VAX:
				sprintf(buf, "@L^%d(r%d)[r%d]", disp, Rn, Rx);
				break;

			case FMT_SANE:
				if (width == 1)
					sprintf(buf, "[[r%d + %d] + r%d]", Rn, disp, Rx);
				else
					sprintf(buf, "[[r%d + %d] + r%d*%d]", Rn, disp, Rx, width);
				break;
			}
			return 2+4;
			}
		default:
			sprintf(buf, "resop [%02X %02X]", blob[idx], blob[idx+1]);
			return 2;
		}
		break;	/* assuage the compiler */
		}

	case 0x50:
		/* register --           Rn   |  Rn      */
		if (Rn == 0xF) {
			sprintf(buf, "resop [%02X]", blob[idx]);
			return 1;
		}

		sprintf(buf, "r%d", Rn);
		return 1;

	case 0x60:
		/* register deferred -- (Rn)  |  [Rn]    */
		if (Rn == 0xF) {
			sprintf(buf, "resop [%02X]", blob[idx]);
			return 1;
		}

		switch (format) {
		case FMT_VAX:
			sprintf(buf, "(r%d)", Rn);
			break;

		case FMT_SANE:
			sprintf(buf, "[r%d]", Rn);
			break;
		}
		return 1;

	case 0x70:
		/* autodecrement --    -(Rn)  |  [--Rn]  */
		if (Rn == 0xF) {
			sprintf(buf, "resop [%02X]", blob[idx]);
			return 1;
		}

		switch (format) {
		case FMT_VAX:
			sprintf(buf, "-(r%d)", Rn);
			break;
		case FMT_SANE:
			sprintf(buf, "[--r%d]", Rn);
			break;
		}
		return 1;

	case 0x80:
		/* autoincrement --    (Rn)+  |  [Rn++]  */
		if (Rn == 0xF) {
			/* This is the only place where 64-bit and 128-bit
			   operands are visible to the assembler, namely when
			   they are used as immediates.  It is therefore the
			   only place where fetch() is called with width as
			   a parameter.

			   32-bit and 64-bit operands may be either integers
			   (long, quad) or floating-point numbers (f, d, g).

			   We need the type letter for the operand in order to
			   parse it correctly.

			   FIXME: 128-bit integers (octo) and floating-point
			   numbers (h) are not supported.
			 */


			/* FIXME this case should really be a case on the
			   type letter, not the width.
			 */

			/* immediate -- I^#xx |  #xxx   */
			if (width == 8) {
				sprintf(buf, "Q^#xxxx_xxxx");
				return 1+8;
			}

			if (width == 0) {
fprintf(stderr, "optype: %c%c\n", access, type);

			}

			int32_t		imm = fetch(idx+1, width);

			switch (format) {
			case FMT_VAX:
				switch (width) {
				case 1:	/* byte */
					sprintf(buf, "B^#%d", imm);
					break;
				case 2: /* word */
					sprintf(buf, "W^#%d", imm);
					break;
				case 4: /* long word */
					sprintf(buf, "L^#%d", imm);
					break;
#if 0
				case 8: /* quad word or d/g floating-point */
					sprintf(buf, "#%d", fetchq(idx+1));
					break;
#endif
				default:
					sprintf(buf, "?? [width=%d]", width);
				}
				break;

			case FMT_SANE:
				sprintf(buf, "#%d", imm);
				break;
			}
			return 1 + width;
		} else {
			switch (format) {
			case FMT_VAX:
				sprintf(buf, "(r%d)+", Rn);
				break;

			case FMT_SANE:
				sprintf(buf, "[r%d++]", Rn);
				break;
			}
			return 1;
		}
		break;	/* assuage the compiler */

	case 0x90:
		/* autoincrement deferred -- @(Rn)+   | [[Rn++]]       */
		if (Rn == 0xF) {
			/* absolute --       @#addr   | [#addr]        */
			int32_t		addr = fetch(idx+1, 4);

			switch (format) {
			case FMT_VAX:
				sprintf(buf, "@#%04X_%04X", SPLIT(addr));
				break;

			case FMT_SANE:
				sprintf(buf, "[#0x%04X_%04X]", SPLIT(addr));
				break;
			}
			return 1+4;
		} else {
			switch (format) {
			case FMT_VAX:
				sprintf(buf, "@(r%d)+", Rn);
				break;

			case FMT_SANE:
				sprintf(buf, "[[r%d++]]", Rn);
				break;
			}
			return 1;
		}
		break;	/* assuage the compiler */

	case 0xA0:
		/* byte displacement   --  B^disp(Rn) | [Rn + bdisp]   */
		{
		int32_t		disp = fetch(idx+1, 1);

		switch (format) {
		case FMT_VAX:
			sprintf(buf, "B^%d(r%d)", disp, Rn);
			break;

		case FMT_SANE:
			sprintf(buf, "[r%d + %d]", Rn, disp);
			break;
		}
		return 1+1;
		}
	case 0xB0:
		/* byte disp deferred  -- @B^disp(Rn) | [[Rn + bdisp]]   */
		{
		int32_t		disp = fetch(idx+1, 1);

		switch (format) {
		case FMT_VAX:
			sprintf(buf, "@B^%d(r%d)", disp, Rn);
			break;

		case FMT_SANE:
			sprintf(buf, "[[r%d + %d]]", Rn, disp);
			break;
		}
		return 1+1;
		}
	case 0xC0:
		/* word disp           -- W^disp(Rn)  | [Rn + wdisp]   */
		{
		int32_t		disp = fetch(idx+1, 2);

		switch (format) {
		case FMT_VAX:
			sprintf(buf, "W^%d(r%d)", disp, Rn);
			break;

		case FMT_SANE:
			sprintf(buf, "[r%d + %d]", Rn, disp);
			break;
		}
		return 1+2;
		}
	case 0xD0:
		/* word disp deferred  -- @W^disp(Rn) | [[Rn + wdisp]] */
		{
		int32_t		disp = fetch(idx+1, 2);

		switch (format) {
		case FMT_VAX:
			sprintf(buf, "@W^%d(r%d)", disp, Rn);
			break;

		case FMT_SANE:
			sprintf(buf, "[[r%d + %d]]", Rn, disp);
			break;
		}
		return 1+2;
		}
	case 0xE0:
		/* lword disp          -- L^disp(Rn)  | [Rn + ldisp]   */
		{
		int32_t		disp = fetch(idx+1, 4);

		switch (format) {
		case FMT_VAX:
			sprintf(buf, "L^%d(r%d)", disp, Rn);
			break;

		case FMT_SANE:
			sprintf(buf, "[r%d + %d]", Rn, disp);
			break;
		}
		return 1+4;
		}
	case 0xF0:
		/* lword disp deferred -- @L^disp(Rn) | [[Rn + ldisp]] */
		{
		int32_t		disp = fetch(idx+1, 4);

		switch (format) {
		case FMT_VAX:
			sprintf(buf, "@L^%d(r%d)", disp, Rn);
			break;

		case FMT_SANE:
			sprintf(buf, "[[r%d + %d]]", Rn, disp);
			break;
		}
		return 1+4;
		}
	default:
		assert(0);
	}
}



struct outfiles {
	FILE	*asm_;	/* asm is a reserved keyword in gcc :( */
	FILE	*lst;
	FILE	*htm;
};


struct buf {
	char	addr[1024];
	char	bytes[1024];
	char	instr[1024];
	char	comment[1024];
};


/* output addr + instruction bytes + disassembly + comment(s), nicely formatted.
 */
static void output(struct outfiles *f, struct buf *buf)
{
	/* output all labels that point to this address -- or even better:
	   all labels that point to *somewhere* within this instruction.

	   empty line at the end of each bb.
	 */



	/* FIXME pseudo instructions/directives like .byte/.word/.org? */

	/* make the output nicer by removing trailing spaces from buf->bytes
	   and buf->instr.
	 */
	trim_trailing_space(buf->bytes);
	trim_trailing_space(buf->instr);


	/* .asm file */
	fprintf(f->asm_, "\t%s", buf->instr);
	if (buf->comment[0])
		fprintf(f->asm_, "%*s ; %s", (int) (30-strlen(buf->instr)), "", buf->comment);
	fprintf(f->asm_, "\n");


	/* .lst file */
	struct reflow	*reflow = reflow_string(buf->bytes, 20);
	fprintf(f->lst, "%s  %s %s", buf->addr, reflow_line(reflow, buf->bytes, 0).str, buf->instr);
	if (buf->comment[0])
		fprintf(f->lst, "%*s ; %s", (int) (30-strlen(buf->instr)), "", buf->comment);
	fprintf(f->lst, "\n");
	for (int i=1; i < reflow_line_cnt(reflow); i++)
		fprintf(f->lst, "%*s  %s\n", (int) (strlen("XXXX_XXXX")), "",
			      reflow_line(reflow, buf->bytes, i).str);
	free(reflow);

	/* HTML */
	fprintf(f->htm, "<TR>\n");
	fprintf(f->htm, "  <TD>%s</TD>\n", buf->addr);
	fprintf(f->htm, "  <TD>%s</TD>\n", buf->bytes);
	fprintf(f->htm, "  <TD>%s</TD>\n", html_escape(buf->instr, true).str);
	if (buf->comment[0])
		fprintf(f->htm, "  <TD>; %s</TD>\n", html_escape(buf->comment, false).str);
	else
		fprintf(f->htm, "  <TD></TD>\n");
	fprintf(f->htm, "</TR>\n");
}


void dis(struct outfiles *f, unsigned idx, int len)
{
	unsigned	end_idx = idx + len;

	if (end_idx > blob_size)
		end_idx = blob_size;

	while (idx < end_idx) {
		int		op, oplen;
		struct buf	buf;

		/* clear output buffers */
		buf.addr[0] = '\0';
		buf.bytes[0] = '\0';
		buf.instr[0] = '\0';
		buf.comment[0] = '\0';

		/* addr */
		sprintf(buf.addr, "%04X_%04X", SPLIT(blob_start + idx));

		/* FIXME always use fetch()! */
		/* FIXME - output opcode */

		/* decode opcode */
		if (blob[idx] == 0xFD) {
			if (idx+1 >= end_idx) {
				sprintf(buf.instr, " FD <incomplete>\n");
				output(f, &buf);
				return;
			}
			op = blob[idx+1] + 0x100;
			oplen = 2;
			sprintf(buf.bytes, "%02X%02X ", blob[idx], blob[idx+1]);
		} else {
			op = blob[idx];
			oplen = 1;
			sprintf(buf.bytes, "%02X ", blob[idx]);
		}

		if (strlen(mne[op]) == 0) {
			/* FIXME db XXh or XXXXh */
			sprintf(buf.instr, " *** reserved instruction");

			/* FIXME handle FFFF, FFFE */

			idx += oplen;

			output(f, &buf);
			continue;
		}

		/* ok, we have an opcode and it's a valid instruction */
		sprintf(buf.instr, "%-6s  ", mne[op]);
		idx += oplen;

		if (strcmp(mne[op], "REI") == 0) {
			sprintf(&buf.comment[strlen(buf.comment)], "return from interrupt");
		}


		/* does it have ops? */
		if (strlen(ops[op]) != 0) {
			for (unsigned i=0; i < op_cnt[op]; i++) {
				if (i > 0)
					sprintf(&buf.instr[strlen(buf.instr)], ", ");

				/* disassemble an operand given its datatype */
				unsigned len;
				len = dis_op(&buf.instr[strlen(buf.instr)],
					      idx, end_idx, ops[op][i*3], ops[op][i*3+1],
					      op_width[op][i]);

				/* output operand bytes */
				for (unsigned j=0; j<len; j++)
					sprintf(buf.bytes + strlen(buf.bytes),
						"%02X", blob[idx+j]);
				sprintf(buf.bytes + strlen(buf.bytes), " ");

				/* move "PC" to just past this operand */
				idx += len;
			}
		}

		/* push the output out to the world as .asm, .lst, and .html */
		output(f, &buf);

		/* FIXME special-case CASE instructions */

		/* FIXME empty line after end of basic blocks */
	}
}


/*   . = unclassified
     @ = code label
     X = code
    (d = data   some day, maybe)


     Transitions:
      . => @
      . => X
      X => @

     This means the map can be used as a quick check to see if a label is known
     already + as a check to see if an EBB is known already (so we can avoid
     scanning through an instruction more than once).

     Problem with this:  jumping into the middle of instructions.
     Can be fixed by encoding instruction starts, too.  Maybe both x and X?
     Or X and '-'?  That would allow detection of overlapping instructions, which
     would probably be a feature.

     Should I use bit flags instead?
     What if bits always get set and never reset?

     Should distinguish between call targets and other targets (so we can
     handle the register mask procedure header correctly).

 */
char	map[ARRAY_SIZE(blob)];


/* instruction classification:
     BB_END	BRB, BRW, JMP, RET, RSB, REI, HALT  -- BPT?  XFC?
     CTRL
     CASE
 */
#define	CTRL	1
#define	BB_END	2
#define CASE	4
uint8_t	class[512];


/* fill in class[] */
void prepare_classification()
{
	memset(class, 0x0, sizeof(class));

	class[0x9D] = CTRL;		/* ACBB		*/
	class[0xF1] = CTRL;		/* ACBL		*/
	class[0x3D] = CTRL;		/* ACBW		*/
	class[0xF3] = CTRL;		/* AOBLEQ	*/
	class[0xF2] = CTRL;		/* AOBLSS	*/

	class[0x1E] = CTRL;		/* BCC/BGEQU	*/
	class[0x1F] = CTRL;		/* BCS/BLSSU	*/
	class[0x13] = CTRL;		/* BEQL/BEQLU	*/
	class[0x18] = CTRL;		/* BGEQ		*/

	class[0x14] = CTRL;		/* BGTR		*/
	class[0x1A] = CTRL;		/* BGTRU	*/
	class[0x15] = CTRL;		/* BLEQ		*/
	class[0x1B] = CTRL;		/* BLEQU	*/

	class[0x19] = CTRL;		/* BLSS		*/
	class[0x12] = CTRL;		/* BNEQ/BNEQU	*/
	class[0x1C] = CTRL;		/* BVC		*/
	class[0x1D] = CTRL;		/* BVS		*/

	class[0xE1] = CTRL;		/* BBC		*/
	class[0xE0] = CTRL;		/* BBS		*/
	class[0xE5] = CTRL;		/* BBCC		*/
	class[0xE3] = CTRL;		/* BBCS		*/
	class[0xE4] = CTRL;		/* BBSC		*/
	class[0xE2] = CTRL;		/* BBSS		*/
	class[0xE7] = CTRL;		/* BBCCI	*/
	class[0xE6] = CTRL;		/* BBSSI	*/
	class[0xE9] = CTRL;		/* BLBC		*/
	class[0xE8] = CTRL;		/* BLBS		*/

	class[0x11] = CTRL|BB_END;	/* BRB		*/
	class[0x31] = CTRL|BB_END;	/* BRW		*/

	class[0x10] = CTRL;		/* BSBB		*/
	class[0x30] = CTRL;		/* BSBW		*/

	class[0x8F] = CTRL|CASE;	/* CASEB	*/
	class[0xCF] = CTRL|CASE;	/* CASEL	*/
	class[0xAF] = CTRL|CASE;	/* CASEW	*/

	class[0x17] = CTRL|BB_END;	/* JMP		*/

	class[0x16] = CTRL;		/* JSB		*/
	class[0x05] = CTRL|BB_END;	/* RSB		*/

	class[0xF4] = CTRL;		/* SOBGEQ	*/
	class[0xF5] = CTRL;		/* SOBGTR	*/

	class[0xFA] = CTRL;		/* CALLG	*/
	class[0xFB] = CTRL;		/* CALLS	*/
	class[0x04] = CTRL|BB_END;	/* RET		*/

//	class[0x03] = CTRL;		/* BPT		*/
	class[0x00] = CTRL|BB_END;	/* HALT		*/
	class[0x02] = CTRL|BB_END;	/* REI		*/
//	class[0xFC] = CTRL|BB_END;	/* XFC		*/

}


/* step through an operand -- tell us how long it was in bytes

   FIXME should probably have a flag to say when to add labels (for JMP, CALL)

    -1   reserved operand
 */
static int op_len(unsigned idx, int width)
{
	switch (blob[idx] & 0xF0) {
	case 0x00:
	case 0x10:
	case 0x20:
	case 0x30:
		return 1;
	case 0x40:
		{
		switch (blob[idx+1] & 0xF0) {
		case 0x00:	// 6-bit imm -- not allowed
		case 0x10:	// 6-bit imm -- not allowed
		case 0x20:	// 6-bit imm -- not allowed
		case 0x30:	// 6-bit imm -- not allowed
		case 0x40:	// indexed + indexed -- not allowed
		case 0x50:	// indexed + register -- not allowed
			return -1;
		case 0x60:
			return 2; // (Rn)[Rx]
		case 0x70:
			return 2; // -(Rn)[Rx]
		case 0x80:
			if ((blob[idx+1] & 0x0F) == 0x0F)
				return -1; // indexed + imm -- not allowed
			else
				return 2;  // (Rn)+[Rx]
		case 0x90:
			if ((blob[idx+1] & 0x0F) == 0x0F)
				return 2+4; // @#addr[Rx]
			else
				return 2;   // @(Rn)+[Rx]
		case 0xA0:	// B^(Rn)[Rx]
		case 0xB0:	// @B^(Rn)[Rx]
			return 2+1;
		case 0xC0:	// W^(Rn)[Rx]
		case 0xD0:	// @W^(Rn)[Rx]
			return 2+2;
		case 0xE0:	// L^(Rn)[Rx]
		case 0xF0:	// @L^(Rn)[Rx]
			return 2+4;
		default:
			assert(0);
		}
		}
	case 0x50:
		return 1; // Rn
	case 0x60:
		return 1; // (Rn)
	case 0x70:
		return 1; // @(Rn)
	case 0x80:
		if ((blob[idx] & 0x0F) == 0xF)
			return 1+width;	// imm
		else
			return 1;		// (Rn)+
	case 0x90:
		if ((blob[idx] & 0x0F) == 0xF)
			return 1+4;		// abs
		else
			return 1;		// @(Rn)+
	case 0xA0:
	case 0xB0:
		return 1+1;
	case 0xC0:
	case 0xD0:
		return 1+2;
	case 0xE0:
	case 0xF0:
		return 1+4;
	default:
		assert(0);
	}
}


void map_blob()
{
	/* reset map, label list, stats, label count */

	/* put "primers" into the label list -- just use @0 for now */

	/* for each label:
	       run through the basic block, i.e. add labels for all the control
	       instructions we encounter, continue until we reach an unconditional
	       control transfer instruction (BRB, BRW, JMP, RET, REI, HALT --
	       not sure what to do about BPT and XFC).

               all the instruction bytes encountered should be marked as 'code'.

	 */

	/* keep going through the labels until no new labels are added

	   nope, slow.  We should have a list of labels (of course) and a work
	   list.  Keep going until the work list is empty.  New labels go into
	   both the label list and the work list.
	 */


	/* what information might we want per label?  target + list of "come froms"
	   + possibly a name.

	   what information might we want per work list item?  just the target
	   address.

	   use 'X' in the map while working, go through the labels afterwards
	   and convert the label targets to '@'.


	   data structures?   simple array w/ doubling (amortized O(1)) for
	   work list?   simple array w/ doubling for labels as well?  With a
	   "thread" providing a tree?

	   I can get by with a simple list of VAX addresses in both cases.
	   If I don't search in the labels list, I don't have to keep it sorted
	   or maintain a hash table or a tree.  I can avoid searching in the
	   labels if I mark all labels immediately in the map as I take their
	   addresses off the work list.

           The instruction tracer should check every label against the map as
           they are generated (as that is very cheap to do).  This code should
           be in the function that adds new labels to the work list.

           How do I avoid tracing through the same code several times if there
           are more than one label in an EBB?  Could have an early out by checking
           with the map to see if it's classified as code/label already.

	 */
}


void map_dump()
{
	char	*fname = "runtime/ka655x.bin.map";
	FILE	*f;

	if ((f = fopen(fname, "w")) == NULL) {
		perror("fopen()");
		fprintf(stdout, "can't create map file.\n");
		exit(1);
	}

	for (unsigned i = 0; i < blob_size; i+=64) {
		fprintf(f, "%04X_%04X  ", SPLIT(i));

		for (unsigned j = 0; (j < 64) && (i+j < blob_size); j++) {
			if ((j == 16) || (j == 32) || (j == 48))
				fprintf(f, " ");
			fprintf(f, "%c", map[i+j]);
		}

		fprintf(f, "\n");
	}

	fclose(f);
}


/***/


void read_ctrl_file(const char *fname)
{
	char	*ctrlname = mem_sprintf("%s.ctrl", fname);
	char	*ctrl = malloc(1024*1024);
	size_t	 ctrl_size;

	FILE	*f;

	/* read the file into memory (the ctrl array) */
	if ((f = fopen(ctrlname, "r")) == NULL) {
		/* can't read .ctrl file -- it probably doesn't exist */
		free(ctrlname);
		return;
	}

	if ((ctrl_size = fread(ctrl, /* size */ 1, /* nmemb */ sizeof ctrl-1, f)) == 0) {
		perror("fread()");
		fprintf(stdout, "can't read '%s' file.\n", ctrlname);
		fclose(f);
		exit(1);
	}

	if (ferror(f)) {
		perror("something went wrong during the .ctrl file reading.");
		fclose(f);
		exit(1);
	}

	fclose(f);

	free(ctrlname);

	/* parse file in memory */
	ctrl[ctrl_size] = '\0';
	parse_init(ctrl);

	parse_begin();
	parse_symbol("{");
	parse_symbol("}");
	parse_eof();
	if (!parse_ok) {
		fprintf(stderr, "couldn't parse .ctrl file\n");
		exit(1);
	}

	/* clean up */
	parse_done();
	free(ctrl);
}



/* create a directory for the HTML (the new directory may be in a subdirectory
   or may be anywhere else in the file system).

     xxx/index.html
     xxx/xxxx.png
     xxx/xxxx.png
     xxx/layout.css
     xxx/code.js

   abort if xxx already exists as a file, continue if it's a directory.
   overwrite index.html, png-files, etc in the directory if they exist.
 */
struct outfiles *open_out_files(const char *fname, const char *outname)
{
	struct outfiles		*f = malloc(sizeof(struct outfiles));
	memset(f, 0x0, sizeof(struct outfiles));

	read_ctrl_file(fname);

	char	*dirname = mem_sprintf("%s.html", outname);

	/* mkdir -- allow failure  (rwxrwxr-x) */
	if (mkdir(dirname, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) < 0) {
		if (errno != EEXIST) {
			perror("mkdir()");
			fprintf(stderr, "Can't create directory with HTML files ('%s').\n",
				dirname);
			exit(1);
		}
	}

	/* fstat() -- is the existing name a file or a directory? */
	struct stat	statinfo;

	if (stat(dirname, &statinfo) < 0) {
		perror("stat()");
		fprintf(stderr, "Can't create directory with HTML files ('%s').\n",
			dirname);
		exit(1);
	}
	if (!S_ISDIR(statinfo.st_mode)) {
		fprintf(stderr, "Can't create directory with HTML files ('%s').\n",
			dirname);
		exit(1);
	}

	/* create a timestamp string to put in .asm/.lst/.html/.map/.info */
	time_t		t;
	t = time(NULL);

	struct tm	*now;
	now = localtime(&t);
	assert(now);

	char		time_str[100];
	if (strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", now) == 0) {
		fprintf(stderr, "strftime() failed.\n");
		exit(1);
	}

	/* size of input file */
	if (stat(fname, &statinfo) < 0) {
		perror("stat()");
		fprintf(stderr, "Can't read the size of the input file.\n");
		exit(1);
	}
	off_t	fsize = statinfo.st_size;


	/* create/open various files */
	FILE	*cssf;

	char	*cssname = mem_sprintf("%s/layout.css", dirname);
	cssf = fopen(cssname, "w");
	if (cssf == NULL) {
		perror("fopen()");
		fprintf(stderr, "Can't create '%s/layout.css'\n", dirname);
		exit(1);
	}
	fprintf(cssf, "#header {\n"
	              "  font-family: \"Courier New\", Courier, monospace;\n"
	              "  color:       #888;\n"
	              "}\n");
	fprintf(cssf, "table.code {\n"
                      "   width: 100%%;\n"
                      "   font-family: \"Courier New\", Courier, monospace;\n"
                      "   text-align: left;\n"
	              "}\n");
	fprintf(cssf, "\n");
	fprintf(cssf, "table.code tr:nth-child(even) { background: #F1F1F1; }\n");
	fprintf(cssf, "table.code tr:nth-child(odd)  { background: #FEFEFE; }\n");
	fprintf(cssf, "table.code tr td:hover { background: #666;  color: #FFF; }\n");
	fprintf(cssf, "\n");
	fprintf(cssf, "table.code tr td { vertical-align: top; }\n");
	fprintf(cssf, "table.code tr td:nth-child(1) { width: 1%%;  white-space: nowrap }\n");
	fprintf(cssf, "table.code tr td:nth-child(2) { width: 10%%; color: #888; }\n");
	fprintf(cssf, "table.code tr td:nth-child(3) { width: 30%%; white-space: nowrap; }\n");
	fprintf(cssf, "table.code tr td:nth-child(4) {              color: #888; }\n");
	fclose(cssf);

	char	*htmname = mem_sprintf("%s/index.html", dirname);
	f->htm = fopen(htmname, "w");
	if (f->htm == NULL) {
		perror("fopen()");
		fprintf(stderr, "Can't create '%s/index.html'\n", dirname);
		exit(1);
	}

	fprintf(f->htm, "<!DOCTYPE html>\n"
	                "<html lang=\"en\">\n"
	                "<head>\n"
	                "  <meta charset=\"utf-8\">\n"
	                "  <title>Disassembly of %s</title>\n"
	                "  <link rel=\"stylesheet\" href=\"layout.css\">\n"
		        "</head>\n"
		        "<body>\n", html_escape(fname, false).str);

	fprintf(f->htm, "<div id=\"header\">\n");
	fprintf(f->htm, "; generated %s<br>\n", time_str);
	fprintf(f->htm, ";<br>\n");
	fprintf(f->htm, "; %s&nbsp;&nbsp; %lld bytes<br>\n", fname, (long long) fsize);
	fprintf(f->htm, "<br>\n");
	fprintf(f->htm, "; all instruction bytes are listed in order.  The VAX is a little-endian<br>\n");
	fprintf(f->htm, "; machine so the operand 8F3412 is the literal 1234h, for example.<br>\n");
	fprintf(f->htm, "<br>\n<br>\n");
	fprintf(f->htm, "</div>\n");

	fprintf(f->htm, "<table class=\"code\">\n");

	/* asm and lst files */
	char	*asmname = mem_sprintf("%s.asm", outname);
	char	*lstname = mem_sprintf("%s.lst", outname);

	f->asm_ = fopen(asmname, "w");
	if (f->asm_ == NULL) {
		perror("fopen(asmname)");
		fprintf(stderr, "Can't create asm output file ('%s')\n", asmname);
		exit(1);
	}

	fprintf(f->asm_, "; generated %s\n", time_str);
	fprintf(f->asm_, ";\n");
	fprintf(f->asm_, "; %s   %lld bytes\n", fname, (long long) fsize);
	fprintf(f->asm_, "\n");
	fprintf(f->asm_, "; all instruction bytes are listed in order.  The VAX is a little-endian\n");
	fprintf(f->asm_, "; machine so the operand 8F3412 is the literal 1234h, for example.\n");
	fprintf(f->asm_, "\n\n");

	f->lst = fopen(lstname, "w");
	if (f->lst == NULL) {
		perror("fopen(lstname)");
		fprintf(stderr, "Can't create lst output file ('%s')\n", lstname);
		exit(1);
	}

	fprintf(f->lst, "; generated %s\n", time_str);
	fprintf(f->lst, ";\n");
	fprintf(f->lst, "; %s   %lld bytes\n", fname, (long long) fsize);
	fprintf(f->lst, "\n");
	fprintf(f->lst, "; all instruction bytes are listed in order.  The VAX is a little-endian\n");
	fprintf(f->lst, "; machine so the operand 8F3412 is the literal 1234h, for example.\n");
	fprintf(f->lst, "\n\n");

	/* clean up a bit */
	free(dirname);
	free(htmname);
	free(cssname);
	free(asmname);
	free(lstname);

	return f;
}


void close_out_files(struct outfiles *f, const char *fname)
{
	fprintf(f->asm_, "\n\n");
	fprintf(f->asm_, "; end of .asm disassembly of %s", fname);
	fclose(f->asm_);

	fprintf(f->lst, "\n\n");
	fprintf(f->lst, "; end of .lst disassembly of %s", fname);
	fclose(f->lst);

	fprintf(f->htm, "</table>\n");
	fprintf(f->htm, "</body>\n");
	fprintf(f->htm, "</html>\n");
	fclose(f->htm);

	free(f);
}


/* NetBSD2	ELF
   OpenBSD3	32-bit big-endian info field, lower 16 bit is ZMAGIC
   Ultrix4	32-bit little-endian info field, lower 16 bit is ZMAGIC
 */
#define ZMAGIC		0x010B		/* 0413 */
#define ELF_MAGIC	0x7F454C46	/* <DEL>ELF */

void detect_type(const char *fname)
{
	FILE		*f;
	Elf32_Ehdr	 elf_header;


	if ((f = fopen(fname, "rb")) == NULL) {
		perror("fopen()");
		fprintf(stdout, "can't open input file.\n");
		exit(1);
	}

	if ((blob_size = fread(blob, /* size */ 1, /* nmemb */ sizeof blob, f)) == 0) {
		perror("fread()");
		fprintf(stdout, "can't read blob file.\n");
		fclose(f);
		exit(1);
	}

	if (ferror(f)) {
		perror("something went wrong during the blob reading.");
		fclose(f);
		exit(1);
	}

	fclose(f);

	/* look for MAGIC values etc */

	uint32_t	info_le = ((uint32_t)(blob[3])<<24) + (blob[2]<<16) +
					     (blob[1] << 8) +  blob[0];
	uint32_t	info_be = ((uint32_t)(blob[0])<<24) + (blob[1]<<16) +
	                                     (blob[2] << 8) +  blob[3];

	/* OpenBSD3 */
	if ((info_be & 0xFFFF) == ZMAGIC) {
		printf("OpenBSD3  a.out\n");
		return;
	}

	/* Ultrix4 */
	if ((info_le & 0xFFFF) == ZMAGIC) {
		printf("Ultrix4  a.out\n");
		return;
	}

	/* ELF */
	if (info_be == ELF_MAGIC) {
		memcpy(&elf_header, blob, sizeof(elf_header));

		if (elf_header.e_machine == EM_VAX) {
			printf("ELF  VAX (FreeBSD?)\n");
			return;
		} else {
			printf("ELF, but not for the VAX.\n");
			return;
		}
	}
}


void load_blob(const char *fname)
{
	FILE	*f;

	if ((f = fopen(fname, "rb")) == NULL) {
		perror("fopen()");
		fprintf(stdout, "can't open input file.\n");
		exit(1);
	}

	if ((blob_size = fread(blob, /* size */ 1, /* nmemb */ sizeof blob, f)) == 0) {
		perror("fread()");
		fprintf(stdout, "can't read blob file.\n");
		fclose(f);
		exit(1);
	}

	if (ferror(f)) {
		perror("something went wrong during the blob reading.");
		fclose(f);
		exit(1);
	}

	fclose(f);
}


static void help()
{
		fprintf(stderr,
"dis [-m vax|sane] [-o <outname>] <binary>\n"
"\n"
"  inputs a VAX binary (raw or a.out) + possibly a <binary>.ctrl file with\n"
"  hints for the disassembler.\n"
"\n"
"  outputs <outname>.(asm|lst|info) and an HTML file in the <outname>.html\n"
"  directory.\n");
}


static void help_exit()
{
	help();
	exit(1);
}


int main(int argc, char *argv[])
{
	(void) op_len;

	/* parse command line */

	if (argc == 1) {
		help();
		exit(0);
	}

	const char	*outname = NULL;

	bool	m_arg = false;
	bool	o_arg = false;
	int	i;
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-m") == 0) {
			if (m_arg)
				help_exit();
			m_arg = true;

			if (i+1 < argc) {
				i++;
				if (strcmp(argv[i], "vax") == 0) {
					format = FMT_VAX;
					continue;
				} else if (strcmp(argv[i], "sane") == 0) {
					format = FMT_SANE;
					continue;
				}
			}
			help_exit();
		}

		if (strcmp(argv[i], "-o") == 0) {
			if (o_arg)
				help_exit();
			o_arg = true;

			if (i+1 < argc) {
				i++;
				outname = argv[i];
				continue;
			}
			help_exit();
		}
		break; /* stop after the last option */
	}

	if (i != argc-1)
		help_exit();

	const char	*fname = argv[i];
	if (!outname)
		outname = fname;

	/***/

	struct outfiles *outf = open_out_files(fname, outname);

	detect_type(fname);
	load_blob(fname);
	blob_start = 0x20040000;

	dis(outf, 0, blob_size);
//	dis(outf, 0, 0x22);
//	dis(outf, 0x24, 0x100);

	close_out_files(outf, fname);
}

