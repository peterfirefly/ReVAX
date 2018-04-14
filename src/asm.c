/* Copyright 2018  Peter Lund <firefly@vax64.dk>

   Licensed under GPL v2.

   ---

   VAX assembler.  Quite simple for now.

     Reads a line at a time, splits off comments and labels, identifies the
     instruction (or pseudo-instruction), reads the operands with a combinator
     parser.

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

uint32_t	org;
uint8_t		blob[1024];	/* FIXME: far too small */

enum { FMT_VAX, FMT_SANE } format = FMT_SANE;


void read_lit()
{
}


bool pseudo_op(int lineno)
{
	int64_t		x;

	parse_begin();
	parse_symbol(".org");
	parse_ws();
	parse_num(&x);
	parse_eof();
	if (parse_ok) {
		parse_commit();
		/* ... */
		return true;
	}
	parse_rollback();

	parse_begin();
	parse_symbol(".vax");
	parse_eof();
	if (parse_ok) {
		parse_commit();
		format = FMT_VAX;
		return true;
	}
	parse_rollback();

	parse_begin();
	parse_symbol(".sane");
	parse_eof();
	if (parse_ok) {
		parse_commit();
		format = FMT_SANE;
		return true;
	}
	parse_rollback();

	parse_begin();
	parse_symbol(".byte");
	parse_ws();
	parse_num(&x);
	parse_eof();
	if (parse_ok) {
		parse_commit();
		if ((x < -128) || (x > 255)) {
			fprintf(stderr, "line %d: number out of range", lineno);
			return false;
		}
		return true;
	}
	parse_rollback();

	parse_begin();
	parse_symbol(".word");
	parse_ws();
	parse_num(&x);
	parse_eof();
	if (parse_ok) {
		parse_commit();
		if ((x < -32768) || (x > 65536)) {
			fprintf(stderr, "line %d: number out of range", lineno);
			return false;
		}
		return true;
	}
	parse_rollback();

	parse_begin();
	parse_symbol(".long");
	parse_ws();
	parse_num(&x);
	parse_eof();
	if (parse_ok) {
		parse_commit();
		if ((x < -128) || (x > 255)) {
			fprintf(stderr, "line %d: number out of range", lineno);
			return false;
		}
		return true;
	}
	parse_rollback();

	parse_begin();
	parse_symbol(".quad");
	parse_ws();
	parse_num(&x);
	parse_eof();
	if (parse_ok) {
		parse_commit();
		return true;
	}
	parse_rollback();

	return false;
}


bool read_vax_op(int lineno, char access, char type, uint8_t buf[17], unsigned *sze)
{
	(void) lineno;
	(void) access, (void) type, (void) buf, (void) sze;

	return true;
}


bool read_sane_op(int lineno, char access, char type, uint8_t buf[17], unsigned *sze)
{
	(void) access, (void) type;
	(void) lineno, (void) buf, (void) sze;

	/* immediates		-- 6-bit or 8F			*/
	/* absolute		-- [#addr]			*/
	/* register		-- r0..r15, ap, fp, sp, pc	*/
	/* register def		-- [reg]			*/
	/* autodec		-- [--reg]			*/
	/* autoinc		-- [reg++]			*/
	/* autoinc def		-- [[reg++]]			*/
	/* b/w/l disp		-- [reg +/- disp]		*/
	/* b/w/l disp def	-- [[reg +/- disp]]		*/

	/* absolute idx		-- [#addr + reg]		*/
	/* register def idx	-- [reg + reg], [reg + reg*sze] */
	/* autodec idx		-- */
	/* autoinc idx		-- */
	/* autoinc def idx	-- */
	/* b/w/l disp idx	-- */
	/* b/w/l disp def idx	-- */

	return false;
}


/* Parse operand, check if it's legal for the this part of the instruction.
   The largest possible operand is 1 opspec + 16 bytes for an octoword int or
   an h floating-point type.

   access:  r/w/m/a
   type:    b/w/l/q/o   f/d/g/h

   Parses using either VAX or SANE operand format.
   Bruteforce implementation: simply tries parsing one format after another
   until there is a match.
 */
bool read_op(int lineno, char access, char type, uint8_t buf[17], unsigned *sze)
{
	switch (format) {
	case FMT_VAX:
		return read_vax_op( lineno, access, type, buf, sze);
	case FMT_SANE:
		return read_sane_op(lineno, access, type, buf, sze);
	default:
		assert(0);
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
		fprintf(stdout, "line %d: error reading input file.\n", lineno);
		return false;
	}

	/* cut off line comment, if any */
	/* FIXME we should ignore ; inside "..." and '' */
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
			fprintf(stdout, "line %d: label expected.\n", lineno);
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

	/* pseudoops */
	if (pseudo_op(lineno))
		return true;

	/* instruction name + possible list of operands */
	parse_begin();
	char	*instrname;
	if (!parse_id(&instrname)) {
		parse_rollback();
		fprintf(stderr, "line %d: instruction name expected.\n", lineno);
		return false;
	}
	parse_commit();

	/* FIXME a tree or hash for faster lookups -- should probably be generated
                 by instr.pl.

           FIXME case folding.
	 */
	int	ino;
	for (ino=0; ino < 512; ino++) {
		if (strcmp(instrname, mne[ino]) == 0) {
			if (ino < 256)
				fprintf(stderr, "%-6s  %02X\n", instrname, ino);
			else
				fprintf(stderr, "%-6s  FD %02X\n", instrname, ino & 0xFF);
			goto found_instr;
		}
	}
	fprintf(stderr, "line %d: '%s' is not an instruction.\n", lineno, instrname);
	return false;

found_instr:

	/* [a-zA-Z][a-zA-Z0-9]* */

	/* check if operand list matches the instruction
	     - # of operands
	     - masks  => only immediates
	     - branch => can only be immediates
	     - addr   => can't be registers or immediates
	     - write operands can't be immediates
	     - size violations for immediates

           operands have been parsed and turned into opspecs by this point but
           they haven't been put into the blob yet.  I think.

           labels are treated as immediates.  At some point they should be
           handled as expressions -- and still be treated as immediates.
         */
	return 1;
}


void asm_file(const char *fname)
{
	FILE		*f;
	int		 lineno;

	if ((f = fopen(fname, "rb")) == NULL) {
		perror("fopen()");
		fprintf(stdout, "can't open input file.\n");
		exit(1);
	}

	org = 0;
	memset(blob, 0x0, sizeof(blob));

	lineno = 1;
	while (read_line(f, lineno))
		lineno++;

	fclose(f);
}


void dump_blob(const char *fname)
{
	FILE		*f;

	if ((f = fopen(fname, "wb")) == NULL) {
		perror("fopen()");
		fprintf(stdout, "can't create blob file.\n");
		exit(1);
	}

	if (fwrite(blob, /* size */ 1, /* nmemb */ org, f) != org) {
		perror("fwrite()");
		fprintf(stdout, "can't write blob file.\n");
		fclose(f);
		exit(1);
	}

	if (fclose(f) != 0) {
		perror("fclose()");
		fprintf(stdout, "can't write blob file (close error).\n");
		fclose(f);
		exit(1);
	}
}


static void help()
{
		fprintf(stderr,
"asm <source>\n"
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
}

