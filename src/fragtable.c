/* Copyright 2018  Peter Lund <firefly@vax64.dk>

   Licensed under GPL v2.

   ---

   generate a table of fragment group lists per instruction -- used in sim


   ./fragtable > src/vax-fraglists.h

   The input data are the revised table of operand lists in vax-tables.h,
   with operand types for interlocked access and for fields + the table of
   fragment groups in fragments.h.

   The output is a table with a list of indices into the fragment groups table.
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "macros.h"
#include "vax-instr.h"
#include "fragments.h"

int fraggrp(const char *optype)
{
	for (unsigned i=0; i < ARRAY_SIZE(fragment_group); i++)
		if (strstr(fragment_group[i].name, optype))
			return i;

	fprintf(stderr, "No fragment group for operand type '%s'.\n", optype);
	exit(1);
}


int main()
{
	/* create a timestamp string */
	time_t		 t;
	struct tm	*now;
	char		 time_str[100];

	t = time(NULL);
	now = localtime(&t);
	assert(now);
	if (strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", now) == 0) {
		perror("strftime()");
		fprintf(stderr, "can't create timestamp string.\n");
		exit(1);
	}

	printf(
"/* Table of fragment group lists -- for sim\n"
"\n"
"   Each opcode maps to a list of indices into the fragment group table\n"
"   in src/fragments.h.\n"
"\n"
"   Generated by fragtable.c -- %s\n"
"\n"
" */\n"
"\n"
"#ifndef VAX_FRAGLISTS__H\n"
"#define VAX_FRAGLISTS__H\n"
"\n"
"#define GROUP_BRANCH\t%d\n"
"\n"
"unsigned frag_list[512][6] = {\n", time_str, fraggrp("bb/bw"));

	for (unsigned op=0; op<512; op++) {
		if (op == 0) {
			printf("/* single-byte opcodes */\n");
			printf("\n");
		} else if (op == 256) {
			printf("/* FD prefix */\n");
			printf("\n");
		}

		if (strlen(sim_ops[op])) {
			printf("/* %02X %-6s */  {", op & 0xFF, mne[op]);

			/* loop through operand types and find them in the
			   fragment group list.
			 */
			for (unsigned i=0; i<op_cnt[op]; i++) {
				if (i > 0)
					printf(", ");

				char	optype[3];
				optype[0] = sim_ops[op][i*3 +0];
				optype[1] = sim_ops[op][i*3 +1];
				optype[2] = '\0';

				printf("%d", fraggrp(optype));
			}

			printf("},\n");
		} else {
			printf("/* %02X %-6s */  {},\n", op & 0xFF, "---");
		}

		if (((op & 0xF) == 0xF) && (op != 511)) {
			printf("\n");
		}
	}
	printf("};\n");
	printf(
"\n"
"#endif /* VAX_FRAGLISTS__H */\n"
"\n");
	return EXIT_SUCCESS;
}

