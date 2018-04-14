/* Copyright 2018  Peter Lund <firefly@vax64.dk>

   Licensed under GPL v2.

   ---

   Some useful macros.

 */

#ifndef MACROS__H
#define MACROS__H


/* for nicer output of 32-bit hex numbers, use with %04X_%04X */
#define SPLIT(x)	((x) >> 16) & 0xFFFF, (x) & 0xFFFF

/* how many elements? */
#define ARRAY_SIZE(x)	(sizeof(x)/sizeof((x)[0]))

/* */
#define UNREACHABLE()	do { assert(0); __builtin_unreachable(); } while (0)

#endif

