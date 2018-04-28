#!/usr/bin/perl

# Copyright 2018  Peter Lund <firefly@vax64.dk>
#
# Licensed under GPL v2.

# Read tables/instr.snip from stdin and output tables describing the VAX
# instructions as C/Perl code.
#
# The tables describe the instruction names and the instruction operand lists.
# The original operand lists are taken from instr.snip but in some cases we
# override them to handle interlocked modify and bit fields.  This tool handles
# the override.
#
# Each operand type (rb, rq, wg, bb, ...) map to a fragment group, that is, a
# table of microcode fragments to execute in pre/post phases, depending on the
# operand class (immediate/register/memory).  Many operand types map to the
# same fragment group.
#
# This tool outputs a table that lists the fragment group for each operand of
# each instruction.
#
# The fragment groups themselves are described elsewhere.  FIXME
#
#    src/instr.pl --c      < tables/instr.snip > src/vax-instr.h
#    src/instr.pl --perl   < tables/instr.snip > src/vax-instr.pl
#
#    src/instr.pl --humans < tables/instr.snip

use strict;
use warnings;

use POSIX qw(strftime);


# opcode (integer) => mnemonic (string)
my %mne = ();

# some opcodes have more than one name
# list of (opcode, instrname) pairs
my @syn = ();

# operand-list for each opcode
# opcode (integer) => operand-list (string)
my %oplist = ();


# opcode instrname  operand-list     flags   exception-list
#
# operand-list and execption-list can be empty
# really long operand-list are continued on the next line -- they are indented
my $opcode=0;  # for handling operand-list continuations

sub read_snippet() {
while (<>) {
	if (/^\s+([0-9A-F]{2,4})\s+(\S+)\s+(\S.*\S|)\s+(\S \S \S \S)(?:\s*(\S.*\S)|)\s*$/) {
#		print "|$1|\n";
		$opcode = hex($1);
		if (($opcode >> 8) != 0x00) {
			# multi-byte opcode
			if ((($opcode & 0xFF) == 0xFD) && (($opcode >> 16) == 0)) {
				# proper two-byte opcode
				$opcode = 0x100 + ($opcode >> 8);
			} else {
				print "Wrong opcode: $1\n";
			}
		}

		my $instrname_str = $2;
		my $oplist_str = $3;
#		my $cc_str = $4;
		my $exp_str = $5;

		# check for synonyms -- XX{=YY}, XX{=YY=ZZ}
		# there are never more than two synonyms
		if ($instrname_str =~ /^(\w+)\{=(\w+)(?:=(\w+))?}$/) {
			$instrname_str = $1; # the primary instruction name
			my $syn1 = $2;
			my $syn2 = defined $3 ? $3 : "";

			# there are two kinds of synonyms, those that specify
			# a completely different mnemonic and those that specify
			# an alternate last letter.  The former are always longer
			# than a single letter.

			if (length $syn1 == 1) {
				# alternate last letter
				push @syn, [$opcode, substr($instrname_str, 0, -1) . $syn1]
			} else {
				# completely different name
				push @syn, [$opcode, $syn1]
			}

			if (length $syn2 == 0) {
				# no second synonym -- do nothing
			} elsif (length $syn2 == 1) {
				# alternate last letter
				push @syn, [$opcode, substr($instrname_str, 0, -1) . $syn2]
			} else {
				# completely different name
				# completely different name
				push @syn, [$opcode, $syn2]
			}
		}

		$mne{$opcode}    = $instrname_str;
		$oplist{$opcode} = $oplist_str;

	} elsif (/^\s{16}\s*(\S.*\S)\s*$/ && $1 !~ /Where/) {
		# continuation of the operand-list
#		print "    $1\n";

		$oplist{$opcode} = $oplist{$opcode} . $1;
	} else {
		# make it easy to print non-matching lines -- helps with
		# debugging.
#		print "> $_";
	}
}
}


# Split operand specs => lists

# ignore implicit ops -- PUSH, field instructions, BSB, CALL, RET, HALT, BPT, REI, ...

# split opspecs on '.'


# Output tabeller:
#  mne[512] => string
#  syn[] = [(op, instrname)]
#
#  opss[512] => string
#  stdops[] => string
#  ops[512] => stdops idx
#
#  excs[512] => string
#  stdexc[] => string
#  exc[512] => stdexc idx
#
# Figure out which instructions come in several lengths where autodetecting
# the length from the operands makes sense.  Make a table for that.


# post process operand lists
#
# - remove whitespace
# - split into operand descriptions (split on ',')
# - discard implicit operands (begin with '{')
# - split each operand into name and type (split on '.')
# - check that each type are two letters
# - check that the types split nicely into access and length
#   - access is one of r/w/m/a/v/b  (read/write/modify/address/bitfield/branch)
#   - length is one of b/w/l/q  (integer)  f/d/g (fp)
#     o (integer)  and h (fp) also exist but are not implemented in CVAX
#   - bw-list is an option for CASEB/W/L -- we treat that as an implicit
#     operand by removing it.
#
# figure out which operand lists are used (including no operands)

# opcode => cleaned up operand list
my %ops = ();

sub cleanup_oplists() {
	for (my $i=0; $i < 512; $i++) {
		if (not exists $oplist{$i}) {
			next;
		}

		my $s = $oplist{$i};

		my $spec = "";	# cleaned up operand list

		# remove all whitespce
		$s =~ s/\s//g;

		# remove implicit operands -- some of them contain ',' so let's get
		# rid of them before splitting the list
		$s =~ s/\{.*?\}//g;

		# displ.bw-list is not a real implicit operand -- but we treat it like
		# one.
		$s =~ s/displ\.bw-list//g;

		my @x = split /,/, $s;
		foreach my $op (@x) {
			# ignore implicit operannds
			if ($op eq "") {
				next;
			}

			# split into name/type
			my ($name, $type) = split /[.]/, $op;

			# check that type is two letters
			if ($type !~ /^[a-zA-Z][a-zA-Z]$/) {
print "  |$s|$op|$type|  bad type ************\n";
			}

			# check first letter
			my $access = substr $type, 0, 1;
			if ($access !~ /[rwmavb]/) {
print "   bad access mode ($access)  ****\n";
			}

			# check second letter
			my $len = substr $type, 1, 1;
			if ($len !~ /[bwlqfdg]/) {
print "   bad type/len ($len)  ****\n";
			}

			# fold floating-point types to int types of same length.
			# doesn't help a whole lot with the number of different
			# oplists.
#			$len =~ tr/fdg/lqq/;

			$spec = $spec . $access . $len . " ";
		}

		$ops{$i} = $spec;
	}
}


# override operand lists for a select few instructions
#
#     ADAWI  			      "rw mw.i"
#     BBSSI/BBCCI	 	      "rl vb.i bb"
#     BBS/BBC, BBSS/BBCS/BBSC/BBCC    "rl vb.1 bb"
#     CMPV/CMPZV		      "rl rb   vb.r rl"
#     EXTV/EXTZV, FFC/FFS             "rl rb   vb.r wl"
#     INSV                            "rl rl   rb   vb.m"
#
#     mw.i   mi   modify word interlocked
#     vb.r   vr   read field
#     vb.m   vm   modify field
#     vb.1   v1   read 1-bit field
#     vb.i   vi   modify 1-bit field interlocked
#
#
# INSQHI/INSQTI/REMQHI/REMQTI also use interlocked rmw but that happens during
# exe phase and not during the operand processing.

my %override = (
	"ADAWI" => "rw mi ",

	"BBCCI" => "rl vi bb ",
	"BBSSI" => "rl vi bb ",

	"BBS"   => "rl v1 bb ",
	"BBC"   => "rl v1 bb ",
	"BBSS"  => "rl v1 bb ",
	"BBCS"  => "rl v1 bb ",
	"BBSC"  => "rl v1 bb ",
	"BBCC"  => "rl v1 bb ",

	"CMPV"  => "rl rb vr rl ",
	"CMPZV" => "rl rb vr rl ",
	"EXTV"  => "rl rb vr wl ",
	"EXTZV" => "rl rb vr wl ",
	"FFC"   => "rl rb vr wl ",
	"FFS"   => "rl rb vr wl ",

	"INSV"  => "rl rl rb vm ",
);




# there are not all that many different operand lists -- let's find out what
# they are.

my @stdop = ();	# list of unique operand lists

# opcode => idx into stdop
my %op = ();

# make list of unique operand lists -- FIXME should use overrides!
sub compress_ops()
{
	# ops strings => idx
	my %ophash = ();

	# loop through opcodes
	for (my $i = 0; $i < 512; $i++) {
		if (exists $ops{$i}) {	# if the instruction exists
			# get operand list
			my $s;
			if (exists $override{$mne{$i}}) {
				$s = $override{$mne{$i}};
			} else {
				$s = $ops{$i};
			}

			# is the oplist already known?
			if (not exists $ophash{$s}) {
				$ophash{$s} = scalar @stdop;
				push @stdop, $s;
			}

			# remember the oplist idx
			$op{$i} = $ophash{$s};
		}
	}
}


my %type_width = (
        'b'     =>  1,
        'w'     =>  2,
        'l'     =>  4,
        'q'     =>  8,
        'o'     => 16,

        'f'     =>  4,
        'd'     =>  8,
        'g'     =>  8,
        'h'     => 16,
);


my %type_ifp = (
        'b'     => 'IFP_INT',
        'w'     => 'IFP_INT',
        'l'     => 'IFP_INT',
        'q'     => 'IFP_INT',
        'o'     => 'IFP_INT',

        'f'     => 'IFP_F',
        'd'     => 'IFP_D',
        'g'     => 'IFP_G',
        'h'     => 'IFP_H',
);

sub width($) {
	my ($optype) = @_;

	# remove first letter (access type) so data type letter remains
	$optype =~ s/^.//;
	$type_width{$optype};
}

sub ifp($) {
	my ($optype) = @_;

	# remove first letter (access type) so data type letter remains
	$optype =~ s/^.//;
	$type_ifp{$optype};
}


sub print_for_c() {
	# I believe in beautiful tables!

	print  "/* Tables for VAX models -- autogenerated by 'instr.pl --c < instr.snip' */\n";
	print  "\n";
	printf "/* generated %s */\n", (strftime "%Y-%m-%d %H:%M:%S", localtime);
	print  "\n";

	printf "#ifndef VAX_INSTR__H\n";
	printf "#define VAX_INSTR__H\n";
	printf "\n";

	# mne[]
	print  "const char *mne[512] = {\n";
	print  "/* single-byte opcodes */\n";
	print "\n";

	for (my $i = 0; $i < 256; $i++) {
		if (($i & 0x7) == 0) {
			printf "/* %02X */ ", $i;
		}

		if (($i & 0x7) == 0x4) {
			print "   ";
		}

		my $fmt = (($i & 0x7) == 0x7) ? "%s\n" : "%-9s";
		if (exists $mne{$i}) {
			printf $fmt, '"' . $mne{$i} . '",';
		} else {
			printf $fmt, '"",';
		}
	}

	print "\n";
	print "/* FD prefix */\n";
	print "\n";

	for (my $i = 256; $i < 512; $i++) {
		if (($i & 0x7) == 0) {
			printf "/* %02X */ ", $i - 0x100;
		}

		if (($i & 0x7) == 0x4) {
			print "   ";
		}

		my $fmt = (($i & 0x7) == 0x7) ? "%s\n" : "%-9s";
		if (exists $mne{$i}) {
			printf $fmt, '"' . $mne{$i} . '",';
		} else {
			printf $fmt, '"",';
		}
	}
	print  "};\n";

	print "\n";
	print "\n";

	# syn[]
	print  "/* mnemonic synonyms -- for example CLRF is a synonym for CLRQ */\n";
	print  "struct {\n";
	print  "        uint16_t     op;\n";
	print  "        const char  *name;\n";
	printf " } syn[%d] = {\n", scalar @syn;
	foreach my $v (@syn) {
		my $opcode = $$v[0];
		my $instr  = $$v[1];

		if ($opcode < 0x100) {
			printf "        { .op =   0x%02X,   .name = \"%s\"%s},\n",
				$opcode, $instr, " " x (6 - length $instr);
		} else {
			printf "        { .op = 0x%02XFD,   .name = \"%s\"%s},\n",
				$opcode - 0x100, $instr, " " x (6 - length $instr);
		}
	}
	print "};\n";
	print "\n";
	print "\n";

	print "/* A table of operands for each instruction -- this table is intended for asm/dis.\n";
	print "\n";
	print "   Operands are described by letter pairs with the following interpretation:\n";
	print "\n";
	print "   Access types\n";
	print "     r  read\n";
	print "     w  write\n";
	print "     m  modify\n";
	print "     a  address\n";
	print "     b  branch\n";
	print "     v  bitfield (\"vector\" or \"variable-length\" bitfield)\n";
	print "        (always uses the b (byte) data type.)\n";
	print "\n";
	print "   Data types\n";
	print "     b  byte (8 bits)\n";
	print "     w  word (16 bits)\n";
	print "     l  longword (32 bits)\n";
	print "     q  quadword (64 bits)\n";
	print "     o  octoword (128 bits)\n";
	print "\n";
	print "     f  floating-point (32-bit, inherited from PDP-11)\n";
	print "     d  floating-point (64-bit, old style, inherited from PDP-11)\n";
	print "     g  floating-point (64-bit, new style)\n";
	print "     h  floating-point (128-bit)\n";
	print "\n";
	print "\n";
	print "   Not all combinations exist.\n";
	print "\n";
	print "   Some VAX models don't have native support for octoword and h floating-point.\n";
	print " */\n";
	print "\n";

	# ops[]
	print "const char *ops[512] = {\n";
	for (my $i = 0; $i < 512; $i++) {
		if ($i == 0) {
			print  "/* single-byte opcodes */\n";
			print "\n";
		} elsif ($i == 256) {
			print "/* FD prefix */\n";
			print "\n";
		}

		if (exists $ops{$i}) {
			printf "/* %02X %-6s */  \"%s\",\n", $i & 0xFF, $mne{$i}, $ops{$i};
		} else {
			printf "/* %02X %-6s */  \"\",\n", $i & 0xFF, "---";
		}

		if ((($i & 0xF) == 0xF) && ($i != 511)) {
			print "\n";
		}
	}
	print "};\n";
	print "\n";
	print "\n";

	print "/* A table of the number of operands each instruction has. */\n";
	print "unsigned op_cnt[512] = {\n";
	for (my $i = 0; $i < 512; $i++) {
		if ($i == 0) {
			print  "/* single-byte opcodes */\n";
			print "\n";
		} elsif ($i == 256) {
			print "/* FD prefix */\n";
			print "\n";
		}

		if (($i & 0xF) == 0) {
			print "/*               0123456 */\n";
		}

		if (exists $ops{$i}) {
			use integer;	# we want integer division
			my $cnt = length($ops{$i}) / 3;

			printf "/* %02X %-6s */  %s%d,\n", $i & 0xFF, $mne{$i}, " " x $cnt, $cnt;
		} else {
			printf "/* %02X %-6s */  0,\n", $i & 0xFF, "---";
		}

		if ((($i & 0xF) == 0xF) && ($i != 511)) {
			print "\n";
		}
	}
	print "};\n";
	print "\n";
	print "\n";

	print "/* A table of operand widths for each instruction -- intended for asm/dis. */\n";
	print "int op_width[512][6] = {\n";
	for (my $i = 0; $i < 512; $i++) {
		if ($i == 0) {
			print  "/* single-byte opcodes */\n";
			print "\n";
		} elsif ($i == 256) {
			print "/* FD prefix */\n";
			print "\n";
		}

		if (exists $ops{$i}) {
			my $s = join(", ", map { width($_) } split(" ", $ops{$i}));

			printf "/* %02X %-6s */  {%s},\n", $i & 0xFF, $mne{$i}, $s;
		} else {
			printf "/* %02X %-6s */  {},\n", $i & 0xFF, "---";
		}

		if ((($i & 0xF) == 0xF) && ($i != 511)) {
			print "\n";
		}
	}
	print "};\n";
	print "\n";
	print "\n";

	print "/* all non-fp operands are \"integers\" -- including branch displacements */\n";
	print "enum ifp	{ IFP_INT, IFP_F, IFP_D, IFP_G, IFP_H };\n";
	print "\n";
	print "/* A table of operand types (int/f/d/g/h) for each instruction -- intended for asm/dis. */\n";
	print "enum ifp op_ifp[512][6] = {\n";
	for (my $i = 0; $i < 512; $i++) {
		if ($i == 0) {
			print  "/* single-byte opcodes */\n";
			print "\n";
		} elsif ($i == 256) {
			print "/* FD prefix */\n";
			print "\n";
		}

		if (exists $ops{$i}) {
			my $s = join(", ", map { ifp($_) } split(" ", $ops{$i}));

			printf "/* %02X %-6s */  {%s},\n", $i & 0xFF, $mne{$i}, $s;
		} else {
			printf "/* %02X %-6s */  {},\n", $i & 0xFF, "---";
		}

		if ((($i & 0xF) == 0xF) && ($i != 511)) {
			print "\n";
		}
	}
	print "};\n";
	print "\n";
	print "\n";


	print "/* A table of operands for each instruction -- this table is intended for sim.\n";
	print "\n";
	print "   Operands are described by letter pairs with the following interpretation:\n";
	print "\n";
	print "   Access types\n";
	print "     r  read\n";
	print "     w  write\n";
	print "     m  modify (mi=modify word interlocked)\n";
	print "     a  address\n";
	print "     b  branch\n";
	print "     v  bitfield (\"vector\" or \"variable-length\" bitfield)\n";
	print "        (vr=read, vm=modify, v1=read 1 bit, vi=modify 1 bit interlocked)\n";
	print "\n";
	print "   Data types\n";
	print "     b  byte (8 bits)\n";
	print "     w  word (16 bits)\n";
	print "     l  longword (32 bits)\n";
	print "     q  quadword (64 bits)\n";
	print "     o  octoword (128 bits)\n";
	print "\n";
	print "     f  floating-point (32-bit, inherited from PDP-11)\n";
	print "     d  floating-point (64-bit, old style, inherited from PDP-11)\n";
	print "     g  floating-point (64-bit, new style)\n";
	print "     h  floating-point (128-bit)\n";
	print "\n";
	print "     i  modify interlocked (mi=modify word, vi=modify 1 bit)\n";
	print "\n";
	print "\n";
	print "   Not all combinations exist.\n";
	print "\n";
	print "   Some operand lists have been rewritten in order to describe interlocked word\n";
	print "   modification (ADAWI) and bitfields correctly, including 1-bit fields and\n";
	print "   interlocking.  This leads to the following extra pairs that don't follow the\n";
	print "   normal naming rules:\n";
	print "\n";
	print "     mi  modify word interlocked\n";
	print "     vr  read variable-size bitfield\n";
	print "     vm  modify variable-size bitfield\n";
	print "     v1  read 1-bit field\n";
	print "     vi  modify 1-bit field interlocked\n";
	print "\n";
	print "   Some VAX models don't have native support for octoword and h floating-point.\n";
	print " */\n";
	print "\n";

	# sim_ops[]
	print "const char *sim_ops[512] = {\n";
	for (my $i = 0; $i < 512; $i++) {
		if ($i == 0) {
			print  "/* single-byte opcodes */\n";
			print "\n";
		} elsif ($i == 256) {
			print "/* FD prefix */\n";
			print "\n";
		}

		if (exists $ops{$i}) {
			my $s;
			if (exists $override{$mne{$i}}) {
				$s = $override{$mne{$i}};
			} else {
				$s = $ops{$i};
			}

			printf "/* %02X %-6s */  \"%s\",\n", $i & 0xFF, $mne{$i}, $s;
		} else {
			printf "/* %02X %-6s */  \"\",\n", $i & 0xFF, "---";
		}

		if ((($i & 0xF) == 0xF) && ($i != 511)) {
			print "\n";
		}
	}
	print "};\n";
	print "\n";
	print "\n";

	print "/* end of generated tables */\n";
	print "\n";

	print "#endif\n";
	print "\n";
}


sub print_for_humans() {
	# mne[]
	for (my $i = 0; $i < 256; $i++) {
		if (exists $mne{$i}) {
			printf "   %02X  %s\n", $i, $mne{$i};
		} elsif ($i < 0xFD) {
			printf "   %02X  *** undefined ***********\n", $i;
		}
	}
	for (my $i = 256; $i < 512; $i++) {
		if (exists $mne{$i}) {
			printf "FD %02X  %s\n", $i-0x100, $mne{$i};
		}
	}
	print "\n";

	# syn[]
	foreach my $v (@syn) {
		my $opcode = $$v[0];
		my $instr  = $$v[1];

		if ($opcode < 0x100) {
			printf "   %02X  %s\n", $opcode, $instr;
		} else {
			printf "FD %02X  %s\n", $opcode - 0x100, $instr;
		}
	}


	# operand lists
	for (my $i = 0; $i < 512; $i++) {
		if (exists $oplist{$i}) {
			printf "%02X  %s -- #%d\n", $i, $oplist{$i}, $op{$i};
		}
	}
	print "\n";

	# stdop[]
	print "stdop[]\n";
	for (my $i = 0; $i < scalar @stdop; $i++) {
		printf "%3d  %s\n", $i, $stdop[$i];
	}
	print "\n";
}


# Perl version of mne[] table for uasm.pl
sub print_for_perl()
{
	print  "# Tables for VAX models -- autogenerated by 'instr.pl --perl < instr.snip'\n";
	print  "\n";
	printf "# generated %s\n", (strftime "%Y-%m-%d %H:%M:%S", localtime);
	print  "\n";

	# mne[]
	print  "our \@mne = (\n";
	print  "# single-byte opcodes\n";
	print "\n";

	for (my $i = 0; $i < 256; $i++) {
		if (($i & 0x7) == 0) {
			printf "# %02X\n", $i;
			print  "     ";
		}

		if (($i & 0x7) == 0x4) {
			print "   ";
		}

		my $fmt = (($i & 0x7) == 0x7) ? "%s\n" : "%-9s";
		if (exists $mne{$i}) {
			printf $fmt, '"' . $mne{$i} . '",';
		} else {
			printf $fmt, '"",';
		}
	}

	print "\n";
	print "# FD prefix\n";
	print "\n";

	for (my $i = 256; $i < 512; $i++) {
		if (($i & 0x7) == 0) {
			printf "# %02X\n", $i - 0x100;
			print  "     ";
		}

		if (($i & 0x7) == 0x4) {
			print "   ";
		}

		my $fmt = (($i & 0x7) == 0x7) ? "%s\n" : "%-9s";
		if (exists $mne{$i}) {
			printf $fmt, '"' . $mne{$i} . '",';
		} else {
			printf $fmt, '"",';
		}
	}
	print  ");\n";
	print "\n";

	# syn[]
	print  "our \@syn = (\n";
	foreach my $v (@syn) {
		my $opcode = $$v[0];
		my $instr  = $$v[1];

		if ($opcode < 0x100) {
			printf "        { 'op' =>   0x%02X, 'name' => \"%s\"%s},\n",
				$opcode, $instr, " " x (6 - length $instr);
		} else {
			printf "        { 'op' => 0x%02XFD, 'name' => \"%s\"%s},\n",
				$opcode - 0x100, $instr, " " x (6 - length $instr);
		}
	}
	print ");\n";
	print "\n";
	print "\n";

	# tell Perl that the "module" initialized ok
	print "1;\n";
}



sub help() {
	print "./instr.pl <flags>  < input > output\n";
	print "\n";
	print "flags:\n";
	print "    --humans\n";
	print "    --c\n";
	print "    --perl\n";
	exit;
}


if ($#ARGV == -1) {
	help();
}

if      ($ARGV[0] eq "--humans") {
	shift @ARGV;

	read_snippet();

	cleanup_oplists();
	compress_ops();

	print_for_humans();

} elsif ($ARGV[0] eq "--c") {
	shift @ARGV;

	read_snippet();

	cleanup_oplists();
	compress_ops();

	print_for_c();

} elsif ($ARGV[0] eq "--perl") {
	shift @ARGV;

	read_snippet();

	cleanup_oplists();
	compress_ops();

	print_for_perl();
} else {
	help();
}


