/* Copyright 2018  Peter Lund <firefly@vax64.dk>

   Licensed under GPL v2.

   ---

   VAX assembler.  Quite simple for now.

     Reads a line at a time, splits off comments and labels, identifies the
     instruction (or pseudo-instruction), reads the operands with a combinator
     parser.

     Would be a lot cooler if it had a symbol table...

     Sorely needs better error reporting.

     Doesn't support expressions in operands :(
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

#include "string-utils.h"
#include "parse.h"

#include "vax-instr.h"

#include "fp.h"

#define STATIC static
#include "op-asm-support.h"
#include "op-asm.h"

#include "op-val-support.h"
#include "op-val.h"

/***/

uint32_t	addr;		/* address we are assembling to */
unsigned	blob_idx;	/* the next index in blob[] to store to */
uint8_t		blob[1024];	/* FIXME: far too small */
bool		first = true;	/* .org only allowed as first (pseudo) instr */

enum { FMT_VAX, FMT_SANE } format = FMT_VAX;


bool pseudo_instr(int lineno)
{
	struct big_int	big;

	/* FIXME allow uppercase */
	/* FIXME allow operand lists for .byte/.word/.long/.quad/.octo */
	/* FIXME .ffloat/.dfloat/.gfloat/.hfloat */
	/* FIXME .ascii and friends */
	/* FIXME .blkx */
	/* FIXME .mask */

	parse_begin();
	parse_symbol(".org");
	parse_ws();
	parse_bigint(&big, 4);
	parse_eof();
	if (parse_ok) {
		parse_commit();

		if (!first) {
			fprintf(stderr, "line %d: .org not first (pseudo) instruction\n", lineno);
			return false;
		}

		addr = big.val[0];
		return true;
	}
	parse_rollback();

	parse_begin();
	parse_symbol(".byte");
	parse_ws();
	parse_bigint(&big, 1);
	parse_eof();
	if (parse_ok) {
		parse_commit();

		blob[blob_idx++] = BYTE(big.val[0], 0);

		addr += 1;
		return true;
	}
	parse_rollback();

	parse_begin();
	parse_symbol(".word");
	parse_ws();
	parse_bigint(&big, 2);
	parse_eof();
	if (parse_ok) {
		parse_commit();

		blob[blob_idx++] = BYTE(big.val[0], 0);
		blob[blob_idx++] = BYTE(big.val[0], 1);

		addr += 2;
		return true;
	}
	parse_rollback();

	parse_begin();
	parse_symbol(".long");
	parse_ws();
	parse_bigint(&big, 4);
	parse_eof();
	if (parse_ok) {
		parse_commit();

		blob[blob_idx++] = BYTE(big.val[0], 0);
		blob[blob_idx++] = BYTE(big.val[0], 1);
		blob[blob_idx++] = BYTE(big.val[0], 2);
		blob[blob_idx++] = BYTE(big.val[0], 3);
		addr += 4;
		return true;
	}
	parse_rollback();

	parse_begin();
	parse_symbol(".quad");
	parse_ws();
	parse_bigint(&big, 8);
	parse_eof();
	if (parse_ok) {
		parse_commit();

		blob[blob_idx++] = BYTE(big.val[0], 0);
		blob[blob_idx++] = BYTE(big.val[0], 1);
		blob[blob_idx++] = BYTE(big.val[0], 2);
		blob[blob_idx++] = BYTE(big.val[0], 3);

		blob[blob_idx++] = BYTE(big.val[1], 0);
		blob[blob_idx++] = BYTE(big.val[1], 1);
		blob[blob_idx++] = BYTE(big.val[1], 2);
		blob[blob_idx++] = BYTE(big.val[1], 3);

		addr += 8;
		return true;
	}
	parse_rollback();

	parse_begin();
	parse_symbol(".octo");
	parse_ws();
	parse_bigint(&big, 16);
	parse_eof();
	if (parse_ok) {
		parse_commit();

		blob[blob_idx++] = BYTE(big.val[0], 0);
		blob[blob_idx++] = BYTE(big.val[0], 1);
		blob[blob_idx++] = BYTE(big.val[0], 2);
		blob[blob_idx++] = BYTE(big.val[0], 3);

		blob[blob_idx++] = BYTE(big.val[1], 0);
		blob[blob_idx++] = BYTE(big.val[1], 1);
		blob[blob_idx++] = BYTE(big.val[1], 2);
		blob[blob_idx++] = BYTE(big.val[1], 3);

		blob[blob_idx++] = BYTE(big.val[2], 0);
		blob[blob_idx++] = BYTE(big.val[2], 1);
		blob[blob_idx++] = BYTE(big.val[2], 2);
		blob[blob_idx++] = BYTE(big.val[2], 3);

		blob[blob_idx++] = BYTE(big.val[3], 0);
		blob[blob_idx++] = BYTE(big.val[3], 1);
		blob[blob_idx++] = BYTE(big.val[3], 2);
		blob[blob_idx++] = BYTE(big.val[3], 3);

		addr += 16;
		return true;
	}
	parse_rollback();

	return false;
}



int read_op(uint8_t b[MAX_OPLEN], uint32_t addr, int width, enum ifp ifp)
{
	switch (format) {
	case FMT_VAX:
		{
			int cnt = op_asm_vax(b, addr, width, ifp);
			if ((cnt > 0) && !op_val(b, width))
				return -1;
			return cnt;
		}
	case FMT_SANE:
		{
			int cnt = op_asm_sane(b, addr, width, ifp);
			if ((cnt > 0) && !op_val(b, width))
				return -1;
			return cnt;
		}
	default:
		UNREACHABLE();
	}
}


/* false "please don't call me again" -- error or EOF
   true in case of success
 */
bool read_line(FILE *f, int lineno)
{
	/* let's be lazy and get the whole line as a string. */
	char		line[512];
	char		label[512];

	/* fgets() always reads a valid zero-terminated string but it may
	   contain a trailing '\n'.  It will removed either by the comment
	   cutter or the trailing space cutter.
	 */
	if (fgets(line, sizeof(line), f) == NULL) {
		if (feof(f))
			return 0;
		perror("fgets()");
		fprintf(stderr, "line %d: error reading input file.\n", lineno);
		return false;
	}

	/* cut off line comment, if any */
	/* FIXME we should ignore ; inside "" and '' */
	if (strchr(line, ';'))
		*strchr(line, ';') = '\0';

	/* cut off trailing space */
	trim_trailing_space(line);

	/* cut off leading space */
	/* FIXME slow! O(n²) */
	while (strlen(line) && isspace(line[0]))
		/* memmove() because it's an overlapping move,
		   strlen(line) bytes because the new string is strlen()-1
		   bytes long + we need to copy the '\0' terminator as well.
		 */
		memmove(line, line+1, strlen(line));

	/* label? */
	/* [a-zA-Z_][a-zA-Z0-9_$]*: */
	label[0] = '\0';
	if (strchr(line, ':')) {	/* FIXME super trashy check! */
		char	*p = line;

		/* there is no leading space -- so we can start immediately */
		if (!isalnum(*p)) {
			fprintf(stderr, "line %d: label expected.\n", lineno);
			return false;
		}

		while (isalnum(*p) || (*p == '_') || (*p == '$'))
			p++;

		/* p now points one past the label -- it should point to either
		   whitespace or ':'.
		 */

		memmove(label, line, p-line);
		label[p-line] = '\0';

		/* check there is only whitespace from now on until the ':' */
		while (*p != ':')
			if (!isspace(*p)) {

			} else
				p++;

		/* p now points to ':' */
		p++;

		/* remove label and ':' */
		memmove(line, p, strlen(p)+1);

		/* remove new leading space from the line */
		/* FIXME slow! O(n²) */
		while (strlen(line) && isspace(line[0]))
			/* memmove() because it's an overlapping move,
			   strlen(line) bytes because the new string is strlen()-1
			   bytes long + we need to copy the '\0' terminator as well.
			 */
			memmove(line, line+1, strlen(line));

		fprintf(stderr, "label: '%s'\n", label);
	}

	/* the line may be empty now, which is totally ok */
	if (strlen(line) == 0)
		return true;

	parse_init(line);

	/* pseudo instruction */
	if (pseudo_instr(lineno)) {
		first = false;
		return true;
	}

	/* instruction name + possible list of operands */
	parse_begin();
	char	*instrname;
	if (!parse_id(&instrname)) {
		parse_rollback();
		fprintf(stderr, "line %d: instruction name expected.\n", lineno);
		return false;
	}
	parse_commit();

	/* upper-case mnemonic */
	for (unsigned i=0; instrname[i]; i++)
		instrname[i] = toupper(instrname[i]);

	/* FIXME a tree or hash for faster lookups -- should probably be generated
                 by instr.pl.
	 */
	int	ino;
	for (ino=0; ino < 512; ino++)
		if (strcmp(instrname, mne[ino]) == 0)
			goto found_instr;
	for (unsigned i=0; i < ARRAY_SIZE(syn); i++) {
		if (strcmp(instrname, syn[i].name) == 0) {
			ino = syn[i].op;
			goto found_instr;
		}
	}

	fprintf(stderr, "line %d: '%s' is not an instruction.\n", lineno, instrname);
	return false;

found_instr:
	/* ino is the instruction number (0..511).

	   ops[ino] is the ops string ("rf rf mf bw ", for example)
	   op_cnt[ino] is the number of operands
	   op_width[ino][] is the operand widths
	   op_ifp[ino][] is the operand types (int/f/d/g/h)
	 */

	/* store the opcode */
	if (blob_idx + 1 + (ino>255) > sizeof(blob)) {
		fprintf(stderr, "line %d: out of blob space.\n", lineno);
		return false;
	}
	if (ino > 255) {
		blob[blob_idx++] = 0xFD;
		addr++;
	}
	blob[blob_idx++] = ino & 0xFF;
	addr++;

	/* parse operands (and store opspecs) */
	for (unsigned opidx=0; opidx < op_cnt[ino]; opidx++) {
		if (opidx > 0) {
			parse_skipws();
			parse_ch(',');
		}

		char	access = ops[ino][opidx*3];

		if (access == 'b') {	/* branch operand */
			/* bb, bw */
			switch (ops[ino][opidx*3+1]) {
			case 'b': {
				  uint32_t	relative_pc = addr+1;
				  uint8_t	disp;

				  if (!parse_branch8(&disp, relative_pc)) {
					fprintf(stderr, "line %d: not a valid byte-sized relative address.\n", lineno);
					return false;
				  }
				  if (blob_idx + 1 > sizeof(blob)) {
					fprintf(stderr, "line %d: out of blob space.\n", lineno);
					return false;
				  }
				  blob[blob_idx++] = disp & 0xFF;
				  addr += 1;
				  break;
				  }
			case 'w': {
				  uint32_t	relative_pc = addr+2;
				  uint16_t	disp;

				  if (!parse_branch16(&disp, relative_pc)) {
					fprintf(stderr, "line %d: not a valid word-sized relative address.\n", lineno);
					return false;
				  }
				  if (blob_idx + 2 > sizeof(blob)) {
					fprintf(stderr, "line %d: out of blob space.\n", lineno);
					return false;
				  }
				  blob[blob_idx++] = disp & 0xFF;
				  blob[blob_idx++] = disp >> 8;
				  addr += 2;
				  break;
				  }
			default:
				UNREACHABLE();
			}

			/* store byte/word, inc addr */
		} else {
			/* normal operand */

			uint8_t	b[MAX_OPLEN];	/* encoded operand */

			/* read_op() is a wrapper around op_asm_xxx() and op_val().

			   op_asm() checks that immediates fit into width.
			   op_val() checks that registers are valid for width.
			 */
			int cnt = read_op(b, addr, op_width[ino][opidx], op_ifp[ino][opidx]);
			if (cnt <= 0) {
				fprintf(stderr, "line %d, operand %d: not a valid operand.\n", lineno, opidx+1);
				return false;
			}

                        /* verify that access type and operand match */
			switch (access) {
			case 'r': /* all operands are valid for read */
				  break;
			case 'w':
			case 'm': /* can't be immediate, lit6 */
				  if ((b[0] & 0xC0) == 0x00) {
					fprintf(stderr, "line %d, operand %d: write/modify operands can't be short literals.\n", lineno, opidx+1);
					return false;
				  }
				  if (b[0] == 0x8F) {
				  	fprintf(stderr, "line %d, operand %d: write/modify operands can't be immediates.\n", lineno, opidx+1);
				  	return false;
				  }
				  break;
			case 'a': /* can't be register, immediate, lit6 */
				  if ((b[0] & 0xF0) == 0x50) {
					fprintf(stderr, "line %d, operand %d: address operands can't be registers.\n", lineno, opidx+1);
					return false;
				  }
				  if ((b[0] & 0xC0) == 0x00) {
					fprintf(stderr, "line %d, operand %d: address operands can't be short literals.\n", lineno, opidx+1);
					return false;
				  }
				  if (b[0] == 0x8F) {
				  	fprintf(stderr, "line %d, operand %d: address operands can't be immediates.\n", lineno, opidx+1);
				  	return false;
				  }
				  break;
			case 'v': /* can't be lit6 -- immediates are weirdly enough ok */
				  if ((b[0] & 0xC0) == 0x00) {
					fprintf(stderr, "line %d, operand %d: bit field operands can't be short literals.\n", lineno, opidx+1);
					return false;
				  }
				  break;
			default:
				UNREACHABLE();
			}

			if (blob_idx + cnt > sizeof(blob)) {
				fprintf(stderr, "line %d: out of blob space.\n", lineno);
				return false;
			}
			memcpy(blob+blob_idx, b, cnt);
			blob_idx += cnt;
			addr     += cnt;
		}
	}

	/* verify that that was the end of the instruction = the rest of the line
	   is empty.
	 */

	if (!parse_eof()) {
		fprintf(stderr, "line %d: illegal characters at end of line.\n", lineno);
		return false;
	}

	first = false;
	return true;
}


void asm_file(const char *fname)
{
	FILE		*f;
	int		 lineno;

	if ((f = fopen(fname, "rb")) == NULL) {
		perror("fopen()");
		fprintf(stderr, "can't open input file.\n");
		exit(1);
	}

	addr = 0;
	blob_idx = 0;
	memset(blob, 0x0, sizeof(blob));

	lineno = 1;
	while (read_line(f, lineno))
		lineno++;

	fclose(f);

/* FIXME don't create .raw file if there were errors */
/* FIXME allow more than one error per run */

}


void dump_blob(const char *fname)
{
	FILE		*f;

	if ((f = fopen(fname, "wb")) == NULL) {
		perror("fopen()");
		fprintf(stderr, "can't create blob file.\n");
		exit(1);
	}

	if (fwrite(blob, /* size */ 1, /* nmemb */ blob_idx, f) != blob_idx) {
		perror("fwrite()");
		fprintf(stderr, "can't write blob file.\n");
		fclose(f);
		exit(1);
	}

	if (fclose(f) != 0) {
		perror("fclose()");
		fprintf(stderr, "can't write blob file (close error).\n");
		fclose(f);
		exit(1);
	}
}


static void help()
{
		fprintf(stderr,
"revax-asm <source>\n"
"\n"
"  inputs a VAX assembly file (not in VAX MACRO format -- that would be much\n"
"  too complicated).\n"
"\n"
"  outputs a raw binary.  The output filename is created from the source filename\n"
"  by replacing the extension with '.raw'.\n");
}


static void help_exit()
{
	help();
	exit(1);
}


int main(int argc, char *argv[])
{
	/* parse command line */

	if (argc != 2)
		help_exit();

	if (strcmp(argv[1], "--version") == 0) {
		printf("revax-asm %s (commit %s)\n", VERSION, GITHASH);
		printf("compiled %s on %s with %s.\n", NOW, PLATFORM, CCVER);
		printf("\n");
		printf("  %s\n", REVAXURL);

		exit(0);
	}

	char	*fname = argv[1];
	char	*barename = cut_ext(fname);
	char	*outname = mem_sprintf("%s.%s", barename, "raw");

	asm_file(fname);
	dump_blob(outname);
	free(barename);
	free(outname);

	return EXIT_SUCCESS;
}

