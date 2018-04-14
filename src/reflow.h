/* Copyright 2018  Peter Lund <firefly@vax64.dk>

   Licensed under GPL v2.

   ---

   Take a long string and reflow it so it fits into a narrow column.


   dis.c contains a function that escapes HTML entities and a mini module that
   outputs text into columns with automatic line wrapping.  That code may also
   end up in this module.

 */

#ifndef REFLOW__H
#define REFLOW__H

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "strret.h"

/***/


/* Reflow string into a fixed-width column, breaking lines where there is
   whitespace:

     flow = reflow_string(string, width);

     for (int i=0; i < reflow_line_cnt(flow); i++)
         printf("%s\n", reflow_line(flow, string, i);

     free(flow);

 */


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


/* lines are numbered 0..line_cnt-1

   the string s must be the same as the string s passed into reflow_string().
 */
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


#endif

