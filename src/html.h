/* Copyright 2018  Peter Lund <firefly@vax64.dk>

   Licensed under GPL v2.

   ---

   HTML escape.

 */

#ifndef HTML__H
#define HTML__H

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "strret.h"

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
			  /* fall-through */
		default:
			*p++ = *s;
		}
	} while (*s++);
	return buf;
}


#endif

