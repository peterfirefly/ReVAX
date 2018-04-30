#!/usr/bin/perl

# Copyright 2018  Peter Lund <firefly@vax64.dk>
#
# Licensed under GPL v2.
#
# ---
#
# Generate microcode types, constants, tables, and stats.
#
#   misc/uasm-experiment.pl < src/ucode.vu > src/vax-ucodex.h
#

use strict;
use warnings;

use POSIX qw(strftime);

# import @mne, @syn from src/vax-instr.pl
push @INC, "src";
require 'vax-instr.pl';

our (@mne, @syn);

###

# the valid µops are specified in src/uops.spec
#
# each valid µop consists of a mnemonic, 1/2/3 operands, and possibly a width
# and a flag argument.
#
# to keep uasm.pl as general (and short and clean) as possible, almost no
# information about the µops is kept in this file.


# This bit masks are used to keep track of which fields (operands + width/flags)
# each uop has.
#
# These are used by dis-uop.h so it can print out the correct fields.

# field mask values
my %field = (
	'UF_S1'		=> 1 << 0,
	'UF_M1'		=> 1 << 1,
	'UF_S2'		=> 1 << 2,
	'UF_M2'		=> 1 << 3,
	'UF_DST'	=> 1 << 4,
	'UF_IMM'	=> 1 << 5,
	'UF_CC'		=> 1 << 6,
	'UF_UTARGET'	=> 1 << 7,
	'UF_WIDTH'	=> 1 << 8,
	'UF_FLAGS'	=> 1 << 9,
);


# read uops.spec
#
# uop_no:    mnemonic --> integer
# uop_name:  integer  --> mnemonic
# uop_regex: mnemonic --> regexp that parses the operands + width/flags
# uop_eval:  mnemonic --> string of Perl code that assigns $n to fields in a Perl "struct"
#                         (Perl can simulate a struct with a hash)
# uop_mask:  mnemonic --> integer (mask) that indicates which fields are used by the µop

my %uop_no    = ();
my %uop_name  = ();
my %uop_regex = ();
my %uop_eval  = ();
my %uop_mask  = ();


sub read_spec() {
	open(my $F, "<:encoding(UTF-8)", "src/uops.spec") or
		die "Could not open src/uops.spec";

	my $uop_no = 0;
	while (<$F>) {
		# kill trailing \n
		chomp;

		# cut off comments
		s/#.*$//;

		# trim whitespace
		s/^\s+//;	# beginning
		s/\s+$//;	# end
		s/[\t ]+/ /g;	# middle

		# skip empty lines
		next if $_ eq '';

		# parse valid spec (or error out)
		if (not
		    /^(\S+)
		      (?:\s+([a-zA-Z0-9\[\]]+)
		       (?:\s*,\s*([a-zA-Z0-9\[\]]+)
		        (?:\s*,\s*(\S+))?
		       )?
		      )?
		      (?:\s*--\s*(width|flags|width\s+flags))?
		      $/x) {
		      	# not a valid uop spec
			die "|$_| is not a valid uop specifiction, line $..\n";
		}

		my ($uop, $op1,$op2,$op3, $wf) = ($1, $2,$3,$4, $5);

		die "|$uop| uop is already defined, line $..\n" if exists $uop_regex{$uop};

		# bit mask that describes the uop's fields
		my $fields = 0;
		if (defined $op1) {
			$fields |= $field{'UF_IMM'}  if $op1 eq 'imm';
			$fields |= $field{'UF_S1'}   if $op1 eq 's1';
			$fields |= $field{'UF_M1'}   if $op1 eq '[s1]';
			$fields |= $field{'UF_DST'}   if $op1 eq 'dst';
			$fields |= $field{'UF_CC'}   if $op1 eq 'cc';
			die "|$op1| is not legal as operand 1, line $..\n" if $op1 !~ /(imm|s1|\[s1\]|dst|cc)/;
		}

		if (defined $op2) {
			$fields |= $field{'UF_S2'}       if $op2 eq 's2';
			$fields |= $field{'UF_M2'}       if $op2 eq '[s2]';
			$fields |= $field{'UF_DST'}      if $op2 eq 'dst';
			$fields |= $field{'UF_UTARGET'}  if $op2 eq 'utarget';
			die "|$op2| is not legal as operand 2, line $..\n" if $op2 !~ /(s2|\[s2\]|dst|utarget)/;
		}

		if (defined $op3) {
			$fields |= $field{'UF_DST'}      if $op3 eq 'dst';
			die "|$op3| is not legal as operand 3, line $..\n" if $op3 !~ /dst/;
		}

		if (defined $wf) {
			$fields |= $field{'UF_WIDTH'}    if ($wf eq 'width') || ($wf eq 'width flags');
			$fields |= $field{'UF_FLAGS'}    if ($wf eq 'flags') || ($wf eq 'width flags');
			die "|$wf| is not width/flags, line $..\n" if $wf !~ /(width|flags|width flags)/;
		}

		# regular expression that matches the uop's operands
		my @re_parts = ();
		push @re_parts, '(\S+)'		if $fields & $field{'UF_IMM'};
		push @re_parts, '(\S+)'		if $fields & $field{'UF_S1'};
		push @re_parts, '\[(\S+)\]'	if $fields & $field{'UF_M1'};
		push @re_parts, '(\S+)'		if $fields & $field{'UF_S2'};
		push @re_parts, '\[(\S+)\]'	if $fields & $field{'UF_M2'};
		push @re_parts, '(\S+)'		if $fields & $field{'UF_DST'};
		push @re_parts, '(\S+)'		if $fields & $field{'UF_CC'};
		push @re_parts, '(\S+)'		if $fields & $field{'UF_UTARGET'};

		my $regexp = join('\s*,\s*', @re_parts);

		# eval string (Perl code) that assigns fields for the uop's operands
		my @eval = ();
		if (defined $op1) {
			push @eval, "'imm' => conv_imm32(\$1)"	if  $op1 eq 'imm';
			push @eval, "'s1' => conv_reg(\$1)"	if ($op1 eq 's1') || ($op1 eq '[s1]');
			push @eval, "'dst' => conv_reg(\$1)"	if  $op1 eq 'dst';
			push @eval, "'cc' => conv_cc(\$1)"	if  $op1 eq 'cc';
		}
		if (defined $op2) {
			push @eval, "'s2' => conv_reg(\$2)"	if ($op2 eq 's2') || ($op2 eq '[s2]');
			push @eval, "'dst' => conv_reg(\$2)"	if  $op2 eq 'dst';
			push @eval, "'utarget' => conv_utarget(\$2)"	if  $op2 eq 'utarget';
		}
		if (defined $op3) {
			push @eval, "'dst' => conv_reg(\$3)"	if  $op3 eq 'dst';
		}
		my $eval = join(', ', @eval);

		# regular expressions and eval strings for width/flags
		if      (($fields & ($field{'UF_WIDTH'} | $field{'UF_FLAGS'})) == $field{'UF_WIDTH'}) {
			$regexp .= '\s*--\s*(\S+)';
			$eval   .= ", 'width' => conv_width(\$" . (scalar @eval + 1) . ")";
		} elsif (($fields & ($field{'UF_WIDTH'} | $field{'UF_FLAGS'})) == $field{'UF_FLAGS'}) {
			$regexp .= '\s*--\s*(\S+)';
			$eval   .= ", 'flags' => conv_flags(\$" . (scalar @eval + 1) . ")";
		} elsif (($fields & ($field{'UF_WIDTH'} | $field{'UF_FLAGS'})) == 0) {
		} else {
			$regexp .= '\s*--\s*(\S+)\s+(\S+)';
			$eval   .= ", 'width' => conv_width(\$" . (scalar @eval + 1) . '), ' .
			           "'flags' => conv_flags(\$" . (scalar @eval + 2) . ")";
		}

		# complete regex/eval
		$regexp = '^' . $regexp . '$';
		$eval = "{'op' => $uop_no, $eval }";

		# remember the uop
		$uop_no{$uop} = $uop_no;
		$uop_name{$uop_no} = $uop;
		$uop_regex{$uop} = $regexp;
		$uop_eval{$uop} = $eval;
		$uop_mask{$uop} = $fields;

		$uop_no++;
	}

	close($F);
}


###

# This file is the canonical document for register numbers, for internal
# processor registers, and for condition codes.
#
# The register numbers used for p*, e* need to be synchronized with the
# address handling microcode and the decoder (that pieces all the microcode
# fragments together for each instruction).  The number of registers written
# to in the pre phase and exe phase depends on the microcode strategy used.

my %reg = (
	# The 16 architected GPR's (SP and PC are treated like GPR's)
	"r0"	  =>  0,
	"r1"	  =>  1,
	"r2"	  =>  2,
	"r3"	  =>  3,

	"r4"	  =>  4,
	"r5"	  =>  5,
	"r6"	  =>  6,
	"r7"	  =>  7,

	"r8"	  =>  8,
	"r9"	  =>  9,
	"r10"	  => 10,
	"r11"	  => 11,

	"r12"	  => 12,	# ap
	"r13"	  => 13,	# fp
	"r14"	  => 14,	# sp
	"r15"	  => 15,	# pc

	# internal registers, set in pre phase
	"p1"	  => 16,
	"p2"	  => 17,
	"p3"	  => 18,
	"p4"	  => 19,
	"p5"	  => 20,
	"p6"	  => 21,
	"p7"	  => 22,

	# internal registers, set in exe phase (read in post phase)
	"e1"	  => 23,
	"e2"	  => 24,
	"e3"	  => 25,

	# temporaries for microcode, any microcode flow can overwrite them
	"t0"	  => 26,
	"t1"	  => 27,

	# the program status longword (= the flags)
	"psl"     => 28,

	# template values in operand microcode, gets filled in by decode unit
	# with actual GPRs (<Rn>, <Rx>), p_n (<pre>), or a GPR or e_n (<exe>).
	"<Rn>"	  => 40,
	"<Rx>"	  => 41,
	"<pre>"   => 42,
	"<exe>"   => 43,
	"<reg>"   => 44,
);

my %reg_alias = (
	"ap"	  => "r12",
	"fp"	  => "r13",
	"sp"	  => "r14",
	"pc"	  => "r15",
);

# No. of real registers (i.e. not counting placeholder values)
my $register_count = 29;


my %preg = (
	"KSP"	 =>  0,
	"ESP"	 =>  1,
	"SSP"	 =>  2,
	"USP"	 =>  3,
	"ISP"	 =>  4,

	"P0BR"	 =>  8,
	"P0LR"	 =>  9,
	"P1BR"	 => 10,
	"P1LR"	 => 11,
	"SBR"	 => 12,
	"SLR"	 => 13,

	"PCBB"	 => 16,
	"SCBB"	 => 17,

	"IPL"	 => 18,
	"ASTLVL" => 19,

	"SIRR"	 => 20,
	"SISR"	 => 21,

	"MAPEN"	 => 56,
	"TBIA"	 => 57,
	"TBIS"	 => 58,

	"SID"	 => 62,
	"TBCHK"	 => 63,
);


my %cc = (
	# 4 bit patterns not used in VAX Bcc opcodes.
	# we use them for unconditional µbranches, µcalls/µrets, and a
	# placeholder that tells the decoder to use the cc bits from the
	# instruction (presumably one of the 12 Bcc instructions).
	"ret"	=>  0,
	"<cc>"	=>  1,
	"call"	=>  6,
	"always"=>  7,

	# 12 bit patterns used in VAX Bcc opcodes
	"neq"	=>  2,
	"!="	=>  2,
	"!z"	=>  2,

	"eql"	=>  3,
	"="	=>  3,
	"z"	=>  3,

	"gtr"	=>  4,
	">"	=>  4,

	"leq"	=>  5,
	"<="	=>  5,

	"geq"	=>  8,
	">="	=>  8,
	"!n"	=>  8,

	"lss"	=>  9,
	"<"	=>  9,
	"n"	=>  9,

	"gtru"	=> 10,
	">u"	=> 10,

	"lequ"	=> 11,
	"<=u"	=> 11,

	"!v"	=> 12,

	"v"	=> 13,

	"gequ"	=> 14,
	">=u"	=> 14,
	"!c"	=> 14,

	"lssu"	=> 15,
	"<u"	=> 15,
	"c"	=> 15,
);


my @cc_out = (
	"ret",		#  0	-- µcode use
	"<cc>",		#  1	-- placeholder
	"!=/!z",	#  2
	"=/z",		#  3

	">",		#  4
	"<=",		#  5
	"call",		#  6	-- µcode use
	"always",	#  7	-- µcode use

	">=",		#  8
	"<",		#  9
	">u",		# 10
	"<=u",		# 11

	"!v",		# 12
	"v",		# 13
	">=u/!c",	# 14
	"<u/c",		# 15
);


# valid widths (+ a placeholder)
my %widths = (
	  '8'		=> 0,
	 '16'		=> 1,
	 '32'		=> 2,
	 '64'		=> 3,
	'128'		=> 4,
	'<width>'	=> 5,
);


# we have two flag sets (for NZVC)
my %flags = (
	'arch'	=> 0,
	'µ'	=> 1,
);


###

# Globals used while we read/parse the microcode

# line number in ucode.vu -- it's updated by read_ucode().
# it's a global so the conv_xxx() functions can refer to it.
my $lineno;

# line number for each uop in the microcode
my @lineno = ();

# track jumps so they can be backpatched, see conv_utarget() and patch_jumps()
my @jmps = ();

# labels in the microcode (label --> µaddr)
my %lbls = ();

# all the microcode we have seen so far, used by conv_utarget() because it
# needs to indicate where the jumps are in the microcode.
#
# µops are stored as references to "structs" (hashes):
#
#  'op'
#  'imm'
#  's1', 's2', 'dst'
#  'cc', 'utarget'
#  'width'
#  'flags'
#  'last'
my @ucode = ();

# placeholder value (<imm>) for imm fields -- pick an arbitrary value that
# doesn't occur in the microcode.
my $imm_placeholder = 0x1234;

# bit flag set on utarget iff the bcc/exc is exc
my $exc_flag = 0x8000;


###

# "field parsers" -- convert a field (as a string) to numbers
#
# they are called by the eval() call in the reader loop in read_ucode().
# the calls are prepared as uops.spec is read in read_spec() above.

sub conv_reg($) {
	my ($regname) = @_;

	my $regname2 = $regname;
	$regname2 = $reg_alias{$regname} if exists $reg_alias{$regname};

	die "|$regname2| is not a register name, line $lineno.\n" unless exists $reg{$regname2};
	return $reg{$regname2};
}


sub conv_imm32($) {
	my ($imm) = @_;

	return $imm_placeholder if $imm eq '<imm>';
	return $preg{$imm}      if exists $preg{$imm};

	$imm =~ s/_//g;	# '_' is a "space" inside numbers

	return hex $imm if $imm =~ /^0x[0-9a-fA-F]+$/;
	return $imm + 0 if $imm =~ /^[0-9]+$/;

	die "|$imm| is not a value, line $lineno.\n";
}


sub conv_cc($) {
	my ($cc) = @_;

	die "|$cc| is not a valid cc, line $lineno.\n" unless exists $cc{$cc};
	return $cc{$cc};
}


# track the jumps so they can be backpatched after all the microcode has been
# seen.
sub conv_utarget($) {
	my ($label) = @_;

	$label = uc $label;
	push @jmps, {'from' => scalar @ucode, 'to' => $label, 'lineno' => $lineno};
	return 0;	# assume 'bcc'
}


sub conv_width($) {
	my ($width) = @_;

	die "|$width| is not a valid width, line $lineno.\n" unless exists $widths{$width};
	return $widths{$width};
}


sub conv_flags($) {
	my ($flags) = @_;

	die "|$flags| is not a valid flag set, line $lineno.\n" unless exists $flags{$flags};
	return $flags{$flags};
}



###

# read uops.spec
#
# uop_regex: mnemonic --> regexp that parses the operands + width/flags
# uop_eval:  mnemonic --> string of Perl code that assigns $n to fields in a Perl "struct"
#                         (Perl can simulate a struct with a hash)

sub read_ucode() {
	while (<>) {
		my $line = $_;
		$lineno = $.;

		# kill trailing \n
		chomp $line;

		# cut off comments
		$line =~ s/#.*$//;

		# label
		if ($line =~ s/^([a-zA-Z0-9\[\]\(\)_\+-]+)://) {
			# FIXME don't throw the label's case away!
			#       ulabels[] is much nicer if case is preserved.

			my $label = uc $1;

			die "|$label| is a duplicate label, line $lineno.\n" if exists $lbls{$label};

			# is it the wrong name for an instruction with multiple names?
			if (grep {$label eq $_->{'name'}} @syn) {
				my $correct = $mne[(grep {$label eq $_->{'name'}} @syn)[0]->{'op'}];
				die "please use |$correct| instead of |$label|, line $lineno.\n";
			}

			# labels must be instructions or "special" (= start with '_')
			if (($label !~ /^-/) and not grep {$label eq $_} @mne) {
				die "|$label| is not a special label and not an instruction, line $lineno.\n";
			}

			$lbls{$label} = scalar @ucode;
		}

		# trim whitespace
		$line =~ s/^\s+//;
		$line =~ s/\s+$//;

		# flow termination?
		if ($line eq '---') {
#			die "can't terminate a flow before the first µop, line $lineno.\n" if scalar @ucode == 0;
#			die "flow has already been terminated, line $lineno.\n" if exists $ucode[-1]->{'last'};

			$ucode[-1]->{'last'} = 1;
			next;
		}

		# skip empty lines
		next if $line eq '';

		# special-case 'exc', 'b'
		my $exc = 0;
		$exc = 1	if $line =~ s/^exc\s+/bcc /;
		$line =~ s/^b\s+(\S+)$/bcc    always, $1 -- µ/;

		# gotta be a uop, right?
		die "|$line| doesn't begin with a mnemonic, line $lineno.\n" unless $line =~ s/^(\S+)\s*//;
		my $mne = $1;
		die "|$mne| is not a valid mnemonic, line $lineno.\n" unless exists $uop_regex{$mne};

		# match the rest of the line
		die "|$line| do not match |$mne| '$uop_regex{$mne}', line $lineno.\n" unless $line =~ $uop_regex{$mne};

		# build tuple for the micro instruction
		my $instr = eval($uop_eval{$mne});
		die "regexp failed: $@" if $@;

		# exc?
		$instr->{'utarget'} = 1		if $exc;

		push @ucode, $instr;
		push @lineno, $.;
	}
}

###


# note which labels are used and which aren't so we can print out stats
my %lbl_use = ();

# patch all bcc/exc with the correct address
sub patch_jumps()
{
	foreach my $jmp (@jmps) {
		my ($from, $to, $lineno) = ($jmp->{'from'}, $jmp->{'to'}, $jmp->{'lineno'});

		die "branch to nonexistent label |$to| at line $lineno.\n" if not exists $lbls{$to};

		# 0=bcc,  1=exc
		my $old_target = $ucode[$from]->{'utarget'};

		if ($old_target == 0) {
			$ucode[$from]->{'utarget'} = $lbls{$to};
		} else {
			$ucode[$from]->{'utarget'} = $lbls{$to} + $exc_flag;
		}

		# statistics
		$lbl_use{$to} = 0 if not exists $lbl_use{$to};
		$lbl_use{$to} = $lbl_use{$to} + 1;
	}
}


###


my $maxflowlen = 0;
my %flowlens = ();

# check that all flows are terminated and calculate flow lengths
sub check_ucode() {
	my $flowlen = 0;

	for (my $i=0; $i < scalar @ucode; $i++) {
		$flowlen = $flowlen + 1;


		if (exists $ucode[$i]->{'last'}) {
			if (not exists $flowlens{$flowlen}) {
				$flowlens{$flowlen} = 0;
			}
			$flowlens{$flowlen} = $flowlens{$flowlen} + 1;

			if ($flowlen > $maxflowlen) {
				$maxflowlen = $flowlen;
			}

			$flowlen = 0;
		}
	}
	if ($flowlen != 0) {
		print "The last flow isn't terminated.\n";
		exit 1;
	}
}


###


# take a label and make it a valid C identifier by replacing chars
sub clabel($) {
	my ($s) = @_;

	$s =~ s/\+/x/g;
	$s =~ s/[\[\]\(\)-]/_/g;
	$s;
}


sub write_tables() {
	printf "/* Microcode-related tables -- autogenerated by 'uasm.pl < ucode.vu'\n";
	printf "   (uasm.pl also reads uops.spec)\n";
	printf "\n";
	printf "   Generated by uasm.pl %s\n", strftime("%Y-%m-%d %H:%M:%S", localtime);
	printf " */\n";
	printf "\n";
	printf "#ifndef VAX_UCODE__H\n";
	printf "#define VAX_UCODE__H\n";
	printf "\n\n";

	# enum uop / uop[]
	printf "enum uopcode {\n";
	foreach my $v (sort {$a <=> $b} values %uop_no) {
		my %rev_uop_no = reverse %uop_no;
		my $name = uc $rev_uop_no{$v};
		$name = 'INC'   if $name eq '++';
		$name = 'DEC'   if $name eq '--';
		$name = 'INDEX' if $name eq '[]';
		printf "\t%-10s\t= %2d,\n", "U_$name", $v;
	}
	printf "};\n";
	printf "\n";
	printf "/* field markers */\n";
	foreach my $v (sort {$a <=> $b} values %field) {
		my %rev_field = reverse %field;
		my $name = uc $rev_field{$v};
		printf "#define %-10s\t0x%03X\n", $name, $v;
	}
	printf "\n";
	printf "struct {\n";
	printf "\tconst char\t*name;\n";
	printf "\tunsigned\tfields;\n";
	printf "} uop[] = {\n";
	foreach my $v (sort {$a <=> $b} values %uop_no) {
		my %rev_uop_no = reverse %uop_no;
		my $name = $rev_uop_no{$v};
		printf "\t[%2d] = {.name = %-11s .fields = 0x%03X},\n", $v, "\"$name\",", $uop_mask{$name};
	}
	printf "};\n";
	printf "\n\n";

	# ureg[]
	printf "/* register names + placeholders */\n";
	printf "#define R_PSL	%d\n", $reg{'psl'};
	printf "\n";
	printf "#define R_RN	%d\n", $reg{'<Rn>'};
	printf "#define R_RX	%d\n", $reg{'<Rx>'};
	printf "#define R_PRE	%d\n", $reg{'<pre>'};
	printf "#define R_EXE	%d\n", $reg{'<exe>'};
	printf "#define R_REG	%d\n", $reg{'<reg>'};
	printf "\n";
	printf "#define R_CNT	%d /* real registers */\n", $register_count;
	printf "\n";
	printf "const char\t*ureg[] = {\n";
	foreach my $v (sort {$a <=> $b} values %reg) {
		my %rev_reg = reverse %reg;
		printf "\t[%2d] = \"%s\",\n", $v, $rev_reg{$v};
	}
	printf "};\n";
	printf "\n\n";

	# enum preg / preg[]
	printf "/* Internal Processor Registers */\n";
	printf "enum preg {\n";
	foreach my $v (sort {$a <=> $b} values %preg) {
		my %rev_preg = reverse %preg;
		printf "\t%-10s = %2d,\n", "PR_$rev_preg{$v}", $v;
	}
	printf "};\n";
	printf "\n";
	printf "const char\t*preg[] = {\n";
	foreach my $v (sort {$a <=> $b} values %preg) {
		my %rev_preg = reverse %preg;
		printf "\t[%2d] = \"%s\",\n", $v, $rev_preg{$v};
	}
	printf "};\n";
	printf "\n\n";

	# CC macros / ccname[]
	printf "/* these 12 cc values come from 4 bits of the Bcc instructions' opcode */\n";
	printf "#define U_CC_NEG\t 2\t/* !Z       */\n";
	printf "#define U_CC_EQL\t 3\t/*  Z       */\n";
	printf "#define U_CC_GTR\t 4\t/* !(N | Z) */\n";
	printf "#define U_CC_LEQ\t 5\t/*  (N | Z) */\n";
	printf "#define U_CC_GEQ\t 8\t/* !N       */\n";
	printf "#define U_CC_LSS\t 9\t/*  N       */\n";
	printf "#define U_CC_GTRU\t10\t/* !(C | Z) */\n";
	printf "#define U_CC_LEQU\t11\t/*  (C | Z) */\n";
	printf "#define U_CC_VC\t\t12\t/* !V       */\n";
	printf "#define U_CC_VS\t\t13\t/*  V       */\n";
	printf "#define U_CC_GEQU\t14\t/* !C       */\n";
	printf "#define U_CC_LSSU\t15\t/*  C       */\n";
	printf "\n";
	printf "\n";
	printf "/* these 4 \"bonus\" cc values are only used internally in the microcode */\n";
	printf "#define U_CC_CC\t\t 1\t/* <cc>     */\n";
	printf "#define U_CC_ALWAYS\t 7\t/* uncond   */\n";
	printf "#define U_CC_CALL\t 6\t/* µcall    */\n";
	printf "#define U_CC_RET\t 0\t/* µret     */\n";
	printf "\n\n";
	printf "const char *ccname[16] = {\n";
	for (my $i = 0; $i < scalar @cc_out; $i++) {
		printf "\t[%2d] = \"%s\",\n", $i, $cc_out[$i];
	}
	printf "};\n";
	printf "\n\n";

	# enum uwidth / uuwidth[]
	printf "enum width {\n";
	foreach my $v (sort {$a <=> $b} values %widths) {
		my %rev_widths = reverse %widths;
		my $name = uc $rev_widths{$v};
		$name = 'TEMPL' if $name eq '<WIDTH>';
		printf "\t%-10s = %d,\n", "UW_$name", $v;
	}
	printf "};\n";
	printf "\n";
	printf "const char\t*uwidth[] = {\n";
	foreach my $v (sort {$a <=> $b} values %widths) {
		my %rev_widths = reverse %widths;
		printf "\t[%d] = \"%s\",\n", $v, $rev_widths{$v};
	}
	printf "};\n";
	printf "\n\n";

	# enum uflag / uflag[]
	printf "enum uflag {\n";
	foreach my $v (sort {$a <=> $b} values %flags) {
		my %rev_flags = reverse %flags;
		my $name = uc $rev_flags{$v};
		$name = 'MICRO' if $name eq 'µ';
		printf "\t%-10s = %d,\n", "U_$name", $v;
	}
	printf "};\n";
	printf "\n";
	printf "const char\t*uflag[] = {\n";
	foreach my $v (sort {$a <=> $b} values %flags) {
		my %rev_flags = reverse %flags;
		printf "\t[%d] = \"%s\",\n", $v, $rev_flags{$v};
	}
	printf "};\n";
	printf "\n\n";

	# special values/masks
	printf "/* bcc/exc µop is exc if this bit is set and bcc if it's clear */\n";
	printf "#define U_EXC_MASK\t0x8000\n";
	printf "\n";
	printf "/* imm field is <imm> instead of a normal value iff it contains this value */\n";
	printf "#define U_IMM_IMM\t0x%X\n", $imm_placeholder;
	printf "\n\n";

	# ucode[]
	printf "/* abbreviations to make ucode[] lines shorter and more readable */\n";
	printf "#define W1\t.width=UW_8,\n";
	printf "#define W2\t.width=UW_16,\n";
	printf "#define W4\t.width=UW_32,\n";
	printf "#define WW\t.width=UW_TEMPL,\n";
	printf "\n";
	printf "#define ARCH\t.flags=U_ARCH,\n";
	printf "#define U\t.flags=U_MICRO,\n";
	printf "\n";
	printf "#define LAST\t.last=1\n";
	printf "\n";
	printf "#define MAXFLOWLEN	%d\n", $maxflowlen;
	printf "\n";
	printf "struct uop {\n";
	printf "\tenum uopcode\top;\n";
	printf "\tuint32_t\timm;\n";
	printf "\tint\t\ts1, s2, dst;\n";
	printf "\tuint16_t\tutarget;\n";
	printf "\tint\t\tcc;\n";
	printf "\tint\t\tflags;  /* 0=arch, 1=µ */\n";
	printf "\tenum width\twidth;\n";
	printf "\t_Bool\t\tlast;\n";
	printf "} ucode[%d] = {\n", scalar @ucode;
	for (my $i=0; $i < scalar @ucode; $i++) {
		if ($i % 10 == 0) {
			if ($i != 0) {
				printf "\n";
			}
			printf " /* %4d */\n", $i;
		}

		print " {";

		my $instr = $ucode[$i];

		my $opname = $uop_name{$instr->{'op'}};
		$opname = 'inc'   if $opname eq '++';
		$opname = 'dec'   if $opname eq '--';
		$opname = 'index' if $opname eq '[]';
		printf ".op= U_%-7s, ", uc $opname;

my $colwidth = 10;
my $cnt=0;	# how many columns ahead are we?
foreach my $k ('s1', 's2', 'dst', 'imm', 'cc', 'utarget') {
	if (exists $instr->{$k}) {
		my $s;
#		$s = sprintf ".%-3s= %s, ", $k, $instr->{$k};
		if ($instr->{$k} > 2000) {
			$s = sprintf ".%-3s= 0x%X, ", $k, $instr->{$k};
		} else {
			$s = sprintf ".%-3s= %2d, ", $k, $instr->{$k};
		}
		print $s;
		if (length($s) > $colwidth) {
			$cnt += length($s) - $colwidth;
		}
	} else {
		my $spaces = $colwidth;
		if ($cnt > 0) {
			if ($cnt <= $colwidth) {
				$spaces -= $cnt;
			} else {
				$spaces -= $colwidth;
			}
		}

		print ' ' x $spaces;
		$cnt -= $colwidth - $spaces;
		$cnt = 0 if $cnt < 0;
	}
}
		if (exists $instr->{'width'}) {
			my @w = ("W1 ", "W2 ", "W4 ", '', '', "WW ");
			print $w[$instr->{'width'}];
		} else {
			my $colwidth = 3;
			my $spaces = $colwidth;
			if ($cnt > 0) {
				if ($cnt <= $colwidth) {
					$spaces -= $cnt;
				} else {
					$spaces -= $colwidth;
				}
			}

			print ' ' x $spaces;
			$cnt -= $colwidth - $spaces;
		}

		if (exists $instr->{'flags'}) {
			if ($instr->{'flags'} == 0) {
				print "ARCH ";
			} else {
				print "U ";
				my $colwidth = 3;
				my $spaces = $colwidth;
				if ($cnt > 0) {
					if ($cnt <= $colwidth) {
						$spaces -= $cnt;
					} else {
						$spaces -= $colwidth;
					}
				}

				print ' ' x $spaces;
				$cnt -= $colwidth - $spaces;
			}
		} else {
			my $colwidth = 5;
			my $spaces = $colwidth;
			if ($cnt > 0) {
				if ($cnt <= $colwidth) {
					$spaces -= $cnt;
				} else {
					$spaces -= $colwidth;
				}
			}

			print ' ' x $spaces;
			$cnt -= $colwidth - $spaces;
		}

		if (exists $instr->{'last'}) {
			printf "LAST";
		} else {
			my $colwidth = 4;
			my $spaces = $colwidth;
			if ($cnt > 0) {
				if ($cnt <= $colwidth) {
					$spaces -= $cnt;
				} else {
					$spaces -= $colwidth;
				}
			}

			print ' ' x $spaces;
			$cnt -= $colwidth - $spaces;
		}


		printf "}, /* line %-4d */\n", $lineno[$i];
	}
	printf "};\n";
	print "\n";
	print "#undef W1\n";
	print "#undef W2\n";
	print "#undef W4\n";
	print "#undef WW\n";
	print "#undef ARCH\n";
	print "#undef U\n";
	print "#undef LAST\n";
	print "\n";
	print "\n";

	# µcode labels
	printf "/* microcode labels -- exceptions (used by sim.c for some µops) */\n";
	foreach my $s (sort keys(%lbls)) {
		if ($s =~ /^-exc/i) {
			printf "#define LBL%-30s  %4d\n", uc clabel($s), $lbls{$s};
		}
	}
	printf "\n";
	printf "/* microcode labels -- imm/reg/mem read/write (used by fragments.h) */\n";
	# #define's for all the special labels, alphabetically
	#  first OP*, then EXC*, then everything else.
	foreach my $s (sort keys(%lbls)) {
		if ($s =~ /^-.*(imm|reg|mem)/i) {
			printf "#define LBL%-30s  %4d\n", uc clabel($s), $lbls{$s};
		}
	}
	printf "\n";
	printf "/* microcode labels -- addressing modes (used by op-dis.h/operands.spec) */\n";
	foreach my $s (sort keys(%lbls)) {
		if ($s =~ /^-addr/i) {
			printf "#define LBL%-30s  %4d\n", uc clabel($s), $lbls{$s};
		}
	}
	printf "\n";
	printf "/* microcode labels -- branch (used by analyze.h) */\n";
	foreach my $s (sort keys(%lbls)) {
		if ($s =~ /^-branch/i) {
			printf "#define LBL%-30s  %4d\n", uc clabel($s), $lbls{$s};
		}
	}
	printf "\n\n";

	# label names
	printf "struct {\n";
	printf "\tconst char\t*name;\n";
	printf "\tint\t\tvalue;\n";
	printf "} ulabels[] = {\n";
	foreach my $s (sort keys(%lbls)) {
		printf "  {.name=\"%s\",  .value=%s},\n", $s, $lbls{$s};
	}
	printf "};\n";
	printf "\n\n";

	# ustart[512]
	printf "/* microcode labels -- instructions */\n";
	print "unsigned ustart[512] = {\n";
	for (my $op = 0; $op < 512; $op++) {
		if ((($op & 0xF) == 0x0) && ($op != 0)) {
			print "\n";
		}

		if ($op < 256) {
			printf "/*    %02X */ ", $op;
		} else {
			printf "/* FD %02X */ ", $op - 0x100;
		}

		if ($mne[$op] eq "") {
			# reserved instruction
			printf "LBL_EXC_RESERVED,\n";
			next;
		}

		if (exists $lbls{$mne[$op]}) {
			printf "%4d,              /* %-8s */\n", $lbls{$mne[$op]}, $mne[$op];
			next;
		}

		printf "LBL_EXC_RESERVED,  /* %-8s */\n", $mne[$op];
	}
	print "};\n";

	printf "\n\n";

	print "#endif /* VAX_UCODE__H */\n";
	printf "\n\n";
}


sub write_stats() {
	# summary of µcode stats
	#
	# histograms
	#
	# output instructions not yet implemented
	#
	# map of µcode with a char per µop, indicate labels
	#   1  µpre
	#   2  µpost
	#   C  µcore
	#   S  µspecial
	#   * more than one kind of label
	#   .  no label

	# map of µcode with type indications
	# map-expressions?  arithmetic on labels?
	# Perl code that gets collected during assembly and run during this
	# stage?
	# 'printf "...", $xxx{'lbl-xxx'} - $xxx{lbl-xxx}

	printf "\n";
	printf "\n";
	printf "/******************\n";
	printf "\n";
	printf "size:         %4d µops\n", scalar @ucode;
	printf "longest flow: %4d µops\n", $maxflowlen;
	printf "\n";
	printf "\n";
	printf "Flow length x count:\n";
	foreach my $len (sort {$b <=> $a} keys %flowlens) {
		# append the first n names of the flows?

		printf "       %4d    %4d\n",  $len, $flowlens{$len};
	}
	printf "\n";

	printf "Use count for special labels, by name:\n";
	foreach my $s (sort keys %lbls) {
		if ($s !~ /^-/) {
			next;
		}
		if (exists $lbl_use{$s}) {
			printf "  %-30s  used %3d times.\n", $s, $lbl_use{$s};
		}
	}
	print "\n";
	print "Unused special labels:\n";
	foreach my $s (sort keys %lbls) {
		if ($s !~ /^-/) {
			next;
		}
		if (not exists $lbl_use{$s}) {
			printf "  %s\n", $s;
		}
	}
	print "\n";

	print "Labels by name:\n";
	foreach my $s (sort keys(%lbls)) {
		if ($s =~ /^-/) {
			printf "%-30s %4d\n", uc $s, $lbls{$s};
		} else {
			printf "%-30s %4d\n", uc $s, $lbls{$s};
		}
	}



	printf "\n";
	print "Labels by target:\n";
	# FIXME -- print delta from previous target
	foreach my $s (sort {$lbls{$a} <=> $lbls{$b}} keys(%lbls)) {
		if ($s =~ /^-/) {
			printf "%-30s %4d\n", uc $s, $lbls{$s};
		} else {
			printf "%-30s %4d\n", uc $s, $lbls{$s};
		}
	}

	printf "\n";

	printf "Unimplemented instructions:\n";
	foreach my $op (sort {$mne[$a] cmp $mne[$b]} 0..511) {
#	for (my $op = 0; $op < 512; $op++) {

		if ($mne[$op] eq "") {
			# reserved instruction
			next;
		}

		if (exists $lbls{$mne[$op]}) {
			# implemented instruction
			next;
		}

		if ($op < 256) {
			printf "  %-8s     %02X\n", $mne[$op], $op;
		} else {
			printf "  %-8s  FD %02X\n", $mne[$op], $op - 0x100;
		}
	}

	printf "******************/\n";
	printf "\n";
}

###

read_spec();
read_ucode();
patch_jumps();
check_ucode();
write_tables();
write_stats();
