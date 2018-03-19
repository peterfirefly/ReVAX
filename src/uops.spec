# Copyright 2018  Peter Lund <firefly@vax64.dk>
#
# Licensed under GPL v2.
#
# ---
#
# This file describes the µops.  Before, they were spread out all over uasm.pl
# and src/dis-uop.h.
#
#
# Microcode formats
# -----------------
# Two formats for the µops: template mode in ROM and expanded mode in the
# pipeline.
#
# The (compressed) template mode could probably be implemented with a width of
# around 9 bits -- because very few immediate literals are going to be used in
# the microcode.  Lots of fields can be overlapped/compressed.   For example,
# the ALU µops don't use all combinations of 6-bit s1 × 6-bit s2 × 6-bit dst.
# They likely only use a dozen different combinations so 4 bits would be enough
# to specify them.
#
# The expanded form must be wide enough for 32-bit immediates (and maybe 32-bit
# PC's in 'readpc').  All other µops can be implemented with fewer bits -- around
# 26 bits for ALU ops.
#
# The expanded form is likely to be 34-36 bits.
#
#
# Phases
# ------
# Three phases: pre/exe/post.  On top of that, no-phase microcode for startup/
# reset, TLB miss, interrupts/exceptions.
#
#
# Placeholders
# ------------
# Placeholder values used in template mode:
#   <Rn>, <Rx>		-- used for address generation microcode (in pre)
#   <pre>, <exe>	-- used in pre/exe/post
#   <reg>               -- used for post fragments that write to registers
#   <cc>		-- used in exe only
#   <width>		-- used in pre/exe/post
#
# no-phase microcode is not allowed to use placeholder values.
# expanded microcode is not allowed to use placeholder values.
#
# should template form exe-phase microcode be allowed to use placeholder values?
# yes.  It is necessary for Bcc instructions (they need <cc>).  It makes it
# easier to implement microcode for many instructions (<pre>, <exe>) and it
# makes the microcode slightly shorter because instructions like ADDB/ADDW/ADDL
# can shared implementations (with <width>).
#
#
# Register names
# --------------
# Uses 6-bit register names in template mode, some of which are not actual
# registers but place holders that are replaced with actual register names
# by the decoder.
#
# Uses 5-bit register names in expanded mode.
#
# It is illegal to reference r15/PC in µops in both template and expanded forms.
# The only valid way to read PC is to use 'readpc'.  The only valid way to write
# PC is to use 'jmp'.
#
#
# Condition codes
# ---------------
# Uses 4-bit condition code in bcc/exc -- some of those values precisely match
# the 4-bit condition code used in the conditional branches in the VAX instruction
# set.  The rest are used: to indicate (in template mode) that the actualy
# condition code should be taken from the VAX instruction, to indicate a microcode
# jump that is always taken, and to indicate microcode calls and returns (both
# unconditional).  The <cc> ("fill in from VAX instruction) code is illegal in
# expanded mode.
#
#
# Flags
# -----
# There are two flag sets, one for the microcode and one for the VAX architected
# state.  All µops that read/write/modify the flags always indicate which flag
# set they operate on.
#
#
# Width
# -----
# Some µops have an associated width.  It has two functions: for the address
# generation µops, it used to generate the right addresses (by determining how
# much to inc/dec/multiply by).  For the load/store/ALU instructions it determines
# how wide the write to memory or the register bank should be AND it determines
# which set of ALU flag outputs should be used (after 8/16/32 bits).
#
# Address generation µops are allowed to use '<width>' in template mode.
#
# This is usually the operand width taken from the instruction tables
# (determined by the VAX instruction).
#
# The valid widths in template form are 8/16/32/<width>, where the last value
# means "use the operand's or instruction's width".
#
# Some of the ALU VAX instructions can use the instruction's width to share
# microcode between several VAX instructions.
#
# The valid widths in expanded form are 8/16/32/64 -- and later 128.
# 64 (and 128) are only allowed in address generation µops.
#
# The expanded form of ALU ops are only allowed to use 8/16/32.
#
# The width conversion µops and the shift + "cheat" µops have variying implicit
# widths.
#
# 'imm' µop has an implicit width of 32 bits
# 'readpc' µop has an implicit width of 32 bits
# 'mfpr'/'mtpr' µops have an implicit width of 32 bits
#
#
# Operand order
# -------------
# The operand order is swapped compared to VAX instructions.
# The µops use a "natural" operand order.  VAX instructions use a different
# order so 2-operand versions of 3-operand instructions can be accomodated (the
# write/modify operands always have to come last).
#
# VAX examples:
#   SUB2    x, y          ; y - x  --> y
#   SUB3    x, y, z       ; y - x  --> z
#   DIV2    x, y          ; y / x  --> y
#   DIV3    x, y, z       ; y / x  --> z
#   ASHL    x, y, z       ; y << x --> z
#   ROTL    x, y          ; y << x --> y
#   BIC2    x, y          ; y & ~x --> y
#   BIC3    x, y, z       ; y & ~x --> z
#   CMP     x, y          ; x op y            <---- NOT implemented as y-x!
#
# µop examples:
#   sub     s1, s2, dst   ; s1 - s2  --> dst
#   div     s1, s2, dst   ; s1 / s2  --> dst
#   ashl    s1, s2, dst   ; s1 << s2 --> dst
#   rotl    s1, s2, dst   ; s1 << s2 --> dst
#
#
# Special-casing of bcc/exc
# -------------------------
# bcc/exc are the same µop.  The only difference is that exc has the high bit of
# the µtarget set.
#
#
#
# Flag modification
# -----------------
# There are not as many different ways of setting flags as I feared when I first
# looked at it in the summer of 2017.  Most of the differences seen with VAX
# instructions are due to 1) combinations of µops used and 2) different (usually
# more human-friendly) interpretations of the same flag setting rules.
#
# This is how each flag can be changed:
#
#     N   Z  V   C
#    --------------
#     m   z  v   c	m = MSB, z = 1 if zero, Z, v = overflow, c = carry/borrow
#     <   Z  0   0      op1 <  op2 (signed)
#     -   =  -   <u     op1 <u op2 (unsigned), op1 == op2
#         -      -
#
# Z = 1 if Z flag already set AND this result is 0 (used for MOVQ)
#
# These are the 7 (seven) different flag modes:
#
#     ----   used with all non-ALU instructions
#     mz00   sign/zero ext, emul
#     mzv0   trunc,  mul/div/ediv/ashl/ashq	-- iov (+ivdz for div/ediv)
#     mzvc   add/sub/adc/sbb			-- iov
#     <=0<   cmp
#     mz0-   mov/and/bic/bis/xor/rotl
#     -Z0-   movx (MOVQ, MOVO)
#
#
# Exceptions/interrupts
# ---------------------
# ... internal processor register, software interrupts...
#
# reserved operand exception, can be raised by decoder and by microcode
# for bitfield addressing modes (and microcode for the exe phase of certain
# instructions).
#
# These exceptions are raised implicitly by certain µops
# --------------------
# iov	integer overflow
# ivdz	integer divide by zero
#
# This exception is raised explicitly by the microcode that handles bitfield
# addressing modes (in pre/post phases, not in exe phase) and by microcode that
# implements the exe phase of certain VAX instructions (BICPSW, BISPSW, inter-
# locked queue instructions with unaligned addresses, CALLG/CALLS/RET, ...)
# --------------------
# rsv	reserved operand
#
# These exceptions are not raised at all (yet)
# --------------------
# prv	privileged instruction
# sub	subscript rnage (INDEX instruction)
# fov	fp overflow
# fuv	fp underflow
# dov	decimal overflow
# ddvz	decimal div by zero
#
#
# Realistic microarchitecture
# ---------------------------
# The imagined microarchitecture is a classic 5-stage pipeline with slightly
# less than 32 physical registers.  The register bank has two read ports and
# one write port.  Partial width (8/16 bits) are allowed.
#
# The NZVC-flags (two sets of 4 bits) are conceptually located right next to
# the ALU.  The PSL (32-bit Program Status Long) is stored in the register
# bank and any reads are checked so the architected register values get merged
# into the value read from the registers.  The writes don't care about the
# lower 4 bits.
#
# There is no way to read the µ flags except through the use of bcc/exc.
#
# There is either a forwarding path or a pipeline interlock (automatic stalling)
# so the µops can freely write to a register in one instruction and read from it
# in the next.
#
# The load/store unit handles b/w/l reads and writes, even if unaligned.
# Automatically stalls in case of unaligned reads/writes that have to be split
# up into two or more bus accesses.
# Automatically stalls in case of cache misses (or in case we are waiting for
# a store to go to memory).  That is, if there is a cache.  We don't know and
# we don't care.
#
# Address translation is handled by TLB lookup in the load/store unit.  The
# load/store unit can generate a TLB miss microexception and an access violation
# exception and a (micro?) exception if the page to be read from/written to
# changes modify/access status.
#
# The load/store unit can generate bus error (or similar) in case a bus
# transaction fails.
#
# The ALU consists of a classic ADD/SUB/LOGIC ALU + a mul/div unit + a shifter +
# a width converter.  The ALU can generate divby0 and overflow exceptions.
#
# The mul/div and shifter units are multicycle with automatic stalling.
# (Potentially, a scoreboard could be used to cut down on the stalling.)
#
# ASHQ, EMUL, EDIV have either three 32-bit inputs or two 32-bit outputs or
# both.  That can't be implemented on this microarchitecture -- but it is easy
# to accomodate in the simulator so we cheat by having µops for each of them.
# At a later stage, uasm.pl could conceivably expand them to something that more
# realistic so the same source code can be used on many implementations.
#
# Microbranches (and calls/rets) cause stalls.  At some point that may be
# changed to a branch delay slot, which the µassembler can try to fill with
# useful work or with a nop.
#
#
# Transactions, state
# -------------------
# The architected registers (including r15/PC) and the architected flags
# need to be under transactional control.
# This is needed in order to correctly roll back register changes due to
# register autoinc/autodec addressing modes in case an exception occurs.
#
# It is also going to be needed in the future in order to handle mispredicted
# branches (when the microarchitecture gains a branch predictor).
#
# There are two µops to control the transactions: commit and rollback.
# Normally, instructions are automatically committed (when they complete without
# an exception).  In case of an exception, µbranches, and interrupts during
# block instructions, the state needs to be explicitly controlled.
#
# Transactions automatically begin when a new VAX instruction gets decoded.

# Microcode specification
# -----------------------
# <name> [<operand>[, <operand>[, <operand>]]]  [-- (width|flags|width flags)]
#
# <operand> ::= imm | s1 | [s1] | s2 | [s2] | dst | cc | utarget
#
# s1/s2/dst	a name/number from %reg
# imm		32-bit number or <imm> (internally, <imm> is marked as a value
#               not used as an actual immediate in the microcode)
#
# cc            a condition from %cc
# utarget       a µcode label

# misc
nop						# irrelevant unless there is a branch delay
stop						# stop the simulator
commit						# commit transaction (no exception)
rollback					# abort transaction (exception happened)

# load an immedite into a register
imm		imm, dst			# 32 bits

# all-purpose control transfer µop for microcode
bcc		cc, utarget	-- flags
#exc		cc, utarget	-- flags	# special-cased, same as bcc w/ bit set on utarget
#b		utarget				# special-cased, same as 'bcc always, utarget -- µ'

# the only control transfer µop for VAX code (writes the PC)
jmp		s1

# read realpc as it was when this µcode fragment was inserted into the pipeline
# (i.e., when it was expanded from template format)
#
# can be implemented as a read from a latch/register
# can be implemented by putting a copy of the PC at decode time into an
# immediate field in the µop.
readpc		dst				# 32 bits

# load/store (interlocked, untranslated) -- all 6 can raise exceptions
ld		[s1], dst	-- width
ldi		[s1], dst	-- width
ldu		[s1], dst	-- width
st		s1, [s2]	-- width
sti		s1, [s2]	-- width
stu		s1, [s2]	-- width

# address generation
++		s1, dst		-- width	# translates as U_INC
--		s1, dst		-- width	# translates as U_DEC
[]		s1, dst		-- width	# translates as U_INDEX


# Internal Processor Registers
# writes to some processor registers raise software interrupts
mfpr		s1, dst			# 32 bits, s1 register holds a preg number
mtpr		s1, dst			# 32 bits, dst register holds a preg number



#
# ALU ops begin here
#
# some µops raise exceptions (ovf, div0)

# move                                            NZVC  exc  notes
#                                                 ----  ---  ---------
mov		s1, dst		-- width flags  # mz0-  -
movx		s1, dst		-- width flags	# -Z0-  -    for MOVQ

# width                                           NZVC  exc  impl.width
#                                                 ----  ---  ----------
signbw		s1, dst		-- flags        # mz00	-    16 bits
signbl		s1, dst		-- flags        # mz00	-    32 bits
signwl		s1, dst		-- flags        # mz00	-    32 bits
zerobw		s1, dst		-- flags        # mz00	-    16 bits
zerobl		s1, dst		-- flags        # mz00	-    32 bits
zerowl		s1, dst		-- flags        # mz00	-    32 bits
truncwb		s1, dst		-- flags        # mzv0	iov   8 bits
trunclb		s1, dst		-- flags        # mzv0	iov   8 bits
trunclw		s1, dst		-- flags        # mzv0	iov  16 bits

# arith                                           NZVC  exc  notes
#                                                 ----  ---  ----------
add		s1, s2, dst	-- width flags  # mzvc	iov
sub		s1, s2, dst	-- width flags  # mzvc	iov
adc		s1, s2, dst	-- width flags  # mzvc	iov
sbb		s1, s2, dst	-- width flags  # mzvc	iov
cmp		s1, s2		-- width flags  # <=0<  -

# logic                                           NZVC  exc
#                                                 ----  ---
and		s1, s2, dst	-- width flags  # mz0-  -
bic		s1, s2, dst	-- width flags  # mz0-  -
bis		s1, s2, dst	-- width flags  # mz0-  -
xor		s1, s2, dst	-- width flags	# mz0-  -

# shift                                           NZVC  exc        notes
#                                                 ----  --------   -----
ashl		s1, s2, dst	-- flags	# mzv0  iov
ashq		s1, s2, dst	-- flags	# mzv0  iov        cheat!
rotl		s1, s2, dst	-- flags	# mz0-  -

# mul/div                                         NZVC  exc        notes
#                                                 ----  --------   -----
mul		s1, s2, dst	-- width flags  # mzv0	iov
div		s1, s2, dst	-- width flags  # mzv0	iov,ivdz
emul						# mz00	-          cheat!
ediv						# mzv0	iov,ivdz   cheat!


