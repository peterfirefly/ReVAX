# Copyright 2018  Peter Lund <firefly@vax64.dk>
#
# Licensed under GPL v2.

###

CC=gcc
CFLAGS=-O2 -W -Wall -std=c99

COV=-fprofile-arcs -ftest-coverage
DBG=-g
SAN-CLANG=-fsanitize=undefined,unsigned-integer-overflow
SAN-GCC=-fsanitize=undefined,leak

CLANG=clang-5.0
GCC=gcc

CPP = cpp	# may need to override on OS X, where 'cpp' is a clang program

# avoid Dropbox folders, possibly also SSD and rotating rust (depending on
# cache settings for the file system).
AFL_OUT=~/tmp/

###

# version info macros

VERSION=v0.1
GITHASH=$(shell cat .git/refs/heads/master)
PLATFORM=$(shell . /etc/lsb-release; echo $$DISTRIB_DESCRIPTION)
NOW=$(shell date --utc '+%Y-%m-%d %H:%M:%S') UTC
REVAXURL=https://github.com/peterfirefly/ReVAX
CCVER=$(shell $(CC) --version | head -n1)

###

DEF=-DVERSION="\"$(VERSION)\"" -DGITHASH="\"$(GITHASH)\"" -DNOW="\"$(NOW)\"" -DPLATFORM="\"$(PLATFORM)\"" \
    -DCCVER="\"$(CCVER)\"" -DREVAXURL="\"$(REVAXURL)\""

###


# macro to help with C dependencies
#
# This:
#   $(call DEP,asm,src/asm.c)
#
# expands to something like:
#   asm:  src/asm.c src/shared.h src/parse.h vax-instr.h src/fp.h src/big-int.h \
#   src/op-support.h op-asm.h op-val.h
#
# The macro asks the C preprocessor) to go through its second argument and
# check which (non-system) header files get #include'd.
#
# It is not superfast because it actually invokes the C preprocessor every time
# it is called but it is simpler than caching the dependency list in a file.
#
# If a file #include's a generated file that doesn't currently exist, the C
# preprocessor will guess that it should exist in the current directory.
# Adding '-Isrc' to cpp's command line doesn't help.
#
# There is a workaround for that issue further down in this makefile where each
# of the incorrect xxx.h filenames is 1) marked PHONY and 2) depends on the
# real filename in src/.

DEP=$(subst \, ,$(shell $(CPP) -Isrc -MM $(2) -MG -MT $(1)))

###

help:
	@echo 'make all | asm|dis|sim | cov |snips|tables|stats | clean|distclean'
	@echo ''
	@echo '  asm          assembler (revax-asm)'
	@echo '  dis          disassembler (revax-dis)'
	@echo '  sim          simulator (revax-sim)'
	@echo ''
	@echo '  *.cov        compile for coverage analysis'
	@echo '  *.dbg        compile with debug symbols'
	@echo '  *.san.clang  compile with clang undefined behaviour sanitizer'
	@echo '  *.san.gcc    compile with gcc undefined behaviour sanitizer'
	@echo ''
	@echo '  cov          create HTML report of coverage after a run'
	@echo ''
	@echo '  snips        small VAX test programs (runtime/test*.bin)'
	@echo '  tables       generated files with tables that describe VAX instructions'
	@echo '  ops          generated files that handle operands for asm/dis/sim'
	@echo '  stats        how much code is there?'
	@echo ''
	@echo '  test-*       various test programs'
	@echo '  afl-test-*   the same test programs built for American Fuzzy Lop'
	@echo '  run-test-*   run the test programs'
	@echo '  afl-run-*    run the afl-test-* versions under American Fuzzy Lop'
	@echo ''
	@echo '  misc         build misc code'
	@echo ''
	@echo '  fixme        check for FIXMEs'
	@echo '  spaces       check for trailing spaces'
	@echo '  reserved     check for reserved identifiers'
	@echo '  vars         print out makefile variables'
	@echo ''
	@echo 'see doc/readme.txt for more information.'
	@echo ''

.PHONY:	help all  tables ops snips  stats  spaces vars  clean distclean

all:	asm dis sim  snips

asm:	revax-asm
dis:	revax-dis
sim:	revax-sim
uop:	revax-uop

###

vars:
	@echo 'CC=$(CC)'
	@echo 'CFLAGS=$(CFLAGS)'
	@echo ''
	@echo 'CLANG=$(CLANG)'
	@echo 'GCC=$(GCC)'
	@echo ''
	@echo 'AFL_OUT=$(AFL_OUT)'
	@echo ''
	@echo 'AFL_PATH=$(AFL_PATH)'
	@echo 'PATH=$(PATH)'
	@echo ''
	@echo 'VERSION=$(VERSION)'
	@echo 'GITHASH=$(GITHASH)'
	@echo 'PLATFORM=$(PLATFORM)'
	@echo 'NOW=$(NOW)'
	@echo 'CCVER=$(CCVER)'
	@echo ''
	@echo 'DEF=$(DEF)'

###

spaces:
	misc/trailing-spaces.pl src/*.[ch] src/*.pl src/*.vu src/*.spec	\
				misc/*.[ch] misc/*.pl misc/*.asm	\
				vax-test/*.c vax-test/*.asm		\
				vax-bin/*.desc				\
				ods2/ods2-firefly.c			\
				tables/*.txt tables/*.snip		\
				doc/*.txt				\
				Makefile *.txt *.sh

###

fixme:
	grep --color=yes FIXME Makefile *.sh */*.[ch] */*.pl */*.vu */*.spec doc/*.txt

###

reserved:	tables ops
	@ctags misc/*.[ch] src/*.[ch] ods2/ods2-firefly.c
	@misc/check-reserved.pl tags

###

tstdep:
	@echo $(call DEP,asm,src/asm.c)

###

# These three targets are the *actual* end product we care about

#asm:	src/asm.c src/shared.h src/vax-instr.h   src/op-asm.h src/op-val.h
$(call DEP,revax-asm,src/asm.c)
	$(CC) $(CFLAGS) $(DEF) $< -Isrc -lmpfr -lgmp -o $@

#dis:	src/dis.c src/shared.h src/vax-instr.h   src/op-dis.h src/op-val.h
$(call DEP,revax-dis,src/dis.c)
	$(CC) $(CFLAGS) $(DEF) $< -Isrc -lmpfr -lgmp -o $@

$(call DEP,revax-uop,src/uop.c)
	$(CC) $(CFLAGS) $(DEF) $< -Isrc -lmpfr -lgmp -o $@

#sim:	src/sim.c src/shared.h src/fragments.h src/dis-uop.h	\
#	src/vax-instr.h src/vax-ucode.h src/vax-fraglists.h	\
#	src/op-sim.h src/op-val.h
$(call DEP,revax-sim,src/sim.c)
	$(CC) $(CFLAGS) $(DEF) $< -Isrc -o $@

###

# generates table with fragment group lists per opcode
#
# needs to be in C because it needs to access µcode labels (#define'd in
# src/vax-ucode.h included by src/fragments.h) and the list fragment groups in
# src/fragments.h).
#src/fragtable:	src/fragtable.c src/fragments.h	\
#	 src/vax-instr.h src/vax-ucode.h
$(call DEP,src/fragtable,src/fragtable.c)
	$(CC) $(CFLAGS) $< -o $@

###

# Compiling a program with -fprofile-arcs etc makes gcc put in a bit of extra
# code that counts every time a basic block is entered or exited.  This data
# is written to a file (.gcda) when the program is run.  The compiler generates
# a description of those basic blocks (.gcno), which can be used by a tool to
# work backwards from the .gcda file to annotated source code.  'gcov' is the
# standard tool for that, but it is not so nice to work with.  'lcov' is a
# wrapper that is nicer (somehow).  'genhtml' takes the output from 'lcov' and
# outputs HTML.
#
# 'lcov' and 'genhtml' are both part of the lcov project.
#
# https://gcc.gnu.org/onlinedocs/gcc/Invoking-Gcov.html
# https://stackoverflow.com/questions/19582660/how-do-the-code-coverage-options-of-gcc-work
# http://ltp.sourceforge.net/coverage/lcov/readme.php


define COV_TEMPLATE =
.PHONY:	$(1).cov
$(1).cov:
	rm -rf $(1) $(1).gcda $(1).gcno cov.info
	$(MAKE) CFLAGS='$(CFLAGS) $(COV)' $(1)
endef

$(eval $(call COV_TEMPLATE, asm))
$(eval $(call COV_TEMPLATE, dis))
$(eval $(call COV_TEMPLATE, sim))



.PHONY:	cov
cov:
	# lcov gathers *all* coverage information -- asm, dis, sim
	lcov --capture --directory . --output-file cov.info
	# turn it into a nice HTML report
	genhtml cov.info --output-directory cov
	@echo ''
	@echo 'Please open the coverage report with a command like:'
	@echo '  gnome-open cov/index.html'

###

# Debug version

define DBG_TEMPLATE =
.PHONY:	$(1).dbg
$(1).dbg:
	rm -f $(1)
	$(MAKE) CFLAGS='$(CFLAGS) $(DBG)' $(1)
endef

$(eval $(call DBG_TEMPLATE, asm))
$(eval $(call DBG_TEMPLATE, dis))
$(eval $(call DBG_TEMPLATE, sim))


###

# Sanitizers
#
# https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html

define SAN_TEMPLATE =
.PHONY:	$(1).san.clang
$(1).san.clang:
	rm -f $(1)
	$(MAKE) CC=$(CLANG) CFLAGS='$(CFLAGS) $(SAN-CLANG)' $(1)


.PHONY:	$(1).san.gcc
$(1).san.gcc:
	rm -f $(1)
	$(MAKE) CC=$(GCC) CFLAGS='$(CFLAGS) $(SAN-GCC)' $(1)
endef

$(eval $(call SAN_TEMPLATE, asm))
$(eval $(call SAN_TEMPLATE, dis))
$(eval $(call SAN_TEMPLATE, sim))


###

.PHONY:	misc headers
misc:	headers cmp-sub fp-ieee sizes-op.o

# most header files should compile as standalone compilation units
headers:	src/vax-ucode.h src/vax-instr.h
	$(CC) -c $(CFLAGS) -Isrc src/macros.h
	$(CC) -c $(CFLAGS) -Isrc src/strret.h
	$(CC) -c $(CFLAGS) -Isrc src/string-utils.h
	$(CC) -c $(CFLAGS) -Isrc src/reflow.h
	$(CC) -c $(CFLAGS) -Isrc src/html.h
	$(CC) -c $(CFLAGS) -Isrc src/parse.h
	$(CC) -c $(CFLAGS) -Isrc src/big-int.h
	$(CC) -c $(CFLAGS) -Isrc src/fp.h
	$(CC) -c $(CFLAGS) -Isrc src/dis-uop.h
	$(CC) -c $(CFLAGS) -Isrc src/op-support.h
	$(CC) -c $(CFLAGS) -Isrc src/op-asm-support.h
	$(CC) -c $(CFLAGS) -Isrc src/op-dis-support.h
	$(CC) -c $(CFLAGS) -Isrc src/op-sim-support.h
	$(CC) -c $(CFLAGS) -Isrc src/op-val-support.h


cmp-sub: misc/cmp-sub.c
	$(CC) $(CFLAGS) -Isrc $< -o $@

fp-ieee: misc/fp-ieee.c
	$(CC) $(CFLAGS) -Isrc $< -o $@ -lmpfr -lgmp

$(call DEP, sizes-op.o,misc/sizes-op.c)
	$(CC) -c $(CFLAGS) -Isrc $< -o $@
	size $@
	size -A $@

###

# Test code
#
# FIXME:
#   Coverage tests...?
#   misc/test-fp.c --> src/fp.h
#   test-op.c      --> src/op-support.h
#   test-alu.c     --> src/alu.c
#   test-analyze.c --> src/analyze.c

.PHONY:	tests
tests:	test-big-int test-fp test-op test-alu test-analyze test-dis-uop


# ordinary built-in tests, compile with -fsanitize=undefined,leak

#test-big-int:	misc/test-big-int.c src/big-int.h src/shared.h
$(call DEP,test-big-int,misc/test-big-int.c)
	$(CC) $(CFLAGS) -g -fsanitize=undefined,leak $< -Isrc -lmpfr -lgmp -o $@

#test-fp:	misc/test-fp.c src/big-int.h src/shared.h
$(call DEP,test-fp,misc/test-fp.c)
	$(CC) $(CFLAGS) -g -fsanitize=undefined,leak $< -Isrc -lmpfr -lgmp -o $@

#test-op:	misc/test-op.c src/big-int.h src/shared.h	\
#		src/op-support.h src/op-asm.h src/op-dis.h src/op-sim.h src/op-val.h
$(call DEP,test-op,misc/test-op.c)
	$(CC) $(CFLAGS) -g -fsanitize=undefined,leak $< -Isrc -lmpfr -lgmp -o $@

#test-alu:	misc/test-alu.c src/shared.h
$(call DEP,test-alu,misc/test-alu.c)
	$(CC) $(CFLAGS) -g -fsanitize=undefined,leak $< -Isrc -o $@

#test-analyze:	misc/test-analyze.c src/shared.h		\
#		src/vax-instr.h src/vax-ucode.h			\
#		src/op-support.h src/op-sim.h src/op-val.h
$(call DEP,test-analyze,misc/test-analyze.c)
	$(CC) $(CFLAGS) -g -fsanitize=undefined,leak $< -Isrc -o $@

#test-dis-uop:	misc/test-dis-uop.c src/dis-uop.h src/shared.h	\
#		src/vax-instr.h src/vax-ucode.h
$(call DEP,test-dis-uop,misc/test-dis-uop.c)
	$(CC) $(CFLAGS) -g -fsanitize=undefined,leak $< -Isrc -o $@



.PHONY:	tests-nosan
tests-nosan:	test-big-int-nosan test-fp-nosan test-op-nosan test-alu-nosan test-analyze-nosan test-dis-uop-nosan


# built-in tests/timing, compiled without sanitizers or assertions


#test-big-int:	misc/test-big-int.c src/big-int.h src/shared.h
$(call DEP,test-big-int-nosan,misc/test-big-int.c)
	$(CC) $(CFLAGS) -DNDEBUG -g $< -Isrc -lmpfr -lgmp -o $@

#test-fp:	misc/test-fp.c src/big-int.h src/shared.h
$(call DEP,test-fp-nosan,misc/test-fp.c)
	$(CC) $(CFLAGS) -DNDEBUG -g $< -Isrc -lmpfr -lgmp -o $@

#test-op:	misc/test-op.c src/big-int.h src/shared.h	\
#		src/op-support.h src/op-asm.h src/op-dis.h src/op-sim.h src/op-val.h
$(call DEP,test-op-nosan,misc/test-op.c)
	$(CC) $(CFLAGS) -DNDEBUG -g $< -Isrc -lmpfr -lgmp -o $@

#test-alu:	misc/test-alu.c src/shared.h
$(call DEP,test-alu-nosan,misc/test-alu.c)
	$(CC) $(CFLAGS) -DNDEBUG -g $< -Isrc -o $@

#test-analyze:	misc/test-analyze.c src/shared.h		\
#		src/vax-instr.h src/vax-ucode.h			\
#		src/op-support.h src/op-sim.h src/op-val.h
$(call DEP,test-analyze-nosan,misc/test-analyze.c)
	$(CC) $(CFLAGS) -DNDEBUG -g $< -Isrc -o $@

#test-dis-uop:	misc/test-dis-uop.c src/dis-uop.h src/shared.h	\
#		src/vax-instr.h src/vax-ucode.h
$(call DEP,test-dis-uop-nosan,misc/test-dis-uop.c)
	$(CC) $(CFLAGS) -DNDEBUG -g $< -Isrc -o $@



.PHONY:	run-tests
run-tests:	run-big-int run-fp run-op run-alu run-analyze


# --built-in
run-big-int:	test-big-int
	./test-big-int --built-in

run-fp:		test-fp
	./test-fp --built-in

run-op:		test-op
	./test-op --built-in

run-alu:	test-alu
	./test-alu --built-in

run-analyze:	test-analyze
	./test-analyze --built-in



# AFL tests, compile with -fsanitize=undefined and afl-clang-fast (or afl-clang
# or afl-gcc).
#
# afl-clang-fast makes the tests run faster.
# AFL_EXIT_WHEN_DONE=1 makes the fuzz test stop when it thinks it is done
# AFL_NO_ARITH=1 makes fuzzing faster for programs that don't do any arithmetic
#  (some of the bit twiddling variations are skipped)
# AFL_HARDEN=1 makes afl-clang-fast add compiler flags that makes the machine
# code check for more errors at a 5-10% performance cost.
#
# FIXME check for errors afterwards!
# FIXME afl-cmin, afl-tmin to make the test corpus smaller/better
# FIXME afl-showmap to check that the test corpus does what it's supposed to
# FIXME use dictionary for asm tests?

# are the afl binaries in $PATH?
.PHONY:	check-afl
check-afl:
	@command -v afl-clang-fast 2>&1 > /dev/null || { echo "afl-clang-fast not found." >&2; exit 1; }
	@command -v afl-clang      2>&1 > /dev/null || { echo "afl-clang not found."      >&2; exit 1; }
	@command -v afl-gcc        2>&1 > /dev/null || { echo "afl-gcc not found."        >&2; exit 1; }
	@command -v afl-fuzz       2>&1 > /dev/null || { echo "afl-fuzz not found."       >&2; exit 1; }

.PHONY:	afl-tests
afl-tests:	afl-test-big-int afl-test-fp afl-test-op #afl-test-alu afl-test-analyze


# ordinary built-in tests, compile with -fsanitize=undefined
#afl-test-big-int:	misc/test-big-int.c src/big-int.h src/shared.h check-afl
$(call DEP,afl-test-big-int,misc/test-big-int.c) check-afl
	afl-clang-fast $(CFLAGS) -fsanitize=undefined,leak -Isrc $< -lmpfr -lgmp -o $@

#afl-test-fp:	misc/test-fp.c src/big-int.h src/shared.h check-afl
$(call DEP,afl-test-fp,misc/test-fp.c) check-afl
	afl-clang-fast $(CFLAGS) -fsanitize=undefined,leak -Isrc $< -lmpfr -lgmp -o $@

#afl-test-op:	misc/test-op.c src/big-int.h src/shared.h		\
#		src/vax-ucode.h src/op-support.h src/op-asm.h src/op-dis.h src/op-sim.h src/op-val.h \
#		check-afl
$(call DEP,afl-test-op,misc/test-op.c) check-afl
	afl-clang-fast $(CFLAGS) -fsanitize=undefined,leak -Isrc $< -lmpfr -lgmp -o $@




.PHONY: afl-run
afl-run:	afl-run-big-int afl-run-fp

# these macros just make the command lines shorter -- that is their only purpose
AFL_FUZZ=AFL_EXIT_WHEN_DONE=1 AFL_NO_AFFINITY=1 afl-fuzz
AFL_BIG=./afl-test-big-int

# pseudo-random tests (passed via stdin), many kinds
AFL_BIG_OUT=$(AFL_OUT)/big-int-out
afl-run-big-int:	afl-test-big-int check-afl
	./afl-test-big-int --output-add
	./afl-test-big-int --output-neg
	./afl-test-big-int --output-shl
	./afl-test-big-int --output-clz
	./afl-test-big-int --output-shortmul
	#
	mkdir -p $(AFL_BIG_OUT)/add
	mkdir -p $(AFL_BIG_OUT)/neg
	mkdir -p $(AFL_BIG_OUT)/shl
	mkdir -p $(AFL_BIG_OUT)/clz
	mkdir -p $(AFL_BIG_OUT)/shortmul
	#
	$(AFL_FUZZ) -T 'big-int add'      -i afl/big-int/add      -o $(AFL_BIG_OUT)/add      -- $(AFL_BIG) --test-add
	$(AFL_FUZZ) -T 'big-int neg'      -i afl/big-int/neg      -o $(AFL_BIG_OUT)/neg      -- $(AFL_BIG) --test-neg
	$(AFL_FUZZ) -T 'big-int shl'      -i afl/big-int/shl      -o $(AFL_BIG_OUT)/shl      -- $(AFL_BIG) --test-shl
	$(AFL_FUZZ) -T 'big-int clz'      -i afl/big-int/clz      -o $(AFL_BIG_OUT)/clz      -- $(AFL_BIG) --test-clz
	$(AFL_FUZZ) -T 'big-int shortmul' -i afl/big-int/shortmul -o $(AFL_BIG_OUT)/shortmul -- $(AFL_BIG) --test-shortmul

afl-run-fp:		afl-test-fp check-afl
	./afl-test-fp --output-from-str
	./afl-test-fp --output-to-str
	#
	mkdir -p afl/from-str-out
	mkdir -p afl/to-str-out
	#
	$(AFL_FUZZ) -T 'fp from-str' -i afl/from-str -o afl/from-str-out -- ./afl-test-fp --test-from-str
	$(AFL_FUZZ) -T 'fp to-str'   -i afl/to-str   -o afl/to-str-out   -- ./afl-test-fp --test-to-str

afl-run-op:		afl-test-op check-afl
	./afl-test-op --output
	#
	mkdir -p afl/
	#
	$(AFL_FUZZ) -T 'op-asm' -i afl/op-asm    -o afl/op-asm-out -- ./afl-test-op --test-asm
	$(AFL_FUZZ) -i 'op-dis' -i afl/op-decode -o afl/op-dis-out -- ./afl-test-op --test-dis
	$(AFL_FUZZ) -i 'op-sim' -i afl/op-decode -o afl/op-sim-out -- ./afl-test-op --test-sim
	$(AFL_FUZZ) -i 'op-val' -i afl/op-decode -o afl/op-val-out -- ./afl-test-op --test-val


test-clean:
	@rm -f     test-big-int     test-fp     test-op     test-alu     test-analyze     test-dis-uop
	@rm -f afl-test-big-int afl-test-fp afl-test-op afl-test-alu afl-test-analyze afl-test-dis-uop
	@rm -rf afl
	@rm -f     test-big-int-nosan test-fp-nosan     test-op-nosan     test-alu-nosan     \
	           test-analyze-nosan test-dis-uop-nosan

###

# DEP macro doesn't know that missing generated files are supposed to live in
# the src/ directory.  Make some dummy dependencies to work around that.

.PHONY:	vax-instr.h vax-ucode.h vax-fraglists.h
vax-instr.h:	src/vax-instr.h
vax-ucode.h:	src/vax-ucode.h
vax-fraglists.h:src/vax-fraglists.h

.PHONY:	op-asm.h op-dis.h op-sim.h op-val.h
op-asm.h:	src/op-asm.h
op-dis.h:	src/op-dis.h
op-sim.h:	src/op-sim.h
op-val.h:	src/op-val.h

###

# Auto generated files that encode/decode operands for asm/dis/sim
#
# The output is based on src/operands.spec + some hardcoded information.

ops:	src/op-asm.h src/op-dis.h src/op-sim.h src/op-val.h

src/op-asm.h:	src/operands.spec src/operands.pl
	src/operands.pl --asm < $< > $@

src/op-dis.h:	src/operands.spec src/operands.pl
	src/operands.pl --dis < $< > $@

src/op-sim.h:	src/operands.spec src/operands.pl src/vax-ucode.h
	src/operands.pl --sim < $< > $@

src/op-val.h:	src/operands.spec src/operands.pl
	src/operands.pl --val < $< > $@

###

# Auto generated files that describe VAX instructions (names, opcodes, addressing
# modes/operand sizes, register names, processor register names, condition
# encoding for conditional branches, µop names/numbers, ...).
#
# The output is partly based on tables/instr.snip (a raw extract from
# dec-src/cvaxspec.txt with a few typos fixed), partly based on src/ucode.vu,
# and partly based on hardcoded information in the generators.

tables: src/vax-instr.h src/vax-instr.pl src/vax-ucode.h src/vax-fraglists.h


src/vax-instr.h:	tables/instr.snip src/instr.pl
	src/instr.pl --c    < $< > $@

src/vax-instr.pl:	tables/instr.snip src/instr.pl
	src/instr.pl --perl < $< > $@

src/vax-ucode.h:	src/ucode.vu src/uasm.pl src/vax-instr.pl
	src/uasm.pl < $< > $@

src/vax-fraglists.h:	src/fragtable
	src/fragtable > $@

###

# vax-linux-gcc, vax-linux-objcopy, vax-linux-objdump are tools I compiled
# locally to handle VAX binaries.
#
# My install lives in ~/vax-tools/ and consists of:
#   gcc 4.9.0
#   binutils 2.28

# small snippets of raw code for testing simulator and disassembler
snips:	check-vax-tools runtime/test1.bin runtime/test2.bin runtime/test3.bin



# are the afl binaries in $PATH?
.PHONY:	check-vax-tools
check-vax-tools:
	@command -v vax-linux-gcc     2>&1 > /dev/null || { echo "vax-linux-gcc not found."     >&2; exit 1; }
	@command -v vax-linux-objcopy 2>&1 > /dev/null || { echo "vax-linux-objcopy not found." >&2; exit 1; }
	@command -v vax-linux-objdump 2>&1 > /dev/null || { echo "vax-linux-objdump not found." >&2; exit 1; }


# the tests are based on small C programs
runtime/%.bin: runtime/%.c
	vax-linux-gcc -std=c99 -c -O0 -W -Wall $< -o $(basename $<).o
	vax-linux-objcopy --output-target binary $(basename $<).o $@
#	vax-linux-objdump -b binary -m vax -D $@


###

# How much code is there and of what kind?

stats:	tables ops
	@echo 'Total code size (without test code and experiments)'
	@echo '---------------------------------------------------'
	@wc src/asm.c src/dis.c src/sim.c src/uop.c			\
	    \
	    src/macros.h src/strret.h src/string-utils.h src/html.h src/reflow.h \
	    src/parse.h src/big-int.h src/fp.h				\
	    src/fragments.h						\
	    src/dis-uop.h						\
	    src/op-support.h src/op-lit6.h				\
	    src/op-asm-support.h src/op-dis-support.h src/op-sim-support.h src/op-val-support.h	\
	    \
	    src/vax-instr.h src/vax-ucode.h				\
	    src/vax-fraglists.h						\
	    src/op-asm.h src/op-dis.h src/op-sim.h src/op-val.h		\
	    \
	    src/fragtable.c						\
	    src/vax-instr.pl 						\
	    src/instr.pl src/uasm.pl src/operands.pl		 	\
	    src/ucode.vu src/uops.spec src/operands.spec | misc/totals.pl
	@echo ''
	@echo ''
	@echo 'Generated code'
	@echo '--------------'
	@wc src/vax-instr.pl src/vax-instr.h src/vax-ucode.h src/vax-fraglists.h \
	    src/op-asm.h src/op-dis.h src/op-sim.h src/op-val.h | misc/totals.pl
	@echo ''
	@echo ''
	@echo 'Hand-written code'
	@echo '-----------------'
	@wc src/asm.c src/dis.c src/sim.c src/uop.c			\
	    src/macros.h src/strret.h src/string-utils.h src/html.h src/reflow.h \
	    src/parse.h src/big-int.h src/fp.h				\
	    src/fragments.h						\
	    src/dis-uop.h						\
	    src/op-support.h src/op-lit6.h				\
	    src/op-asm-support.h src/op-dis-support.h src/op-sim-support.h src/op-val-support.h	\
	    src/fragtable.c src/instr.pl src/operands.pl src/uasm.pl	\
	    src/ucode.vu src/uops.spec src/operands.spec | misc/totals.pl
	@echo ''
	@echo ''
	@echo 'VAX simulator, C code'
	@echo '---------------------'
	@wc src/sim.c							\
	    src/macros.h src/strret.h					\
	    src/fragments.h						\
	    src/dis-uop.h						\
	    src/op-support.h src/op-lit6.h src/op-sim-support.h src/op-val-support.h \
	    src/vax-instr.h src/vax-ucode.h src/vax-fraglists.h		\
	    src/op-sim.h src/op-val.h | misc/totals.pl
	@echo ''
	@echo ''
	@echo 'VAX simulator, hand-written code'
	@echo '--------------------------------'
	@wc src/sim.c							\
	    src/macros.h src/strret.h					\
	    src/fragments.h						\
	    src/dis-uop.h						\
	    src/op-support.h src/op-lit6.h src/op-sim-support.h src/op-val-support.h \
	    src/ucode.vu src/uops.spec src/operands.spec | misc/totals.pl
	@echo ''
	@echo ''
	@echo 'Generators'
	@echo '----------'
	@wc  src/fragtable.c src/instr.pl src/operands.pl src/uasm.pl 	\
	  | misc/totals.pl
	@echo ''
	@echo ''
	@echo 'Test code'
	@echo '---------'
	@wc misc/test-big-int.c misc/test-fp.c misc/test-op.c misc/test-alu.c misc/test-analyze.c \
	    misc/test-dis-uop.c						\
	  | misc/totals.pl
	@echo ''
	@echo ''
	@echo 'Experiments'
	@echo '-----------'
	@wc misc/cmp-sub.c  misc/fp-ieee.c  misc/fp-vax.c  misc/idioms.c	\
	    misc/regalloc.c misc/sizes-op.c misc/structs.c misc/struct-test.c	\
	    misc/zeroext.c 							\
	    misc/vax.asm							\
	    misc/uasm-old.pl							\
	  | misc/totals.pl
	@echo ''
	@echo ''
	@echo 'Tools (not incl. generators), mostly written by others'
	@echo '------------------------------------------------------'
	@wc misc/totals.pl ods2/ods2-firefly.c				\
	  | misc/totals.pl
	@echo ''
	@echo ''
	@echo 'VAX test snippets'
	@echo '-----------------'
	@wc runtime/test*.c						\
	  | misc/totals.pl
	@echo ''
	@echo ''
	@echo 'CVAX microcode (written by DEC)'
	@echo '-------------------------------'
	@wc cvax-ucode/*.mic cvax-ucode/*.com				\
	  | misc/totals.pl
	@echo ''
	@echo ''
	@mkdir -p tmp
	@# use tmp dir to control which files gets counted, use a different
	@# name for the microcode so it counts as "asm".
	@cp src/asm.c src/dis.c src/sim.c src/uop.c			\
	    src/macros.h src/strret.h src/string-utils.h src/html.h src/reflow.h \
	    src/parse.h src/big-int.h src/fp.h				\
	    src/fragments.h						\
	    src/dis-uop.h						\
 	    src/op-support.h src/op-lit6.h				\
	    src/op-asm-support.h src/op-dis-support.h src/op-sim-support.h src/op-val-support.h	\
	    src/instr.pl src/uasm.pl src/operands.pl			\
	    src/fragtable.c						\
	    tmp
	# pretend µcode is assembler
	@cp src/ucode.vu      tmp/ucode.s
	# pretend that operand spec is shell
	@cp src/operands.spec tmp/operands.sh
	# pretend that uops spec is shell
	@cp src/uops.spec     tmp/uops.sh
	@sloccount tmp | head -n -6 | tail -n +7 | cat -s
	@rm -rf tmp

###

clean:	test-clean
	-@rm -f	\
	   *.o src/*.o misc/*.o ods2/*.o    			\
	   *.s src/*.s misc/*.s ods2/*.s			\
	   revax-asm revax-dis revax-sim revax-uop		\
	   a.out ods2-read cmp-sub fp-ieee			\
	   src/regalloc						\
	   src/fragtable					\
	   src/vax-instr.h src/vax-instr.pl src/vax-ucode.h	\
	   src/vax-fraglists.h					\
	   src/op-asm.h src/op-dis.h src/op-sim.h src/op-val.h	\
	   src/*.gch						\
	   vax-test/test*.o vax-test/test*.bin vax-test/test*.s	\
	   vax-test/*.raw vax-test/*.raw.asm			\
	   *.lst *.raw						\
	   *.gcno *.gcda *.info src/*.gcno src/*.gcda
	-@rm -rf cov/
	-@rm -rf vax-bin/*.disasm  vax-bin/*.lst  vax-bin/*.html
	-@rm -rf vax-test/*.disasm vax-test/*.lst vax-test/*.html
	-@rm -f  cachegrind.out.*
	-@rm -f  tags

distclean:	clean
	-@rm -f *~ doc/*~ src/*~ tables/*~ ods2/*~ vax-test/*~  vax-bin/*~ misc/*~


