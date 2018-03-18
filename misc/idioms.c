/* Copyright 2018  Peter Lund <firefly@vax64.dk>\

   Licensed under GPL v2.

   ---

   B/W/L extraction idioms testbench

   BYTE() is not tested -- we don't care about is code size/performance since
   it is used only in asm.

   B/W/L are used in dis/sim and especially the latter is important.

   Newist gcc/clang compilers can do extraction and sign extension using a
   single instruction (MOVSB/MOVSW) and handle 32-bit extraction with a single
   instruction as well.

   Older ones can only do so if we use some ugly pointer casting and unaligned
   access + assume a little-endian machine.

   Haven't decided on double casting or signed bitfields as the workhorse
   trick.  The latter requires the use of gcc's statement expression extension.

   I don't expect the code to run on anything but x86 and ARM, compiled with
   gcc/clang, so I can basically do what I want.

   All the idioms ought to be verified for undefined behaviour with American
   Fuzzy Lop and gcc/clang's sanitizers.

   ---

   Idiom detection generally work better with '|' than with '+'.
   Be careful with int promotion -- make sure you are shifting unsigned numbers.

   ---

   gcc-xxx/clang-xxx -c -g -W -Wall src/idioms.c
   objdump -d -M intel idioms.o

   gcc-xxx/clang-xxx -S -masm=intel -fverbose-asm -W -Wall src/idioms.c
   less idioms.s
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* packing/unpacking */
#define BYTE(x, n)	(((x) >> (n*8)) & 0xFF)


/* double cast trick works in both gcc and clang.
   B   is recognized as an idiom by at least gcc 4.7+ and clang 3.9+
   W/L are recognized as idioms by at least clang 5.0+
 */
#define B(x)		((int32_t)(int8_t) ((&(x))[0]))

#define W(x)		((int32_t)(int16_t)(((unsigned) ((&(x))[0])     ) | \
				            ((unsigned) ((&(x))[1]) <<  8)))

#define L(x)		((int32_t)         ((uint32_t) ((&(x))[0])      ) | \
					   ((uint32_t) ((&(x))[1]) <<  8) | \
					   ((uint32_t) ((&(x))[2]) << 16) | \
					   ((uint32_t) ((&(x))[3]) << 24))



/* ok:
     clang-3.8
     clang-3.9
     clang-4.0
     clang-5.0
     gcc-4.8
     gcc-5.4
     gcc-7.3
   bad:
 */
int32_t test1(uint8_t b[9])
{
	return B(b[1]);
}


/* ok:
     clang-5.0
     gcc-5.4
     gcc-7.3
     gcc-8-20180218
   bad:
     clang-3.8
     clang-3.9
     clang-4.0
     gcc-4.8
 */
int32_t test2(uint8_t b[9])
{
	return W(b[1]);
}


/* ok:
     clang-5.0
     gcc-5.4
     gcc-7.3
     gcc-8-20180218
   bad:
     clang-3.8
     clang-3.9
     clang-4.0
     gcc-4.8
 */
int32_t test3(uint8_t b[9])
{
	return L(b[1]);
}


int32_t test4(uint8_t b[9])
{
	return (int32_t)(int16_t)((b[2]<<8) + b[1]);
}


int32_t test5(uint8_t b[9])
{
	return ((b[2]<<8) + b[1]);
}


int32_t test6(uint8_t b[9])
{
	return (b[1] + (b[2] << 8));
}


int32_t test7(uint8_t b[9])
{
	return ((b[1]<<8) + b[0]);
}


int32_t test8(uint8_t b[9])
{
	return (b[0] + (b[1]<<8));
}


#if 0
int32_t test9(uint8_t *restrict b)
{
	return (b[0] + (b[1]<<8));
}
#endif


/* ok:
     gcc-5.4
     gcc-7.3
     gcc-8-20180218

   bad:
     clang-5.0
 */
int32_t test10(uint8_t b[9])
{
	unsigned char	*p = &b[0];

	return (p[0] | (p[1]<<8));
}


int32_t test11(uint16_t x)
{
	return (((x >> 8) & 0xFF) | (((x & 0xFF) << 8)));
}


/* ok:
     clang-5.0

   bad:
     gcc-5.4
     gcc-7.3
     gcc-8-20180218
 */
int32_t test12(uint8_t b[9])
{
	unsigned char	*p = &b[0];

	return (int16_t)(p[0] | (p[1]<<8));
}



/* ok:
     clang-5.0
     gcc-5.4
     gcc-7.4
     gcc-8-20180218

   bad:
     gcc-4.7
     gcc-4.8

  %&"%#! int promotion rules!  That's why the (unsigned) cast matters.
 */
uint16_t test13(uint8_t b[9])
{
	unsigned char	*p = &b[0];

	return (((unsigned) p[1]<<8) | (unsigned) p[0]);
//	return (p[0] | (p[1]<<8));
}


#define Lx(x)		((int32_t)         ((&(x))[0]      ) + \
					   ((&(x))[1] <<  8) + \
					   ((&(x))[2] << 16) + \
					   ((&(x))[3] << 24))
/* ok:
     clang-5.0

   bad:
     clang-4.0
     gcc-5.4
     gcc-7.3
     gcc-8-20180218
 */
uint32_t test14(uint8_t b[9])
{
	return ((uint32_t) b[0]      ) +
	       ((uint32_t) b[1] <<  8) +
	       ((uint32_t) b[2] << 16) +
	       ((uint32_t) b[3] << 24);
}


/* ok:
     clang-5.0
     gcc-5.4
     gcc-7.3
     gcc-8-20180218

   bad:
     clang-4.0
     gcc-4.8
     gcc-4.9

   changing + to | helps.
 */
uint32_t test15(uint8_t b[9])
{
	return ((uint32_t) b[0]      ) |
	       ((uint32_t) b[1] <<  8) |
	       ((uint32_t) b[2] << 16) |
	       ((uint32_t) b[3] << 24);
}


/* ok:
     clang-5.0
     gcc-5.4
     gcc-7.3
     gcc-8-20180218

   bad:
     clang-4.0
     gcc-4.8
     gcc-4.9

   changing + to | helps.
 */
int32_t test16(uint8_t b[9])
{
	return ((uint32_t) b[0]      ) |
	       ((uint32_t) b[1] <<  8) |
	       ((uint32_t) b[2] << 16) |
	       ((uint32_t) b[3] << 24);
}


/* ok:
     clang-5.0
     gcc-5.4
     gcc-7.3
     gcc-8-20180218

   bad:
     clang-4.0
     gcc-4.8
     gcc-4.9

   changing + to | helps.
 */
int32_t test17(uint8_t b[9])
{
	return (int16_t)(((uint32_t) b[0]      ) |
	                 ((uint32_t) b[1] <<  8));
}


/* ok:
     clang-5.0
     gcc-5.4
     gcc-7.3
     gcc-8-20180218

   bad:
     clang-4.0
     gcc-4.7
     gcc-4.8
     gcc-4.9

   changing + to | helps.
 */
int32_t test18(uint8_t b[9])
{
	struct	{signed x:16;} s;

	return s.x = (((uint32_t) b[0]      ) |
	              ((uint32_t) b[1] <<  8));
}


int32_t test19(uint8_t b[9])
{
	struct	{signed x:16;} s;

	return s.x = (((unsigned) b[0]      ) |
	              ((unsigned) b[1] <<  8));
}


/* ok:
     clang-4.0
     gcc-4.7
     gcc-8-20180218

   will do unaligned reads, assumes little-endian
 */
int32_t test20(uint8_t b[9])
{
	uint32_t	*p = (uint32_t *) &b[1];
	uint32_t	x = *p;

	return x;
}


/* ok:
     clang 3.8
     clang 3.9
     clang 4.0
     clang 5.0
     gcc 4.7
     gcc 4.8
     gcc 4.9
     gcc 5.4
     gcc 7.3
     gcc-8-20180218
   bad:

   will do unaligned reads, assumes little-endian   
 */
int32_t test21(uint8_t b[9])
{
	uint16_t	*p = (uint16_t *) &b[1];
	struct {signed x:16;} s;

	return s.x = *p;
}


int main()
{
	printf("%d\n", test1((uint8_t [9]) {0x78, 0x56, 0x34, 0x12}));
	printf("%d\n", test2((uint8_t [9]) {0x78, 0x56, 0x34, 0x12}));
	printf("%d\n", test3((uint8_t [9]) {0x78, 0x56, 0x34, 0x12}));

	printf("%d\n", test4((uint8_t [9]) {0x78, 0x56, 0x34, 0x12}));
	return EXIT_SUCCESS;
}

