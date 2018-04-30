/* Copyright 2018  Peter Lund <firefly@vax64.dk>

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

#include "strret.h"
#include "reflow.h"
#include "string-utils.h"
#include "html.h"

#include "parse.h"

#include "vax-instr.h"

#define STATIC static
#include "op-dis-support.h"
STATIC struct dis_ret op_dis_sane(uint8_t b[MAX_OPLEN], uint32_t pc, int width, enum ifp ifp) __attribute__((unused));
#include "op-dis.h"

/* op_sim() is used to decode operand 3 of CASE instructions */
#include "vax-ucode.h"		/* necessary for op-sim.h */
#include "op-sim-support.h"
#include "op-sim.h"

#include "op-val-support.h"
#include "op-val.h"


/***/


uint32_t	blob_start;
uint8_t		blob[1024*1024];
uint32_t	blob_size;


/***/

/* fetch block


  bool fetch(uint32_t addr, unsigned bytecnt, uint8_t dst[bytecnt])

  alternative:

    fetch n bytes from addr into dstbuf
    if that is impossible, fill remaining bytes with 0x00

    return no of valid bytes?
    return last valid addr?
 */




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



static void clear_buf(struct buf *buf)
{
	buf->addr[0] = '\0';
	buf->bytes[0] = '\0';
	buf->instr[0] = '\0';
	buf->comment[0] = '\0';
}


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


static void output_empty_line(struct outfiles *f)
{
	/* .asm file */
	fprintf(f->asm_, "\n");

	/* .lst file */
	fprintf(f->lst, "\n");

	/* HTML */
	fprintf(f->htm, "<TR>\n");
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
		clear_buf(&buf);

		/* addr */
		sprintf(buf.addr, "%04X_%04X", SPLIT(blob_start + idx));


		/* decode opcode */
		switch (blob[idx]) {
		case 0xFC:	/* XFC nn */
			if (idx+1 >= end_idx) {
				sprintf(buf.instr, "XFC <incomplete>");
				sprintf(buf.bytes, "FC");
				sprintf(buf.comment, "FC is the first byte of an \"extended\" instruction");
				output(f, &buf);
				return;
			}
			sprintf(buf.instr, "XFC #0x%02X", blob[idx+1]);
			sprintf(buf.bytes, "%02X%02X", blob[idx], blob[idx+1]);
			idx += 2;
			output(f, &buf);
			continue;
		case 0xFD:	/* normal two-byte instruction */
			if (idx+1 >= end_idx) {
				sprintf(buf.instr, " FD <incomplete>");
				sprintf(buf.bytes, "FD");
				sprintf(buf.comment, "FD is the start of a normal two-byte instruction");
				output(f, &buf);
				return;
			}
			op = blob[idx+1] + 0x100;
			oplen = 2;
			sprintf(buf.bytes, "%02X%02X ", blob[idx], blob[idx+1]);
			break;
		case 0xFE:
		case 0xFF:
			/* two-byte reserved instructions */
			if (idx+1 >= end_idx) {
				sprintf(buf.instr, " %02X <incomplete>", blob[idx]);
				sprintf(buf.bytes, "%02X", blob[idx]);
				sprintf(buf.comment, "%02X is the start of an undefined two-byte instruction", blob[idx]);
				output(f, &buf);
				return;
			}
			sprintf(buf.instr, ".byte   0x%02X, 0x%02X", blob[idx], blob[idx+1]);
			sprintf(buf.bytes, "%02X%02X", blob[idx], blob[idx+1]);
			sprintf(buf.comment, " *** reserved instruction");
			idx += 2;
			output(f, &buf);
			continue;
		default:
			op = blob[idx];
			oplen = 1;
			sprintf(buf.bytes, "%02X ", blob[idx]);
		}

		if (strlen(mne[op]) == 0) {
			if (op <= 255)
				sprintf(buf.instr, ".byte   0x%02X", op);
			else
				sprintf(buf.instr, ".byte   0xFD, 0x%02X", op & 0xFF);
			sprintf(buf.comment, " *** reserved instruction");
			idx += oplen;
			output(f, &buf);
			continue;
		}

		/* ok, we have an opcode and it's a valid instruction */
		sprintf(buf.instr, "%-6s  ", mne[op]);
		idx += oplen;

		if (strcmp(mne[op], "REI") == 0) {
			sprintf(buf.comment + strlen(buf.comment), "return from interrupt");
		}


		/* disassemble ops -- also note operand #3 for CASE instructions */
		bool		op3_const = false;
		uint32_t	op3_value = 0;
		for (unsigned i=0; i < op_cnt[op]; i++) {
			if (i > 0)
				sprintf(buf.instr + strlen(buf.instr), ", ");

			/* prepare b[], pc, width for the operand */
			uint8_t	b[MAX_OPLEN];
			if (idx+MAX_OPLEN-1 >= end_idx) {
				memset(b, 0x0, MAX_OPLEN);
				memcpy(b, blob+idx, end_idx - idx);
			} else {
				memcpy(b, blob+idx, MAX_OPLEN);
			}

			uint32_t	pc = blob_start + idx;
			int		width = op_width[op][i]; /* no of bytes */

			/* disassemble an operand given its datatype */
			if (ops[op][i*3] == 'b') {	// branch operand
				uint32_t	relative_pc = blob_start + idx;
				uint32_t	disp;

				/* bb, bw */
				switch (op_width[op][i]) {
				case 1: relative_pc += 1;
					disp = (int32_t)(int8_t) blob[idx];
					sprintf(buf.instr + strlen(buf.instr), "0x%04X_%04X", SPLIT(relative_pc + disp));
					sprintf(buf.bytes + strlen(buf.bytes), "%02X", blob[idx]);
					idx += 1;
					break;
				case 2: relative_pc += 2;
					disp = (int32_t)(int16_t) ((blob[idx+1] << 8) | blob[idx]);
					sprintf(buf.instr + strlen(buf.instr), "0x%04X_%04X", SPLIT(relative_pc + disp));
					sprintf(buf.bytes + strlen(buf.bytes), "%02X%02X", blob[idx], blob[idx+1]);
					idx += 2;
					break;
				}
				continue;
			}

			/* note operand 3 for CASEB/CASEW/CASEL */
			if ((i == 2) && ((op == 0x8F) || (op == 0xAF) || (op == 0xCF))) {
				if (((b[0] & 0xC0) == 0x00) || (b[0] == 0x8F)) {
					struct fields	fields;

					op_sim(b, &fields, width, op_ifp[op][i]);
					op3_const = true;
					op3_value = fields.imm.val[0];

					/* op_sim() sign extends, let's zero
					   extend instead.
					 */
					switch (width) {
					case 1:	op3_value = op3_value & 0xFF;
							break;
					case 2:	op3_value = op3_value & 0xFFFF;
							break;
					}
				}
			}

			struct dis_ret dis_ret;
			dis_ret = op_dis_vax(b, pc, width, op_ifp[op][i]);

			if ((dis_ret.cnt <= 0) || !op_val(b, width)) {
				sprintf(buf.instr   + strlen(buf.instr), "???");
				sprintf(buf.comment + strlen(buf.comment), "reserved operand");
			} else if (idx+dis_ret.cnt > end_idx) {
				/* output operand bytes */
				for (unsigned j=0; j<(unsigned)dis_ret.cnt; j++)
					sprintf(buf.bytes + strlen(buf.bytes), "%02X", b[j]);
				sprintf(buf.bytes + strlen(buf.bytes), " ");

				sprintf(buf.instr   + strlen(buf.instr), "???");
				sprintf(buf.comment + strlen(buf.comment), "incomplete operand");
				output(f, &buf);
				return;
			} else {
				/* output operand bytes */
				for (unsigned j=0; j<(unsigned)dis_ret.cnt; j++)
					sprintf(buf.bytes + strlen(buf.bytes), "%02X", b[j]);
				sprintf(buf.bytes + strlen(buf.bytes), " ");

				/* add disassembled operand to the disassembled instruction */
				strncat(buf.instr, dis_ret.str, sizeof(buf.instr) - strlen(buf.instr)-1);

				/* skip just past this operand */
				idx += dis_ret.cnt;
			}
		}

		/* push the output out to the world as .asm, .lst, and .html */
		output(f, &buf);

		/* special case CASEB/CASEW/CASEL */
		if ((op == 0x8F) || (op == 0xAF) || (op == 0xCF)) {
			/* The size of the jump table is given by the third
			   operand (limit).

			   A table size of 0 is not useful, so the range
			   has been shifted a bit:

			    limit  | size of table
			   ------------------------
			       0   |    1
			       1   |    2
			       2   |    3
			          ...
			     255   |  256

			   If the limit operand is not a short literal or an
			   immediate, then we don't know how big the jump table
			   is.  In practice, it is always a constant.
			 */

			if (!op3_const) {
				clear_buf(&buf);
				sprintf(buf.comment, "*** unknown jump table size!");
				output(f, &buf);
				output_empty_line(f);
			} else {
				uint32_t	relative_pc = blob_start + idx;

				/* loop through the jump table */
				for (uint32_t i=0;; i++) {
					uint16_t	disp;

					if (idx+1 > end_idx) {
						clear_buf(&buf);
						sprintf(buf.addr, "%04X_%04X", SPLIT(blob_start + idx));
						sprintf(buf.instr, ".word    ???");
						sprintf(buf.comment, "incomplete jump table");
						output(f, &buf);
						idx +=2;
						break;
					} else {
						disp = (blob[idx+1] << 8) | blob[idx];
					}

					clear_buf(&buf);
					sprintf(buf.addr, "%04X_%04X", SPLIT(blob_start + idx));
					sprintf(buf.instr, ".word   0x%04X", disp);
					sprintf(buf.bytes, "%02X%02X", disp & 0xFF, disp >> 8);
					sprintf(buf.comment, "%-3d --> %04X_%04X",
							i, SPLIT(relative_pc + (int32_t)(int16_t)(disp)));
					output(f, &buf);

					idx +=2;
					if (i == op3_value)
						break;
				}
				output_empty_line(f);
			}
		}

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
static int op_len(unsigned idx, int width) __attribute__((unused));
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
	char	*asmname = mem_sprintf("%s.disasm", outname);
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
	fprintf(f->asm_, "; end of .asm disassembly of %s\n", fname);
	fclose(f->asm_);

	fprintf(f->lst, "\n\n");
	fprintf(f->lst, "; end of .lst disassembly of %s\n", fname);
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
"revax-dis [-m vax|sane] [-o <outname>] <binary>\n"
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
	/* parse command line */

	if (argc == 1) {
		help();
		exit(0);
	}

	if (strcmp(argv[1], "--version") == 0) {
		printf("revax-dis %s (commit %s)\n", VERSION, GITHASH);
		printf("compiled %s on %s with %s.\n", NOW, PLATFORM, CCVER);
		printf("\n");
		printf("  %s\n", REVAXURL);

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
//					format = FMT_VAX;
					continue;
				} else if (strcmp(argv[i], "sane") == 0) {
//					format = FMT_SANE;
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
printf("blob_size: %d\n", blob_size);

/* FIXME "written runtime/ka655x.bin.asm"
         "written runtime/ka655x.bin.lst"
         "written runtime/ka655x.bin/html/"

         best if the filenames are easy to cut and paste

         "Writing to:" before the disassembly is just as good.
 */
	dis(outf, 0, blob_size);
//	dis(outf, 0, 0x22);
//	dis(outf, 0x24, 0x100);

	close_out_files(outf, fname);
}

