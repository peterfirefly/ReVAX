/* Copyright 2018  Peter Lund <firefly@vax64.dk>

   Licensed under GPL v2.

   ---

   Strings wrapped in structs -- makes it nice and clean to return strings
   from functions that don't have to be explicitly freed or have space explicitly
   allocated by the caller.

 */

#ifndef STRRET__H
#define STRRET__H


/* use struct return to return strings from functions -- this is much better
   than passing a pointer to a function scope static storage char buffer or
   heap allocating all the time.  Strings returned inside structs don't have
   to be explicitly freed.

   A typical compiler implementation does NOT copy big character arrays around.
   Instead, a struct is allocated on the stack (with a simple stack pointer
   subtraction), and a pointer to the struct is passed as a parameter to the
   function (typically in a register).  The function then uses that pointer to
   fill in the first few characters in the buffer inside the struct.
 */
struct str_ret {
	char	str[100];
};


struct bigstr_ret {
	char	str[6*1024];
};

#endif

