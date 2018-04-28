/* Copyright 2018  Peter Lund <firefly@vax64.dk>

   Licensed under GPL v2.

   ---

   VAX instruction decoder.  Takes a list of VAX instructions as input and
   outputs a corresponding list of µops.

     ./revax-uop xxx

   FIXME takes a dummy argument for now.

 */

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "macros.h"
#include "strret.h"

#include "fragments.h"

#include "vax-instr.h"
#include "vax-ucode.h"
#include "vax-fraglists.h"


#define STATIC static
#include "op-sim-support.h"
#include "op-sim.h"

#include "op-val-support.h"
#include "op-val.h"

#include "dis-uop.h"

/***/

struct str_ret opcode(int op)
{
	struct str_ret	s;

	if (op < 0x100)
		sprintf(s.str, "%02X", op);
	else
		sprintf(s.str, "FD %02X", op & 0xFF);
	return s;
}


void dis(int uaddr)
{
	for (unsigned i=0; i < ARRAY_SIZE(ulabels); i++)
		if (ulabels[i].value == uaddr)
			printf("%s:\n", ulabels[i].name);

	dis_uinstr(uaddr, ARRAY_SIZE(ucode) - uaddr, DIS_STOP, ucode);
}


/* analyze a single VAX instruction */
void analyze(unsigned cnt, uint8_t blob[cnt])
{
	/* FIXME handle blob[] out of range access */

	/* FIXME disassemble instruction */


	/* --- */

	unsigned	idx = 0;
	int		op;

	/* decode opcode */
	switch (blob[0]) {
	case 0xFC:	/* XFC nn */
		printf("FC = XFC nn two-byte instruction.\n");
		return;
	case 0xFD:	/* normal two-byte instruction */
		op = blob[1] + 0x100;
		idx = 2;
		break;
	case 0xFE:
	case 0xFF:	/* reserved two-byte instruction */
		printf("%02X = reserved two-byte instruction.\n", blob[0]);
		return;

	default:
		op = blob[0];
		idx = 1;
	}

	assert((op >= 0) && (op <= 511));

	if (strlen(mne[op]) != 0)
		printf("; %-6s", mne[op]);

	/* reserved instruction? */
	uint32_t op_addr = ustart[op];
	if (op_addr == LBL_EXC_RESERVED) {
		printf(" -- *** %s is a reserved/unimplemented opcode ***\n", opcode(op).str);
		goto end_of_instruction;
	}

	printf(" -- opcode: %s, op_addr: %d,  %d operand(s)\n",
		opcode(op).str, op_addr, op_cnt[op]);
	printf("\n");

	/* analyze operands */

	struct {
		int	group;
		int	cl;
	} analysis[6];

	for (unsigned i=0; i < op_cnt[op]; i++) {
		printf(";  #%d:", i+1);

		if (ops[op][i*3] == 'b') {
			/* branch operand */

			analysis[i].group = GROUP_BRANCH;
			analysis[i].cl    = -1;

			printf("  \"%s\"", fragment_group[GROUP_BRANCH].name);
			printf("\n");

			if (ops[op][i*3+1] == 'b')
				idx += 1;
			else
				idx += 2;
		} else {
			/* normal operand */

			struct fields	fields;
			struct sim_ret	sim_ret;

			/* decode operand */
			memset(&fields, 0xFF, sizeof(fields));
			sim_ret = op_sim(blob+idx, &fields, op_width[op][i], op_ifp[op][i]);

			/* validate operand */
			if ((sim_ret.cnt <= 0) || !op_val(blob+idx, op_width[op][i])) {
				printf("\n");
				printf(";  can't decode operand!  (cnt: %d)\n", sim_ret.cnt);
				goto end_of_instruction;
			}

			/* group/classification/label */
			analysis[i].group = frag_list[op][i];
			analysis[i].cl = sim_ret.cl;

			/* dump operand group */
			printf("  \"%s\"\n", fragment_group[analysis[i].group].name);

			/* dump operand bytes */
			printf("; ");
			for (int j=0; j < sim_ret.cnt; j++)
				printf(" %02X", blob[idx+j]);
			printf("\n");

			/* dump decoded fields */
			printf(";  ");
			if (fields.Rn != -1)
				printf("Rn=%d ", fields.Rn);
			if (fields.Rx != -1)
				printf("Rx=%d ", fields.Rx);
			if (fields.addr != -1)
				printf("addr=%04X_%04X ", SPLIT(fields.addr));
			if (fields.disp != -1)
				printf("disp=%04X_%04X ", SPLIT(fields.disp));
			if ((fields.imm.val[0] != (uint32_t) -1) || (fields.imm.val[1] != (uint32_t) -1)) {
				printf("imm[0]=%04X_%04X ", SPLIT(fields.imm.val[0]));
				if (fields.imm.val[1] != (uint32_t) -1)
					printf("imm[1]=%04X_%04X ", SPLIT(fields.imm.val[1]));
			}
			if (fields.lit6 != -1)
				printf("lit6=%02X ", fields.lit6);
			printf("\n");

			/* step forward to next operand */
			idx += sim_ret.cnt;
		}

		printf("\n");
	}


	/* disasm µop -- PRE EXE POST */

	printf("; PRE phase\n");
	printf("\n");
	for (unsigned i=0; i < op_cnt[op]; i++) {
		printf(";  #%d", i+1);

		int	group = analysis[i].group;
		int	cl    = analysis[i].cl;

		if (fragment_group[group].isbranch) {
			printf("\n");
			dis(LBL_BRANCH);
			printf("\n");
			continue;
		}

		int	frag;

		switch (cl) {
		case CLASS_IMM:
			frag = fragment_group[group].frags[FRAG_PRE][0];
			break;
		case CLASS_REG:
			frag = fragment_group[group].frags[FRAG_PRE][1];
			break;
		default:
			frag = fragment_group[group].frags[FRAG_PRE][2];
			break;
		}

		switch (frag) {
		case FRAG_NONE:
			printf("  --\n");
			break;
		case FRAG_ERR:
			printf("  reserved operand\n");
			break;
		case FRAG_ADDR:
			printf("  addr fragment\n");
			dis(cl);
			break;
		default:
			printf("\n");
			dis(frag);
		}

		switch (cl) {
		case CLASS_IMM:
		case CLASS_REG:
			break;
		default:
			frag = fragment_group[group].frags[FRAG_PRE][3];
			if (frag != FRAG_NONE) {
				printf("\n");
				dis(frag);
			}
			break;
		}

		printf("\n");
	}

	printf("; EXE phase\n");
	printf("\n");
	dis(op_addr);
	printf("\n");

	printf("; POST phase\n");
	printf("\n");
	for (unsigned i=0; i < op_cnt[op]; i++) {
		printf(";  #%d", i+1);

		int	group = analysis[i].group;
		int	cl    = analysis[i].cl;

		/* branches are handled in PRE */
		if (fragment_group[group].isbranch) {
			printf("  --\n");
			continue;
		}

		int	frag;

		switch (cl) {
		case CLASS_IMM:
			frag = fragment_group[group].frags[FRAG_POST][0];
			break;
		case CLASS_REG:
			frag = fragment_group[group].frags[FRAG_POST][1];
			break;
		default:
			frag = fragment_group[group].frags[FRAG_POST][2];
			break;
		}

		switch (frag) {
		case FRAG_NONE:
			printf("  --\n");
			break;
		case FRAG_ERR:
			printf("  reserved operand\n");
			break;
		case FRAG_ADDR:
			printf("  addr fragment\n");
			dis(cl);
			break;
		default:
			printf("\n");
			dis(frag);
		}

		/* no fragment group has two POST phase memory fragments */
		assert(fragment_group[group].frags[FRAG_POST][3] == FRAG_NONE);

		printf("\n");
	}


end_of_instruction:
	printf("; -------------------------\n");
	printf("\n");
	printf("\n");
}


/***/

static void help()
{
		fprintf(stderr,
"revax-uop <source>\n"
"\n"
"  inputs a VAX assembly file.\n"
"\n"
"  outputs a list of µops.\n");
}


static void help_exit()
{
	help();
	exit(1);
}


int main(int argc, char *argv[argc])
{
	/* parse command line */

	if (argc != 2)
		help_exit();

	if (strcmp(argv[1], "--version") == 0) {
		printf("revax-uop %s (commit %s)\n", VERSION, GITHASH);
		printf("compiled %s on %s with %s.\n", NOW, PLATFORM, CCVER);
		printf("\n");
		printf("  %s\n", REVAXURL);

		exit(0);
	}

	/* FIXME the command-line argument is ignored      */
	/* FIXME the instructions to analyze are hardcoded */

	/* let's analyze some VAX instructions */

	/* simple instructions */
	analyze(3, (uint8_t []) {0x80, 0x51, 0x52});			/*     ADDB2   r1, r2         */
	analyze(3, (uint8_t []) {0xA0, 0x51, 0x52});			/*     ADDW2   r1, r2         */
	analyze(3, (uint8_t []) {0xC0, 0x51, 0x52});			/*     ADDL2   r1, r2         */
	analyze(4, (uint8_t []) {0x81, 0x51, 0x52, 0x53});		/*     ADDB3   r1, r2, r3     */

	/* all addressing modes */
	analyze(3, (uint8_t []) {0xD0, 0x17, 0x57});				/* MOVL   S^#23, r7		*/
	analyze(7, (uint8_t []) {0xD0, 0x8F, 0xD2, 0x04, 0x00, 0x00, 0x58});	/* MOVL	  I^1234, r8		*/

	analyze(2, (uint8_t []) {0x94, 0x50});					/* CLRB   r0			*/
	analyze(2, (uint8_t []) {0x94, 0x60});					/* CLRB   (r0)			*/
	analyze(2, (uint8_t []) {0x94, 0x70});					/* CLRB   -(r0)			*/
	analyze(2, (uint8_t []) {0x94, 0x80});					/* CLRB   (r0)+			*/
	analyze(2, (uint8_t []) {0x94, 0x90});					/* CLRB   @(r0)+		*/
	analyze(6, (uint8_t []) {0x94, 0x9F, 0x78, 0x56, 0x34, 0x12});		/* CLRB   @#0x1234_5678		*/

	analyze(3, (uint8_t []) {0x94, 0x41, 0x60});				/* CLRB   (r0)[r1]		*/
	analyze(3, (uint8_t []) {0x94, 0x41, 0x70});				/* CLRB   -(r0)[r1]		*/
	analyze(3, (uint8_t []) {0x94, 0x41, 0x80});				/* CLRB   (r0)+[r1]		*/
	analyze(3, (uint8_t []) {0x94, 0x41, 0x90});				/* CLRB   @(r0)+[r1]		*/
	analyze(7, (uint8_t []) {0x94, 0x41, 0x9F, 0x78, 0x56, 0x34, 0x12});	/* CLRB   @#0x1234_5678[r1]	*/

	analyze(3, (uint8_t []) {0x94, 0xA1, 0x02});				/* CLRB   B^2(r1)		*/
	analyze(3, (uint8_t []) {0x94, 0xAF, 0xDB});				/* CLRB   B^0x2004_0002 	*/
	analyze(3, (uint8_t []) {0x94, 0xB1, 0x02});				/* CLRB   @B^2(r1)		*/
	analyze(3, (uint8_t []) {0x94, 0xBF, 0xD5});				/* CLRB   @B^0x2004_0002	*/

	analyze(4, (uint8_t []) {0x94, 0xC1, 0x02, 0x00});			/* CLRB   W^2(r1)		*/
	analyze(4, (uint8_t []) {0x94, 0xCF, 0xCE, 0xFF});			/* CLRB   W^0x2004_0002		*/
	analyze(4, (uint8_t []) {0x94, 0xD1, 0x02, 0x00});			/* CLRB   @W^2(r1)		*/
	analyze(4, (uint8_t []) {0x94, 0xDF, 0xC6, 0xFF});			/* CLRB   @W^0x2004_0002	*/

	analyze(6, (uint8_t []) {0x94, 0xE1, 0x02, 0x00, 0x00, 0x00});		/* CLRB   L^2(r1)		*/
	analyze(6, (uint8_t []) {0x94, 0xEF, 0xBC, 0xFF, 0xFF, 0xFF});		/* CLRB   L^0x2004_0002 	*/
	analyze(6, (uint8_t []) {0x94, 0xF1, 0x02, 0x00, 0x00, 0x00});		/* CLRB   @L^2(r1)		*/
	analyze(6, (uint8_t []) {0x94, 0xFF, 0xB0, 0xFF, 0xFF, 0xFF});		/* CLRB   @L^0x2004_0002	*/

	analyze(4, (uint8_t []) {0x94, 0x42, 0xA1, 0x02});			/* CLRB   B^2(r1)[r2]		*/
	analyze(4, (uint8_t []) {0x94, 0x42, 0xAF, 0xA6});			/* CLRB   B^0x2004_0002[r2]	*/
	analyze(4, (uint8_t []) {0x94, 0x42, 0xB1, 0x02});			/* CLRB   @B^2(r1)[r2]		*/
	analyze(4, (uint8_t []) {0x94, 0x42, 0xBF, 0x9E});			/* CLRB   @B^0x2004_0002[r2]	*/

	analyze(5, (uint8_t []) {0x94, 0x42, 0xC1, 0x02, 0x00});		/* CLRB   W^2(r1)[r2]		*/
	analyze(5, (uint8_t []) {0x94, 0x42, 0xCF, 0x95, 0xFF});		/* CLRB   W^0x2004_0002[r2]	*/
	analyze(5, (uint8_t []) {0x94, 0x42, 0xD1, 0x02, 0x00});		/* CLRB   @W^2(r1)[r2]		*/
	analyze(5, (uint8_t []) {0x94, 0x42, 0xDF, 0x8B, 0xFF});		/* CLRB   @W^0x2004_0002[r2]	*/

	analyze(7, (uint8_t []) {0x94, 0x42, 0xE1, 0x02, 0x00, 0x00, 0x00});	/* CLRB   L^2(r1)[r2]		*/
	analyze(7, (uint8_t []) {0x94, 0x42, 0xEF, 0x7F, 0xFF, 0xFF, 0xFF});	/* CLRB   L^0x2004_0002[r2]	*/
	analyze(7, (uint8_t []) {0x94, 0x42, 0xF1, 0x02, 0x00, 0x00, 0x00});	/* CLRB   @L^2(r1)[r2]		*/
	analyze(7, (uint8_t []) {0x94, 0x42, 0xFF, 0x71, 0xFF, 0xFF, 0xFF});	/* CLRB   @L^0x2004_0002[r2]	*/

	/* branches */
	analyze(2, (uint8_t []) {0x12, 0xFE});				/* L1: BNEQ    L1			*/
	analyze(3, (uint8_t []) {0x31, 0xFB, 0xFF});			/*     BRW     L1			*/
	analyze(6, (uint8_t []) {0x3D, 0x51, 0x52, 0x53, 0xF5, 0xFF});	/*     ACBW    r1, r2, r3, L1		*/
	analyze(3, (uint8_t []) {0xF4, 0x51, 0xF2});			/*     SOBGEQ  r1, L1			*/
	analyze(4, (uint8_t []) {0xF3, 0x51, 0x52, 0xEE});		/*     AOBLEQ  r1, r2, L1		*/
	analyze(3, (uint8_t []) {0xE8, 0x50, 0xEB});			/*     BLBS    r0, L1			*/
	analyze(4, (uint8_t []) {0xE2, 0x50, 0x51, 0xE7});		/*     BBSS    r0, r1, L1		*/

	/* all fragment groups */

	/* rb/rw/rl/rf */
	analyze(2, (uint8_t []) {0x95, 0x50});				/* L2: TSTB   r0			*/
	/* rq/rd/rg */
	analyze(3, (uint8_t []) {0x7D, 0x50, 0x52});			/*     MOVQ   r0, r2			*/
	/* mb/mw/ml/mf */
	analyze(2, (uint8_t []) {0x96, 0x50});				/*     INCB   r0			*/
	/* mi */
	analyze(3, (uint8_t []) {0x58, 0x02, 0x50});			/*     ADAWI  S^#0x0002, r0		*/
	analyze(3, (uint8_t []) {0x58, 0x02, 0x60});			/*     ADAWI  S^#0x0002, (r0)		*/
	/* mq/md/mg */
	analyze(3, (uint8_t []) {0x60, 0x50, 0x52});			/*     ADDDD2 r0, r2			*/
	/* wb/ww/wl/wf */
	analyze(2, (uint8_t []) {0x94, 0x50});				/*     CLRB   r0			*/
	/* wq/wd/wg */
	analyze(2, (uint8_t []) {0x7C, 0x50});				/*     CLRQ   r0			*/
	/* ab/aw/al/aq */
	analyze(2, (uint8_t []) {0x9F, 0x60});				/*     PUSHAB (r0)			*/
	/* vr */
	analyze(5, (uint8_t []) {0xEE, 0x00, 0x03, 0x60, 0x51});	/*     EXTV   S^#0, S^#3, (r0), r1	*/
	/* vm */
	analyze(5, (uint8_t []) {0xF0, 0x51, 0x00, 0x03, 0x60});	/*     INSV   r1, S^#0, S^#3, (r0)	*/
	/* v1 */
	analyze(4, (uint8_t []) {0xE0, 0x00, 0x60, 0xDC});		/*     BBS    S^#0, (r0), L2		*/
	/* vi */
	analyze(4, (uint8_t []) {0xE6, 0x00, 0x60, 0xD8});		/*     BBSSI  S^#0, (r0), L2		*/
	/* bb/bw */
	analyze(2, (uint8_t []) {0x11, 0xD6});				/*     BRB    L2			*/

	return EXIT_SUCCESS;
}

