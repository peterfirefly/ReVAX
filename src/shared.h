/* Copyright 2018  Peter Lund <firefly@vax64.dk>\

   Licensed under GPL v2.

 */

/* Shared string handling/parsing code for asm.c and dis.c


   dis.c contains a function that escapes HTMl entities and a mini module that
   outputs text into columns with automatic line wrapping.  That code may also
   end up in this module.

 */

#ifndef SHARED__H
#define SHARED__H

#include <stdint.h>
#include <stdbool.h>


/* for nicer output of 32-bit hex numbers, use with %04X_%04X */
#define SPLIT(x)	((x) >> 16) & 0xFFFF, (x) & 0xFFFF

/* how many elements? */
#define ARRAY_SIZE(x)	(sizeof(x)/sizeof((x)[0]))


/***/


/* use struct return to return strings from functions -- this is much better
   than passing a pointer to a function scope static storage char buffer or
   heap allocating all the time.  Strings returned inside structs don't have
   to be explicitly freed.
 */
struct str_ret {
	char	str[100];
};


struct bigstr_ret {
	char	str[6*1024];
};


/***/


/* returns a fresh heap allocated string -- which we don't always bother to
   free() because the resulting memory leaks are insignificantly small.
 */
char *mem_sprintf(const char *fmt, ...);


/* O(n) in the length of the string */
void trim_trailing_space(char *s);

/* remove right-most file extension -- leaves input untouched and returns a new
   string that should be freed after use.
 */
char *cut_ext(const char *fname);


/***/


struct bigstr_ret html_escape(const char *s, bool nbsp);


/***/

/* Reflow string into a fixed-width column, breaking lines where there is
   whitespace:

     flow = reflow_string(string, width);

     for (int i=0; i < reflow_line_cnt(flow); i++)
         printf("%s\n", reflow_line(flow, string, i);

     free(flow);

 */


/* analyze a string and describe how to reflow it -- the return value should
   be free()'d after use.
 */
struct reflow *reflow_string(const char *s, unsigned colwidth);

/* lines are numbered 0..line_cnt-1

   the string s must be the same as the string s passed into reflow_string().
 */
struct str_ret reflow_line(struct reflow *flow, const char *s, int line);

int reflow_line_cnt(struct reflow *flow);


/***/


/* Simple transactional parser routines.

   Useful for building a recursive descent parser that looks a lot like a
   combinator parser.
 */


/* !parse_ok means that any parse error makes all subsequent parse calls fail
   (errors are sticky).

   You have to call rollback() to reenable parsing.

   Calling begin() or commit() while !parse_ok is an error (checked with an assertion).

   Parsing is transactional: start a transaction with begin(), end/abort it with
   commit()/rollback().
 */

extern bool	 parse_ok;


/* initialize the parser with a string (clean up not necessary) */
void parse_init(const char *s);

/* signal that we are done parsing */
void parse_done();

/* transaction control */
void parse_begin();
void parse_commit();
void parse_rollback();

/* match optional whitespace */
void parse_skipws();

/* match mandatory whitespace */ 
void parse_ws();

/* [_a-zA-Z][_a-zA-Z0-9]* -- implicit parse_skipws()  */
bool parse_id(char **s);

/* string surrounded by double quotes (") -- implicit parse_skipws()  */
bool parse_string(char **s);

/* hex number (without leading '0x') -- implicit parse_skipws() */
bool parse_hex(unsigned long *x);

/* decimal -- no 0b/0/0x prefixes -- implicit parse_skipws() */
bool parse_int(int *x);

/* decimal -- or binary/octal/hex with 0b/0/0x prefixes  -- implicit parse_skipws() 

   FIXME  num8, num16, num32, num64?
 */
bool parse_num(int64_t *x);

/* match s -- implicit parse_skipws() */
bool parse_symbol(const char *s);

/* match ch exactly -- no whitespace skipping */
bool parse_ch(char ch);

/* match ch exactly (except for case) -- no whitespace skipping */
bool parse_chx(char ch);

/* match one of multiple possible characters -- no whitespace skipping */
bool parse_oneof_ch(const char *s, char *ch);

/* are we done? */
bool parse_eof();

#endif

