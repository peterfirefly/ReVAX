2018-03-19
-------------------------------------------------------------------------------

See doc/readme.txt for more (but less organized) information.


About ReVAX
---
This is an (incomplete!!!) VAX simulator + assorted tools.

The major tools are an assembler, a disassembler, and a tool that can read
Files-11 (ODS-2) filesystems, which is what the VMS operating system for the
VAX uses.

The simulated VAX is mostly like a CVAX without the math chip.



Status
---
Nothing really quite works yet.



License/copyright
---
The code I've written is licensed under GPLv2.  There's other code and text (and
some VAX binaries) written by others and not under GPLv2.



Structure
---
The simulator implements a simple data path, controlled by relatively realistic
"microcode".  The microcode is written in a text file and then "assembled" to
C code that declares a big, initialized array.  Each VAX instruction is implemented
by stitching together several (sometimes many) fragments of microcode.  The
decoder does that by using a set of tables, most of which are machine generated.

The set of allowed microops is defined in a text file that the microcode assembler
reads.  As a byproduct, the assembler generates tables that are used by the
microcode disassembler (the simulator has a microcode disassembler for debugging
purposes).

A particularly complex aspect of the VAX architecture is the overly flexible
operand mechanism.  All addressing modes are specified in a text file and then
converted to C code that implements an assembly language encoder, a disassembler,
a validator, and a dissector for the simulator.

Pretty much everything that can be machine generated -- *is* machine generated.
Pretty much everything that can be table-driven -- *is* table-driven.



Testing
---
Many modules can be tested alone through a set of built-in testcases or through
directed fuzzing with American Fuzzy Lop.  They generally output a corpus of
tests based on the built-in testcases.

There is also (incomplete) support for coverage testing.



Assorted make targets:
---

 'make help'
 'make vars'
 'make all'

 'make asm', 'make dis', 'make sim'
 'make tests', 'make run-tests'
 'make afl-tests', 'make af-run'

 'make stats'



Most important directories:
---

doc/		Docs.

src/		All the active source code, including code generators and their
		input files.

misc/		Small experiments and retired code.


