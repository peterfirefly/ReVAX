/* Copyright 2018  Peter Lund <firefly@vax64.dk>

   Licensed under GPL v2.

   ---

   VAX simulator -- a simple, straightforward VAX simulator.

     It is not a complete simulator yet:
       - it does not simulate ANY hardware yet
       - it doesn't handle traps properly
       - it doesn't handle interrupts properly -- but there is also no hardware
         to generate interrupts ;)
       - queue instructions are not implemented
       - bit field instructions are not implemented
       - some of the weirder instructions are not implemented and should be
         simulated by ROM code.  This is how CVAX and many other VAX implementations
         worked.  Unfortunately, the simulation trap isn't implemented yet.

     None of the code has been seriously tested yet.

 */



#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "macros.h"



/* VAX instruction tables -- generated by instr.pl from instr.snip */
#include "vax-instr.h"

/* compiled µCode + register names, etc -- generated by uasm.pl from ucode.vu */
#include "vax-ucode.h"

#define STATIC static
#include "op-sim-support.h"
#include "op-sim.h"

#include "op-val-support.h"
#include "op-val.h"


/***/


/* can we read/write a page? (for protecting ROMs)

   both bits are set on normal memory.

   This remapping makes it easy to provide snapshot/rollback facilities in the
   simulator.
 */
#define MF_READ		1
#define MF_WRITE	2

struct mem_table {
	void	*pages[1 << 22];	/* 4 M entries => 32 MB on a 64-bit machine */
	uint8_t	 flags[1 << 22];
};

struct cpu {
	struct mem_table	*mem;	/* FIXME ptr so we can share them between CPU's */

	uint32_t	r[R_CNT];	/* GPRs, p_n, e_n, t_n */
	uint32_t	psl[2];		/* psl[1] is only valid in the lower 4 bits */
	uint32_t	preg[64];

	int		stopped;
};

/* flags -- reading */
#define N(x)	(((x) >> 3) & 1)
#define Z(x)	(((x) >> 2) & 1)
#define V(x)	(((x) >> 1) & 1)
#define C(x)	(((x) >> 0) & 1)

/* flags -- writing */
#define NZVC(N,Z,V,C)	((!!(N) << 3) | (!!(Z) << 2) | (!!(V) << 1) | (!!(C) << 0))


/***/

/* memory and I/O read/write modes

   bit 0: write
   bit 1: interlocked
   bit 2: untranslated

   access() sees all 3 bits.
   io() only sees the low 1 bit.
 */
#define MODE_LD		0x0
#define MODE_LDI	0x2
#define MODE_LDU	0x4

#define MODE_READ	0x0
#define MODE_WRITE	0x1




/* mem_r/rm/w/wu() either read/write memory, with or without interlock, or
   they access I/O space.

   Are there interlocks on I/O space operations?

   I/O operations can fail, if there is no valid hardware at the address or the
   device malfunctions.
 */


/* 0: I/O read didn't go well
   1: I/O read ok.

   len can be 1/2/4.  pa must be naturally aligned.
 */
static int io(uint32_t pa, int mode, int len, uint32_t *data, int *err) __attribute__((unused));
static int io(uint32_t pa, int mode, int len, uint32_t *data, int *err)
{
	/* simple switch at first, then something table driven, where devices
	   can register ranges of addrs they wish to get callbacks for.
	   The callback provides pa, ofs, len, data, err.  Ofs is how far into
	   the range pa is.
	 */
	(void) pa, (void) mode, (void) len, (void) data, (void) err;
	return 1;
}



/***/



/* 0:  can't translate -- exception of some kind, info in pa
   1:  translated ok

   Every translation checks whether mapping is enabled, then checks the region,
   then the region length, then the pte.

   It calls itself recursively, once, if asked to translate P0 or P1 addresses
   and mapping is enabled.  Process page tables are pageable and reside in S.
   System page tables are not pageable.
 */
static int xlat(struct cpu *cpu, uint32_t va, uint32_t *pa) __attribute__((unused));
static int xlat(struct cpu *cpu, uint32_t va, uint32_t *pa)
{
	if (cpu->preg[PR_MAPEN] & 1) {
		/* mapping enabled */
		uint32_t	base;
		uint32_t	len;

		(void) base, (void) len;

		/* check region */
		switch ((va >> 30) & 0x3) {
		case 0: /* P0 */
			base = cpu->preg[PR_P0BR];
			len  = cpu->preg[PR_P0LR];
			break;
		case 1: /* P1 */
			base = cpu->preg[PR_P1BR];
			len  = cpu->preg[PR_P1LR];
			break;
		case 2: /* S0 */
			base = cpu->preg[PR_SBR];
			len  = cpu->preg[PR_SLR];
			break;
		case 3:	/* reserved region */
			return 0;
		default:
			UNREACHABLE();
		}

		return 0;

	} else {
		/* no translation */
		*pa = va;
		return 1;
	}
}


/* 1: success
   0: failure -- no memory at address, no I/O device at address, unaligned
      I/O access, invalid page, no read/write access to page, ...

   len: 1/2/4/8
   datahi ignored unless len == 8
   err only set on failure

   The VAX defines something called "interlock granularity" which may be something
   as big as a whole 512-byte page.  All interlocks within an interlock granularity
   act as a single interlock.

 */
static int access(struct cpu *cpu, int mode, uint32_t va, int len,
           uint32_t *data, uint32_t *datahi, int *err)
{
	(void) cpu, (void) mode, (void) va, (void) len, (void) data, (void) datahi, (void) err;

	/* xlat/access check, once or twice */
	/* mem or I/O calls, once or twice
	   handle mem with memcpy()?  or with aligned access?  the latter.
	   I/O definitely needs aligned access

           use mem pages even for I/O space.  if a page is present, go use it,
           otherwise call I/O system.
	 */
	return 0;
}


/* fetch block


  bool fetch(uint32_t addr, unsigned bytecnt, uint8_t dst[bytecnt])

  alternative:

    fetch n bytes from addr into dstbuf
    if that is impossible, fill remaining bytes with 0x00

    return no of valid bytes?
    return last valid addr?

   in sim, we actually need to fetch() to go through address translation and
   access checks.  Possibly more than one address translation and address check.
   at some point, simulated bus cycles should be generated.
 */

/* fetch with memory translation, handle unaligned access.
   stop at non-existing memory or non-mapped page.

   let's ignore I/O space for now, except for ROMs.
 */
static void mem_fetch(struct cpu *cpu, uint32_t va, void *buf, size_t req_sze, size_t *fetch_sze)
{
	(void) cpu, (void) va, (void) buf, (void) req_sze, (void) fetch_sze;
}


/* how much contiguous memory? */
static void mem_init(struct cpu *cpu, size_t sze)
{
	/* how many pages? */
	if (sze & (512-1)) {
		fprintf(stderr, "mem_init(sze: %zu), not an integral number of pages.\n", sze);
		exit(1);
	}

	size_t	 pagecnt = sze >> 9;
	uint8_t	*blob    = calloc(pagecnt, 512);

	for (size_t page=0; page < pagecnt; page++) {
		cpu->mem->pages[page] = blob + page*512;
		cpu->mem->flags[page] = MF_READ | MF_WRITE;
	}
}


/* load a ROM */
static void rom_load(struct cpu *cpu, uint32_t pa, const char *fname) __attribute__((unused));
static void rom_load(struct cpu *cpu, uint32_t pa, const char *fname)
{
	(void) cpu, (void) pa, (void) fname;
}




/***/

#include "dis-uop.h"


/* instruction decode -- opcode => µop/index, expected operands */

static void decode(struct cpu *cpu, int *uop_cnt, struct uop uop[])
{
	(void) cpu, (void) uop_cnt, (void) uop;

	*uop_cnt = 0;
}




/***/


/* Sign extend to 32 bits.  The sign extension compiles down to two different
   single instructions on x86, depending on whether we are going from 8 or 16 bits.

   'static' makes it inlineable (ELF/gnu make public symbols overrideable at
   run-time which means that public functions can't be inlined).  This should
   get us down to a single instruction in many cases.
 */
static int32_t signext(int32_t x, int len)
{
	switch (len) {
	case U_WIDTH_8:
		/* two-casts-in-a-row trick :) */
		return (int32_t)(int8_t) x;
	case U_WIDTH_16:
		return (int32_t)(int16_t) x;
	case U_WIDTH_32:
		return x;
	default:
		/* U_LEN_WIDTH + illegal len values */
		UNREACHABLE();
	}
}


/* data width for uop */
static int uop_width(int len)
{
	switch (len) {
	case U_WIDTH_8 :	return 1;
	case U_WIDTH_16:	return 2;
	case U_WIDTH_32:	return 4;
	default:
		UNREACHABLE();
	}
}


/* check flags according to cc -- used by bcc-arch/bcc-µ */
static int match_cc(int flags, int cc)
{
	switch (cc) {
	case  2: /* NEG   !=          */  return ! Z(flags);
	case  3: /* EQL   =           */  return   Z(flags);
	case  4: /* GTR   >  signed   */  return !(N(flags) | Z(flags));
	case  5: /* LEQ   <= signed   */  return  (N(flags) | Z(flags));
	case  8: /* GRQ   >= signed   */  return ! N(flags);
	case  9: /* LSS   <  signed   */  return   N(flags);
	case 10: /* GTRU  >  unsigned */  return !(C(flags) | Z(flags));
	case 11: /* LEQU  <= unsigned */  return  (C(flags) | Z(flags));
	case 12: /* VC   ovrflw clear */  return ! V(flags);
	case 13: /* VS   ovrflw set   */  return   V(flags);
	case 14: /* GEQU  >= unsigned */  return ! C(flags);
	case 15: /* BLSSU <  unsigned */  return   C(flags);
	default:
		UNREACHABLE();
	}
}


static void result(struct cpu *cpu, struct uop u, int32_t x, int w)
{
	switch (w) {
	case U_WIDTH_8:
		cpu->r[u.dst] = (cpu->r[u.dst] & 0xFFFFFF00) | (x &   0xFF);
#if 0
		nzvc = ...
#endif
		break;
	case U_WIDTH_16:
		cpu->r[u.dst] = (cpu->r[u.dst] & 0xFFFF0000) | (x & 0xFFFF);
#if 0
		nzvc = ...
#endif
		break;
	case U_WIDTH_32:
		cpu->r[u.dst] = x;
#if 0
		nzvc = ...
#endif
		break;
	default:
		UNREACHABLE();
	}
}


/*
   Exceptions:
     INTO, DIV0

   Neither is actually raised by alu().  They are just reported and may or may
   not lead to an actual exception.  In both cases, the result actually gets
   written to reg/mem, i.e., the µpost flow actually runs.

   Perhaps the best way to handle this is to have yet another bitfield somewhere,
   that indicates the requested exceptions?

   I first thought that returning an exception indication or passing a reference
   to a bitmask into alu() would be good ideas.  I now think it's better to
   add a field to struct cpu.
 */
static void alu(struct cpu *cpu, struct uop u)
{
	uint32_t	old_NZVC;

	old_NZVC = cpu->psl[u.flags] & 0xF;

	switch (u.op) {
	/* compare */
	case U_CMP:
		printf("%s, %s  -- %s %s",
			regstr(u.s1).str, regstr(u.s2).str, lenstr(u.width), flagstr(u.flags));
		break;


	/* add/sub */
	case U_ADD:
		result(cpu, u, cpu->r[u.s1] + cpu->r[u.s2], u.width);
		break;
	case U_SUB:
		result(cpu, u, cpu->r[u.s1] - cpu->r[u.s2], u.width);
		break;
	case U_ADC:
		cpu->r[u.dst] = cpu->r[u.s1] + cpu->r[u.s2] + C(cpu->psl[u.flags]);
		break;
	case U_SBB:
		cpu->r[u.dst] = cpu->r[u.s1] - cpu->r[u.s2] - C(cpu->psl[u.flags]);
		break;


	/* logic */
	case U_AND:
		cpu->r[u.dst] = cpu->r[u.s1] & cpu->r[u.s2];
		break;
	case U_BIC: /* AND NOT */
		cpu->r[u.dst] = cpu->r[u.s1] &~cpu->r[u.s2];
		break;
	case U_BIS: /* OR */
		cpu->r[u.dst] = cpu->r[u.s1] | cpu->r[u.s2];
		break;
	case U_XOR:
		cpu->r[u.dst] = cpu->r[u.s1] ^ cpu->r[u.s2];
		break;


	/* "barrell shifter" */
	case U_ASHL:
		cpu->r[u.dst] = cpu->r[u.s1] << cpu->r[u.s2];
		break;
	case U_ROTL:
//		cpu->r[u.dst] = cpu->r[u.s1] << cpu->r[u.s2];
		break;


	/* MUL/DIV */
	case U_MUL:
		cpu->r[u.dst] = cpu->r[u.s1] * cpu->r[u.s2];
		break;
	case U_DIV:
		cpu->r[u.dst] = cpu->r[u.s1] / cpu->r[u.s2];
		break;

	/* no explicit operands, always set arch flags */
	case U_EMUL:
		printf("p1, p2, p3, e2'e1");
		break;

	case U_EDIV:
		printf("p1, p3'p2, p4, e1, e2");
		break;
	}

	/* overflow?

	   INTO is edge-triggered on the architectural V flag... and only if
	   we observe the edge while PSL<5> is set.

	   Popping a PSL value can also lead to INTO?  FIXME
	   In any case, that is not handled here.

           Use a temporary so we can detect V changes.
	 */
	if ((u.flags == 0) &&
	    (cpu->psl[0] & (1 << 5)) && (V(cpu->psl[0]) != V(old_NZVC))) {
		/* INTO! */
	}
}



#define UADDR_DONE	0xFFFF

/* UADDR_DONE for "done"
   nn for "please fetch me the µop flow starting at nn"
 */
static int datapath(struct cpu *cpu, int uop_cnt, struct uop uop[uop_cnt])
{
	for (int i=0; i < uop_cnt; i++) {
		struct uop	u = uop[i];

		/* µcode is never allowed to refer to r15/PC directly */
		assert(u.s1  != 15);
		assert(u.s2  != 15);
		assert(u.dst != 15);

		switch (u.op) {
		/* no operands */
		case U_NOP:
			break;
		case U_STOP:
			cpu->stopped = 1;
			return UADDR_DONE;
		case U_COMMIT:
		case U_ROLLBACK:
			/* FIXME -- not supported yet, may not need to be µops */
			assert(0);
			break;

		/* imm32, dst */
		case U_IMM:
			cpu->r[u.dst] = u.imm;
			break;

		/* cc, utarget -- flags */
		case U_BCC:
			switch (u.cc) {
			case U_CC_CC:
				assert(0);
				break;
			case U_CC_ALWAYS:
				return u.utarget;

			case U_CC_CALL:
			case U_CC_RET:
				/* FIXME -- not supported/necessary yet */
				assert(0);
				break;
			default:
				if (match_cc(cpu->psl[u.flags], u.cc))
					return u.utarget;
			}
			break;

		/* src */
		case U_JMP:
			cpu->r[15] = cpu->r[u.s1];
			break;

		/* dst */
		case U_READPC:
			/* Only works correctly if the operand decoder isn't
			   allowed to run ahead, i.e., only if the decoder
			   digests an operand, determines what microcode to
			   run, and it then gets run before the decoder looks
			   at more operands.
			 */
			cpu->r[u.dst] = cpu->r[15];
			break;

		/* src, src', dst -- len flags */
		case U_MOV:
			/* merge src and src' according to len */
			switch (u.width) {
			case U_WIDTH_8:
				cpu->r[u.dst] = (cpu->r[u.s1] & 0xFFFFFF00) | (cpu->r[u.s2] &    0xFF);
				break;
			case U_WIDTH_16:
				cpu->r[u.dst] = (cpu->r[u.s1] & 0xFFFFFF00) | (cpu->r[u.s2] & 0xFFFF);
				break;
			case U_WIDTH_32:
				cpu->r[u.dst] = cpu->r[u.s1];
				break;
			default:
				assert(0);
			}
			break;

		/* src, src', dst -- len flags */
		case U_MOVX:
			/* MOVQ is implemented as mov + mov-

			   The trouble is that the Z flag has to be set based
			   on *both* 32-bit values.
			 */
			break;

		/* s1, dst -- len */
		case U_LD:
		case U_LDI:
		case U_LDU:
			{
			uint32_t	tmp, tmphi;
			int		err;

			if (!access(cpu,
				    MODE_READ +
				    (MODE_LDI * (u.op == U_LDI)) +
				    (MODE_LDU * (u.op == U_LDU)),
				    u.s1, u.width, &tmp, &tmphi, &err)) {
				/* exception */
				return LBL_EXC_ACCESS | U_EXC_MASK;
			}

			switch (u.width) {
			case U_WIDTH_8:
			case U_WIDTH_16:
			case U_WIDTH_32:
				cpu->r[u.dst] = signext(tmp, u.width);
				break;
			}
			}
			break;

		/* s1, s2 -- len */
		case U_ST:
		case U_STI:
		case U_STU:
			{
			uint32_t	tmp, tmphi;
			int		err;

			switch (u.width) {
			case U_WIDTH_8:
			case U_WIDTH_16:
			case U_WIDTH_32:
				tmp = cpu->r[u.dst];
				break;
			}
			if (!access(cpu,
				    MODE_WRITE +
				    (MODE_LDI * (u.op == U_LDI)) +
				    (MODE_LDU * (u.op == U_LDU)),
				    u.s1, u.width, &tmp, &tmphi, &err)) {
				/* exception */
				return LBL_EXC_ACCESS | U_EXC_MASK;
			}
			}
			break;

		/* src, dst -- width */
		case U_INC:
			cpu->r[u.dst] += uop_width(u.width);
			break;
		case U_DEC:
			cpu->r[u.dst] -= uop_width(u.width);
			break;
		case U_INDEX:
			cpu->r[u.dst] = cpu->r[u.s1] * uop_width(u.width);
			break;

		/* The entire ALU group is handled outside this switch for
		   both readability reasons and in order to be nice to the
		   compiler.  Not all compilers tolerate big functions equally
		   well.
		 */
		case U_CMP:
		case U_ADD:
		case U_SUB:
		case U_MUL:
		case U_DIV:
		case U_AND:
		case U_BIC:
		case U_BIS:
		case U_XOR:
		case U_ASHL:
		case U_ROTL:
		case U_ADC:
		case U_SBB:
		case U_EMUL:
		case U_EDIV:
			{
				uint32_t	exc_addr;
				(void) exc_addr;

				alu(cpu, u);
			}
			break;


		/* s1, dst    ; s1 is a GPR with the number of a preg */
		case U_MFPR:
			/* FIXME move outside this function. */
			/* merge with U_MTPR? */
			cpu->r[u.dst] = cpu->preg[u.s1];
			break;

		/* s1, dst    ; dst is a GPR with the number of a preg */
		case U_MTPR:
			/* FIXME move outside this function. */
			/* merge with U_MFPR? */
			cpu->preg[u.dst] = cpu->r[u.s1];
			break;

		default:
			printf("#%3d", u.op);
		}
	}
	return 0;
}


/* start at a µaddr, fetch a basic block, execute it, if there was a branch,
   fetch a new basic block and repeat.
 */
static void run_flow()
{
}


/* start by looking at interrupt rq and trace flag.

   fetch instruction
   decode-opcode, figure out stuff
   decode-operand, fetch basic block, run it, fetch new basic block if branch/exception, ...
   decode next operand until we are done (or we get an exception)
   write result(s) (if we didn't get an exception)

   did we request an exception?  I think this is for faults only -- INTO, DIV0

   Do we do the stack stuff and jmp to the exception vector here or do we wait
   until the next time we get called?
 */
static void run_instruction()
{
}


/* r0..r15, NZVC -- highlight differences between reg/flags */
static void dump_regs(struct cpu *cpu, struct cpu *old_cpu)
{
	const char	*regname[] = {
		"r0    ",
		"r1    ",
		"r2    ",
		"r3    ",
		"r4    ",
		"r5    ",
		"r6    ",
		"r7    ",
		"r8    ",
		"r9    ",
		"r10   ",
		"r11   ",
		"r12/AP",
		"r13/FP",
		"r14/SP",
		"r15/PC"};

	const char	*emph = "\x1B[1m";	/* SGR set bold        */
	const char	*norm = "\x1B[0m";	/* SGR reset all attrs */

	for (int i=0; i <= 15; i++) {
		if (cpu->r[i] != old_cpu->r[i])
			printf("%s  %s%04X_%04X%s\n", regname[i], emph, SPLIT(cpu->r[i]), norm);
		else
			printf("%s  %04X_%04X\n",     regname[i],       SPLIT(cpu->r[i])      );
	}


	printf("%s%c%s%s%c%s%s%c%s%s%c%s\n",
		N(cpu->psl[0]) != N(old_cpu->psl[0]) ? emph : "", "nN"[N(cpu->psl[0])], norm,
		Z(cpu->psl[0]) != Z(old_cpu->psl[0]) ? emph : "", "zZ"[Z(cpu->psl[0])], norm,
		V(cpu->psl[0]) != V(old_cpu->psl[0]) ? emph : "", "vV"[V(cpu->psl[0])], norm,
		C(cpu->psl[0]) != C(old_cpu->psl[0]) ? emph : "", "cC"[C(cpu->psl[0])], norm);
}


static void cpu_run(struct cpu *cpu)
{
	struct cpu	old_cpu;
	uint16_t	utarget;

	dump_regs(cpu, cpu);
	memcpy(&old_cpu, cpu, sizeof(struct cpu));

	cpu->r[2] = 5;
	cpu->r[3] = 7;
	cpu->psl[0] = NZVC(1,0,1,1);

	while (!cpu->stopped) {
		struct uop	uops[MAXFLOWLEN];
		int		uop_cnt;

		uint8_t		buf[12];
		size_t		fetch_cnt;

		mem_fetch(cpu, cpu->r[15], &buf, sizeof(buf),&fetch_cnt);

		printf("PC: %04X_%04X  %02X %02X %02X %02X   %02X %02X %02X %02X   %02X %02X %02X %02X\n",
			SPLIT(cpu->r[15]),
			buf[ 0], buf[ 1], buf[ 2], buf[ 3],
			buf[ 4], buf[ 5], buf[ 6], buf[ 7],
			buf[ 8], buf[ 9], buf[10], buf[11]);

		decode(cpu, &uop_cnt, uops);

		do {
			dis_uinstr(0, uop_cnt, uops);

			utarget = datapath(cpu, uop_cnt, uops);
#if 0
			if (utarget != -1)
				fetch(utarget, &uop_cnt, uops);
#endif
		} while (utarget != 0xFFFF);
	}

	dump_regs(cpu, &old_cpu);
}


static void cpu_init(struct cpu *cpu)
{
	memset(cpu, 0x0, sizeof (struct cpu));
	cpu->mem = malloc(sizeof (struct mem_table));
	memset(cpu->mem, 0x0, sizeof (struct mem_table));
}


static void cpu_program(struct cpu *cpu)
{
	strcpy((char *) (cpu->mem->pages[0]),
#if 0
	"\xC1\x52\x53\x54"	/* ADDL3   r2, r3, r4	*/
	"\xC3\x52\x53\x54"	/* SUBL3   r2, r3, r4	*/
	"\xC0\x52\x55"		/* ADDL2   r2, r5	*/
	"\xC2\x52\x55"		/* SUBL2   r2, r5	*/

	"\xC1\x8F\xBE\xBA\xFE\xCA\x53\x54"	/* ADDL3   0xCAFEBABE, r3, r4	*/
	"\xC3\x8F\xBE\xBA\xFE\xCA\x53\x54"	/* SUBL3   0xCAFEBABE, r3, r4	*/
	"\xC0\x8F\xBE\xBA\xFE\xCA\x55"		/* ADDL2   0xCAFEBABE, r5	*/
	"\xC2\x8F\xBE\xBA\xFE\xCA\x55"		/* SUBL2   0xCAFEBABE, r5	*/

	"\xC1\x52\x8F\xBE\xBA\xFE\xCA\x54"	/* ADDL3   r2, 0xCAFEBABE, r4	*/
	"\xC3\x52\x8F\xBE\xBA\xFE\xCA\x54"	/* SUBL3   r2, 0xCAFEBABE, r4	*/

	"\xC1\x8F\xBE\xBA\xFE\xCA\x8F\xEF\xBE\xAD\xDE\x54"	/* ADDL3   0xCAFEBABE, 0xDEADBEEF, r4	*/
	"\xC3\x8F\xBE\xBA\xFE\xCA\x8F\xEF\xBE\xAD\xDE\x54"	/* SUBL3   0xCAFEBABE, 0xDEADBEEF, r4	*/

	"\xC1\x8F\xBE\xBA\xFE\xCA\x56\x57"	/* ADDL3   0xCAFEBABE, r6, r7	*/
#endif

	"\xC1\x64\x52\x57"	/* ADDL3   r2, [r4], r7 */

	"\xFF"			/* STOP			*/
	);
}


int main()
{
	struct cpu	cpu;

	/* disable stdout buffering so we still get output in case of seg faults */
	setbuf(stdout, NULL);

	cpu_init(&cpu);
	mem_init(&cpu, 512 * 1024 * 1024);
	cpu_program(&cpu);
	cpu_run(&cpu);
}


