/* Copyright 2018  Peter Lund <firefly@vax64.dk>\

   Licensed under GPL v2.

   ---

   Experiment with SUB and CMP flags.  I think this code generates them
   correctly and it shows that CMP and SUB really do set the flags differently.

 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>


int main()
{
	unsigned diff_cnt = 0;
	unsigned neq_cnt  = 0;

	for (uint32_t i=0; i <= 0xFFFF; i++) {
		uint8_t	x = (i >> 8) & 0xFF;
		uint8_t y =        i & 0xFF;

		/* sub */
		bool	sub_n, sub_z, sub_v, sub_c;
		uint8_t z = y - x;

		sub_n = z >> 7;
		sub_z = z == 0;
		sub_v = ((x & 0x80) != (y & 0x80)) || ((x & 0x80) == (z & 0x80));
		sub_c = z < y;

		/* cmp */
		bool	cmp_n, cmp_z, cmp_v, cmp_c;
		cmp_n = (int8_t) x < (int8_t) y;
		cmp_z = x == y;
		cmp_v = 0;
		cmp_c = x < y;

		/* */
		if ((i & 0xFF) == 0x00) {
			if (i != 0) {
				printf("\n\n");
			}
			printf("                            y-x           NZVC   NZVC              \n");
			printf(" x            y             z             sub    cmp      x <=> y  \n");
			printf("------------|-------------|-------------|--------------|-----------|\n");
		}

		if (sub_n == cmp_n) {
			neq_cnt++;
		}

		if ((sub_n != cmp_n) || (sub_z != cmp_z) || (sub_v != cmp_v) || (sub_c != cmp_c)) {
			diff_cnt++;
			printf("%02X/%3u/%4d | %02X/%3u/%4d | %02X/%3u/%4d | %d%d%d%d   %d%d%d%d  ",
			       x,x,(int8_t)x, y,y,(int8_t)y, z,z,(int8_t)z,
			       sub_n, sub_z, sub_v, sub_c,
			       cmp_n, cmp_z, cmp_v, cmp_c);


			printf("|");
			if ((int8_t) x < (int8_t) y)
				printf(" < ");
			else
				printf("   ");
			if (x == y)
				printf(" == ");
			else
				printf("    ");

			if (x < y)
				printf(" <u ");
			else
				printf("    ");
			printf("|");
			if (sub_n == cmp_n) {
				printf(" ! ");
			}

			printf("\n");
		}
	}

	printf("%u/%u  %g%%\n", diff_cnt, 0x10000, (diff_cnt * 100.0) / 0x10000);
	printf("neq_cnt: %u\n", neq_cnt);
	return EXIT_SUCCESS;
}

