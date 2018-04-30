/* Copyright 2018  Peter Lund <firefly@vax64.dk>

   Licensed under GPL v2.

 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "macros.h"

#include "vax-instr.h"
#include "vax-ucode.h"


#define DIS_STOP	true
#define DIS_CONT	false

/* disassemble uop_cnt µops from ucode[] starting at index i.

   perhaps stop at "---" (when 'last' flag is set).
 */
static void dis_uinstr(int i, int uop_cnt, bool stop, struct uop ucode[])
{
	while (uop_cnt--) {
		struct uop	u = ucode[i];

		const char	*name;
		if (u.op == U_BCC) {
			if (u.utarget & U_EXC_MASK)
				name = "exc";
			else
				name = "bcc";
		} else {
			name = uop[u.op].name;
		}
		printf("%3d:   %-9s ", i, name);

		int	fields = 0;
		char	buf[100] = {[0] = '\0'};
		char	*p = buf;

		if (uop[u.op].fields & UF_IMM) {
			if (fields) p += sprintf(p, ", ");
			if (u.imm == U_IMM_IMM)
				p += sprintf(p, "<imm>");
			else
				p += sprintf(p, "0x%04X_%04X", SPLIT(u.imm));
			fields++;
		}

		if (uop[u.op].fields & UF_S1) {
			if (fields) p += sprintf(p, ", ");
			p += sprintf(p, "%s",   ureg[u.s1]);
			fields++;
		}
		if (uop[u.op].fields & UF_M1) {
			if (fields) p += sprintf(p, ", ");
			p += sprintf(p, "[%s]", ureg[u.s1]);
			fields++;
		}

		if (uop[u.op].fields & UF_S2) {
			if (fields) p += sprintf(p, ", ");
			p += sprintf(p, "%s",   ureg[u.s2]);
			fields++;
		}

		if (uop[u.op].fields & UF_M2) {
			if (fields) p += sprintf(p, ", ");
			p += sprintf(p, "[%s]", ureg[u.s2]);
			fields++;
		}

		if (uop[u.op].fields & UF_DST) {
			if (fields) p += sprintf(p, ", ");
			p += sprintf(p, "%s", ureg[u.dst]);
			fields++;
		}

		if (uop[u.op].fields & UF_CC) {
			if (fields) p += sprintf(p, ", ");
			p += sprintf(p, "%s", ccname[u.cc]);
			fields++;
		}

		if (uop[u.op].fields & UF_UTARGET) {
			if (fields) p += sprintf(p, ", ");
			p += sprintf(p, "%d", u.utarget & ~U_EXC_MASK);
			fields++;
		}

		if (uop[u.op].fields & (UF_WIDTH | UF_FLAGS)) {
			while (strlen(buf) < 25)
				p += sprintf(p, " ");

			p += sprintf(p, "-- ");
		}

		if (uop[u.op].fields & UF_WIDTH) {
			p += sprintf(p, "%2s ", uwidth[u.width]);
		} else {
			p += sprintf(p, "   ");
		}

		if (uop[u.op].fields & UF_FLAGS) {
			p += sprintf(p, "%s", uflag[u.flags]);
		}

		printf("%s\n", buf);
		if (u.last) {
			printf("       ───\n");
			if (stop)
				break;
		}

		i++;
	}
}

