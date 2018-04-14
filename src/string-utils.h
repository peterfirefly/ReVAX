/* Copyright 2018  Peter Lund <firefly@vax64.dk>

   Licensed under GPL v2.

   ---

   Some useful string manipulation routines.

 */

#ifndef STRING_UTILS__H
#define STRING_UTILS__H

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/***/


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


#endif

