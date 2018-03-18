/* Copyright 2018  Peter Lund <firefly@vax64.dk>\

   Licensed under GPL v2.

   ---

   Shared string handling/parsing code for asm.c and dis.c


   dis.c contains a function that escapes HTMl entities and a mini module that
   outputs text into columns with automatic line wrapping.  That code may also
   end up in this module.

 */

#include "shared.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


/* returns a fresh heap allocated string -- which we don't always bother to
   free() because the resulting memory leaks are insignificantly small.
 */
char *mem_sprintf(const char *fmt, ...)
{
	va_list		ap;

	/* How long is the output?   Test with a NULL buffer. */
	va_start(ap, fmt);
	int sze = vsnprintf(NULL, 0, fmt, ap);
	va_end(ap);

	/* negative = error */
	if (sze < 0)
		return NULL;

	/* get a buffer big enough for the output + '\0' */
	sze++;
	char	*p = malloc(sze);
	assert(p);

	/* actually produce the output string -- all the *sprintf() functions
	   output a terminating '\0', which is nice.
	 */
	va_start(ap, fmt);
	sze = vsnprintf(p, sze, fmt, ap);
	va_end(ap);
	if (sze < 0) {
		free(p);
		return NULL;
	}

	return p;
}


/***/


/* O(n) in the length of the string */
void trim_trailing_space(char *s)
{
	char	*p = s + strlen(s);

	while (p > s) {
		p--;
		if (isspace(*p))
			*p = '\0';
		else
			break;
	}
}

/* remove right-most file extension -- leaves input untouched and returns a new
   string that should be freed after use.
 */
char *cut_ext(const char *fname)
{
	assert(fname);

	char	*p = strrchr(fname, '.');

	if (!p)
		p = (char *) fname + strlen(fname);

	size_t	len = p - fname;

	char	*buf = malloc(len + 1);
	memcpy(buf, fname, len);
	buf[len] = '\0';

	return buf;
}



/***/


/*
   FIXME
   Instead of using &nbsp; for buf_instr[], we could use "white-space: pre;"
   in the CSS.
 */
struct bigstr_ret html_escape(const char *s, bool nbsp)
{
	/* be very careful -- max theoretical expansion is 6x */
	assert(strlen(s) < sizeof(struct bigstr_ret) / 6);

	struct bigstr_ret	buf;
	char		*p = buf.str;

	do {
		switch (*s) {
		case '<': sprintf(p, "&lt;");  p+=4;  break;
		case '>': sprintf(p, "&gt;");  p+=4;  break;
		case '&': sprintf(p, "&amp;"); p+=5;  break;
		case ' ': if (nbsp) {
				sprintf(p, "&nbsp;"); p+=6;  break;
			  }
		default:
			*p++ = *s;
		}
	} while (*s++);
	return buf;
}


/***/


/* describes how a long string gets broken up and reflown to fit into multiple
   lines of a column.

   line_cnt can be 0.

   line_start[] is the index into a string where the line begins,
   line_len[] is the length in bytes of the line.  The line has no leading or
   trailing whitespace.
 */
struct reflow {
	int		line_cnt;
	unsigned	colwidth;
	unsigned	line_start[1000];	/* crazy big */
	unsigned	line_len[1000];		/* crazy big */
};


/* analyze a string and describe how to reflow it -- the return value should
   be free()'d after use.
 */
struct reflow *reflow_string(const char *s, unsigned colwidth)
{
	/* word_cnt can be 0.

	   word_start[] is the index into a string where the word begins,
	   word_len[] is the length in bytes of the word.  The word has no
	   leading or trailing whitespace.
	 */
	int		word_cnt;
	unsigned	word_start[1000];	/* crazy big */
	unsigned	word_len[1000];		/* crazy big */

	struct reflow	*flow = malloc(sizeof(struct reflow));

	/* analyze string for word start/len */
	int		 state = 0;
	const char	*p = s;
	word_cnt = 0;
	for (int i=0; *p; i++, p++) {
		switch (state) {

		/* eat whitespace before a word, start new word if not whitespace */
		case 0:	if (isspace(*p)) {
				/* nop */
			} else {
				word_cnt++;
				word_start[word_cnt-1] = i;
				word_len  [word_cnt-1] = 1;
				state = 1;
			}
			break;

		/* are we still in a word? */
		case 1: if (isspace(*p)) {
				state = 0;
			} else {
				word_len[word_cnt-1]++;
			}
			break;
		}
	}

	/* reflow */
	flow->colwidth = colwidth;
	if (word_cnt == 0) {
		flow->line_cnt = 0;
		return flow;
	}

	/* there is at least one word so there is at least one line, which starts
	   where the first word starts (which may be after some leading white-
	   space).
	 */
	flow->line_cnt = 1;
	flow->line_start[0] = word_start[0];

	for (int i=0; i < word_cnt; i++) {
		/* line length including current word

		   word_start[i] - line_start[line_cnt-1] is the line length
		   up to just before the current word (including whitespace).
		   Adding word_len[i] gives us the whole line length.
		 */
		unsigned	newlen = word_start[i] -
					 flow->line_start[flow->line_cnt-1] +
					 word_len[i];

		if (newlen <= colwidth) {
			/* if the current word fits on the current line, update
			   the line length.
			 */
			flow->line_len[flow->line_cnt-1] = newlen;
		} else {
			/* the current word doesn't fit on the current line so
			   start a new line consisting of only the current word.

			   if the current word is very long, it may overflow the
			   column all by itself.  We'll let the reflow_output()
			   routine handle that.
			 */
			flow->line_cnt++;
			flow->line_start[flow->line_cnt-1] = word_start[i];
			flow->line_len  [flow->line_cnt-1] = word_len  [i];
		}
	}
	/* there is no need to post process any of the line_start/line_end
	   data structures because they have been continuously updated.
	 */

	return flow;
}


struct str_ret reflow_line(struct reflow *flow, const char *s, int line)
{
	struct str_ret	buf;

	assert(flow);
	assert(s);

	if (line >= flow->line_cnt) {
		memset(buf.str, ' ', flow->colwidth);
		buf.str[flow->colwidth] = '\0';
		return buf;
	}

	/* line numbers */
	assert(line >= 0);

	/* line length check */
	assert((flow->line_len[line] <= flow->colwidth) && (flow->colwidth < sizeof(buf.str)));

	/* we are not running out of input string, are we? */
	assert(flow->line_start[line] + flow->line_len[line] <= strlen(s));

	/* fill buffer with spaces */
	memset(buf.str, ' ', flow->colwidth);
	buf.str[flow->colwidth] = '\0';

	/* copy from s to buffer */
	unsigned bytes = flow->line_len[line];
	/* very long words can overflow the column width all by themselves */
	if (bytes > flow->colwidth)
		bytes = flow->colwidth;
	memcpy(buf.str, s + flow->line_start[line], bytes);

	/* we're done! */
	return buf;
}


int reflow_line_cnt(struct reflow *flow)
{
	assert(flow);

	return flow->line_cnt;
}


/***/


/* !parse_ok means that any parse error makes all subsequent parse calls fail +
   errors are sticky.

   you have to call rollback() to reenable parsing.

   Parsing is transactional: start a transaction with begin(), end/abort it with
   commit()/rollback().
 */

static const char	*nextp;
       bool		 parse_ok;

static unsigned	 	 parse_sp;
static const char	*open_parses[100];


/* initialize the parser with a string (clean up not necessary) */
void parse_init(const char *s)
{
	nextp = s;
	parse_sp = 0; /* no open parses */
	parse_ok = true;	/* everything's ok so far */
}


/* signal that we are done parsing */
void parse_done()
{
	/* poison/clear parser data */
	nextp = NULL;
	parse_sp = -1;
	parse_ok = false;
}


void parse_begin()
{
	/* push parse position to stack */

	assert(parse_ok);

	assert(parse_sp < ARRAY_SIZE(open_parses));
	open_parses[parse_sp++] = nextp;
}


void parse_commit()
{
	/* pop stack, throw popped value away */

	assert(parse_ok);

	assert(parse_sp > 0);
	parse_sp--;
}


void parse_rollback()
{
	/* pop stack, set nextp to popped value */
	assert(parse_sp > 0);
	nextp = open_parses[--parse_sp];
	parse_ok = true;
}


/* match optional whitespace */
void parse_skipws()
{
	if (!parse_ok)
		return;

	while (isspace(*nextp))
		nextp++;
}


/* match mandatory whitespace */ 
void parse_ws()
{
	if (!parse_ok)
		return;

	if (isspace(*nextp)) {
		nextp++;
		parse_skipws();
	} else {
		parse_ok = false;
	}
}


/* [_a-zA-Z][_a-zA-Z0-9]* */
bool parse_id(char **s)
{
	if (!parse_ok)
		return false;

	parse_skipws();
	if (isalpha(*nextp) || (*nextp == '_')) {
		const char	*start = nextp;

		nextp++;
		/* [a-zA-Z][a-zA-Z0-9_]* */
		while (isalpha(*nextp) || isdigit(*nextp) || (*nextp == '_'))
			nextp++;

		int	len = nextp - start;
		char	*p = malloc(len+1);
		memcpy(p, start, len);
		p[len] = '\0';

		*s = p;
		return true;
	} else {
		parse_ok = false;
		return false;
	}
}


/* string surrounded by double quotes (") */
bool parse_string(char **s)
{
	if (!parse_ok)
		return false;

	parse_skipws();
	if (*nextp == '"') {
		nextp++;

		const char	*start = nextp;

		/* FIXME backslash escapes \\, \n, \r, \t, \u+XX */
		/* [a-zA-Z][a-zA-Z0-9_]* */
		while ((*nextp) && (*nextp != '"'))
			nextp++;

		if (*nextp == '\0')
			return false;

		int	len = nextp - start - 2;
		char	*p = malloc(len+1);
		memcpy(p, start, len);
		p[len] = '\0';

		*s = p;
		return true;
	} else {
		parse_ok = false;
		return false;
	}
}


/* hex number (without leading '0x') */
bool parse_hex(unsigned long *x)
{
	if (!parse_ok)
		return false;

	parse_skipws();
	if (isdigit(*nextp)) {
		const char	*start = nextp;

		/* [0-9a-fA-F][0-9a-fA-F_]*

		   FIXME  _ in numbers
		 */


		nextp++;

		/* [0-9]* */
		while (isdigit(*nextp))
			nextp++;

		int	len = nextp - start;
		char	*p = malloc(len+1);
		char	*end;

		/* FIXME use a loop, don't copy '_' */
		memcpy(p, start, len);
		p[len] = '\0';

		errno = 0;
		*x = strtoul(p, &end, 16);
		free(p);

		if ((*x == ULONG_MAX) && (errno == ERANGE)) {
			parse_ok = false;
			return false;
		}

		return true;
	} else {
		parse_ok = false;
		return false;
	}
}


/* decimal -- no 0b/0/0x prefixes -- implicit parse_skipws() */
bool parse_int(int *x)
{
	if (!parse_ok)
		return false;

	parse_skipws();

	/* [0-9] */
	if (isdigit(*nextp)) {
		const char	*start = nextp;

		nextp++;

		/* [0-9]* */
		while (isdigit(*nextp))
			nextp++;

		int	len = nextp - start;
		char	*p = malloc(len+1);
		char	*end;
		long	 tmp;

		memcpy(p, start, len);
		p[len] = '\0';

		errno = 0;
		tmp = strtol(p, &end, 10);
		free(p);

		if ((tmp == LONG_MAX) && (errno == ERANGE)) {
			parse_ok = false;
			return false;
		}
		if (tmp > INT_MAX) {
			parse_ok = false;
			return false;
		}

		*x = tmp;
		return true;
	} else {
		parse_ok = false;
		return false;
	}
}


/* decimal -- or binary/octal/hex with 0b/0/0x prefixes

   FIXME  num8, num16, num32, num64?
 */
bool parse_num(int64_t *x)
{
	if (!parse_ok)
		return false;

	parse_skipws();
	if (isdigit(*nextp)) {
		const char	*start = nextp;

		/* [0-9a-fA-F][0-9a-fA-F_]*

		   FIXME  _ in numbers
		 */


		nextp++;

		/* [0-9]* */
		while (isdigit(*nextp))
			nextp++;

		int	len = nextp - start;
		char	*p = malloc(len+1);
		char	*end;

		/* FIXME use a loop, don't copy '_' */
		memcpy(p, start, len);
		p[len] = '\0';

		/* FIXME replace strtoull() with something handwritten that
		   works on 64-bit values.
		 */
		errno = 0;
		*x = strtoull(p, &end, 16);
		free(p);

		if (((uint64_t) *x == ULONG_MAX) && (errno == ERANGE)) {
			parse_ok = false;
			return false;
		}

		return true;
	} else {
		parse_ok = false;
		return false;
	}
}


/* match s */
bool parse_symbol(const char *s)
{
	if (!parse_ok)
		return false;

	parse_skipws();
	if (strncmp(s, nextp, strlen(s)) == 0) {
		nextp += strlen(s);
		return true;
	} else {
		parse_ok = false;
		return false;
	}
}


/* match ch exactly -- no whitespace skipping */
bool parse_ch(char ch)
{
	if (!parse_ok)
		return false;
	if (*nextp == ch) {
		nextp++;
		return true;
	} else {
		parse_ok = false;
		return false;
	}
}



/* match ch exactly (except for case) -- no whitespace skipping */
bool parse_chx(char ch)
{
	if (!parse_ok)
		return false;
	if (toupper(*nextp) == toupper(ch)) {
		nextp++;
		return true;
	} else {
		parse_ok = false;
		return false;
	}
}


/* match one of multiple possible characters -- no whitespace skipping */
bool parse_oneof_ch(const char *s, char *ch)
{
	if (!parse_ok)
		return false;
	char	tmpch = *nextp;
	if ((tmpch != '\0') && (strchr(s, tmpch) != NULL)) {
		nextp++;
		*ch = tmpch;
		return true;
	} else {
		parse_ok = false;
		return false;
	}
}


/* are we done? */
bool parse_eof()
{
	if (!parse_ok)
		return false;

	parse_skipws();

	if (*nextp == '\0')
		return true;
	else {
		parse_ok = false;
		return false;
	}
}



