/* Copyright 2018  Peter Lund <firefly@vax64.dk>

   Licensed under GPL v2.

   ---

   Simple transactional parser routines.

   Useful for building a recursive descent parser that looks a lot like a
   combinator parser.

 */


#ifndef PARSE__H
#define PARSE__H

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "macros.h"


/***/


/* !parse_ok means that any parse error makes all subsequent parse calls fail
   (errors are sticky).

   You have to call rollback() to reenable parsing.

   Calling begin() or commit() while !parse_ok is an error (checked with an assertion).

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


/***/

/* transaction control */

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


/* [_a-zA-Z][_a-zA-Z0-9]* -- implicit parse_skipws()  */
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


/* string surrounded by double quotes (") -- implicit parse_skipws()  */
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


/* hex number (without leading '0x') -- implicit parse_skipws() */
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


/* match s -- implicit parse_skipws() */
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


#endif

