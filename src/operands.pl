#!/usr/bin/perl

# Copyright 2018  Peter Lund <firefly@vax64.dk>
#
# Licensed under GPL v2.
#
# ---
#
# Generate operand handling code for asm/dis/sim:
#
#   src/operands.pl --asm < src/operands.spec > src/op-asm.h
#   src/operands.pl --dis < src/operands.spec > src/op-dis.h
#   src/operands.pl --sim < src/operands.spec > src/op-sim.h
#   src/operands.pl --val < src/operands.spec > src/op-val.h
#
#   (the last line is for operand validation)
#
# There's also (to check the parser):
#
#   src/operands.pl --dump < src/operands.spec
#
# And (to check the input file):
#
#   src/operands.pl --check < src/operands.spec
#

use strict;
use warnings;

use POSIX qw(strftime);

########
#
# read file

# section name => [case]*
#
# case = [byte]+ [data] [@check-function] action
#
# byte = mask, pattern, [field, [convert-function]]
# data = name, width
#
# byte:
#   pattern = string (01xxx etc)
#   field   = string
#   function = name of function to call (to handle immediate fields and fp)
#              ignored in [asm]
#
# data:
#   name = string
#   width= (8|16|32|width)	-- width is 1/2/4/8/16 bytes
#
# check-function = name of function to call to verify immediate fields
#
# action   = unparsed string -- the format is section-dependent
#
# TODO allow list of actions, each action having its own guard function
# TODO '_' in patterns (don't care bits, no capture)

my %file = ();


sub read_file() {
	my $section = undef;
	while (<>) {
		# kill trailing \n
		chomp;

		my $line = $_;

		# section header?
		if (/^\[(\S+)\]$/) {
#			if (defined $section) {
#printf "end of %s, %d elmts\n", $section, scalar @{$file{$section}};
#			}

			$section = $1;
			next;
		}

		# comment line (can't have anything else on the line)
		s/^\s*#.*$//;

		# empty line?
		s/^\s+//;
		s/\s+$//;
		if ($_ eq "") {
			next;
		}

		my @bytes = ();
		# match 'byte' patterns
		#  [ \t]* doesn't confuse the gedit syntax highlighter, \s* does
		while (s/^([01x_]{8})(?:=([a-zA-Z_][a-zA-Z0-9_]*)(?:\/([a-zA-Z0-9_]+))?)?[ \t]*//) {
			#  --------      ----------------------       -------------
			my $pattern = $1;
			my $field   = $2;
			my $fun     = $3;

			if (($pattern =~ /x/) && !defined $field) {
				printf STDERR "pattern without field, line $..\n";
				printf STDERR " pattern |%s|\n", $pattern;
				exit 1;
			}
			if (defined $field && $pattern !~ /x/) {
				printf STDERR "field without pattern, line $..\n";
				printf STDERR " field |%s|\n", $field;
				exit 1;
			}

			push @bytes, {'pattern' => $pattern,
			              'field'   => $field // '',
			              'fun'     => $fun   // ''};
		}

		my $data;
		my $width;
		# match 'data'
		#  [ \t]* doesn't confuse the gedit syntax highlighter, \s* does
		if (s/^[ \t]*<([a-zA-Z_]+)(?::(8|16|32|width))?>[ \t]*//) {
			#      ----------      -------------
			$data  = $1;
			$width = $2;
		}

		my $checkfun;
		# @check-function
		#  [ \t]* doesn't confuse the gedit syntax highlighter, \s* does
		if (s/^[ \t]*@([a-zA-Z_]+)[ \t]*//) {
			#      ----------
			$checkfun = $1;
		}

		my $action;
		# remainder is 'action'
		#  [ \t]* doesn't confuse the gedit syntax highlighter, \s* does
		if (m/^\|[ \t]*(.*?)[ \t]*$/) {
			#       ---
			$action = $1;
		} elsif ($_ ne '') {
			printf STDERR "Parse error on line $.\n";
			printf STDERR "  original line: |$line|\n";
			printf STDERR "  remaining:     |$_|\n";
			exit 1;
		}
		push @{$file{$section}},
			   {'bytes'    => \@bytes,
		            'data'     => $data,     'width' => $width,
		            'checkfun' => $checkfun,
		            'action'   => $action};
	}

#	if (defined $section) {
#printf "End -- %s %d elmts\n", $section, scalar @{$file{$section}};
#	}
}



sub dump_file() {
	foreach my $section (sort keys (%file)) {
		printf "[$section] -- %d elmts\n", scalar @{$file{$section}};

		my @sec = @{$file{$section}};
		foreach my $case (@sec) {
			# bytes
			my @bytes = @{$case->{'bytes'}};
			my $s = '';
			for (my $i=0; $i < scalar @bytes; $i++) {
				my %byte = %{$bytes[$i]};
				my $pattern = $byte{'pattern'};
				my $field   = $byte{'field'};
				my $fun     = $byte{'fun'};

				$s .= $pattern;
				if ($field ne '') {
					$s .= "=$field";
					if ($fun ne '') {
						$s .= "/$fun";
					}
				} else {
					$s .= "   "
				}

				$s .= " ";
			}

			# immediate fields
			if (defined $case->{'data'}) {
				my $data  = $case->{'data'};
				my $width = $case->{'width'};

				$s .= "<$data:$width> ";
			}

			# check function
			my $checkfun = '';
			if (defined $case->{'checkfun'}) {
				$checkfun = "@" . $case->{'checkfun'};
			}

			# action
			my $action = '';
			if (defined $case->{'action'}) {
				$action = $case->{'action'};
			}

			printf "%-40s %-8s| %s\n", $s, $checkfun, $action;
		}
		print "\n";
	}
}


# FIXME: check syntax at the same time?
sub output_file() {
	my $section = undef;
	while (<>) {
		# kill trailing \n
		chomp;

		my $line = $_;

		# section header?
		if (/^\[(\S+)\]$/) {
#			if (defined $section) {
#printf "end of %s, %d elmts\n", $section, scalar @{$file{$section}};
#			}

			print "$_\n";
			$section = $1;
			next;
		}

		# comment line (can't have anything else on the line)
		s/^\s*#.*$//;

		# empty line?
		s/^\s+//;
		s/\s+$//;
		if ($_ eq "") {
			print "$line\n";
			next;
		}

		if ($section =~ /^(asm|dis)/) {
			if (/^(.*?)\|(.*)$/) {
				my ($pattern, $action) = ($1, $2);

				$action =~ s/_//g;
				$action =~ s/<[a-zA-Z0-9_]+:pcrel>/reladdr/g;
				$action =~ s/<[a-zA-Z0-9_]+:addr>/absaddr/g;
				$action =~ s/<([a-zA-Z0-9_]+):([a-zA-Z0-9_]+)>/$1/g;
				$action =~ s/<width>/W/g;
				print "$pattern|$action\n";
			} else {
				print "** $line\n";
			}
		} else {
			print "$line\n";
		}
	}
}



########
#
# verify patterns -- the patterns/fields have to be the same in all sections
#                    (except [val])

sub verify_file() {
	print "Reported differences between sections are not necessarily errors.\n";
	print "Cases and bytes are 0-indexed.\n";
	print "\n";

	# get the section names
	my @secnames = sort keys (%file);

	# check that the number of cases are the same in all sections
	for (my $i=1; $i < scalar @secnames; $i++) {
		if ($secnames[$i] eq 'val') {
			next;
		}
		if (scalar @{$file{$secnames[ 0]}} !=
		    scalar @{$file{$secnames[$i]}}) {
			printf STDERR "[%s] has %d entries, [%s] has %d entries.\n",
				$secnames[ 0], scalar @{$file{$secnames[ 0]}},
				$secnames[$i], scalar @{$file{$secnames[$i]}};
			exit 1;
		}
	}

	# compare the first section with the remaining sections
	# for each case:
	#   for each byte:
	#      cmp pattern, field, function
	#
	#   cmp data (name/width)
	#   cmp check-function
	#   ignore action string
	for (my $i=1; $i < scalar @secnames; $i++) {
		if ($secnames[$i] eq 'val') {
			next;
		}
		my @sec0 = @{$file{$secnames[ 0]}};
		my @seci = @{$file{$secnames[$i]}};
		for (my $j=0; $j < scalar @sec0; $j++) {
			# cmp bytes
			my @bytes0 = @{$sec0[$j]->{'bytes'}};
			my @bytesi = @{$seci[$j]->{'bytes'}};

			# same number of opspec bytes?
			if (scalar @bytes0 != scalar @bytesi) {
				printf STDERR "Different byte count.\n";
				printf STDERR " [%s]#%d = %d, [%s]#%d = %d.\n",
					$secnames[ 0], $j, scalar @bytes0,
					$secnames[$i], $j, scalar @bytesi;
#				exit 1;
			}

			for (my $k=0; $k < scalar @bytes0; $k++) {
				my $pattern0 = $bytes0[$k]->{'pattern'};
				my $patterni = $bytesi[$k]->{'pattern'};
				my $field0   = $bytes0[$k]->{'field'};
				my $fieldi   = $bytesi[$k]->{'field'};
				my $fun0     = $bytes0[$k]->{'fun'};
				my $funi     = $bytesi[$k]->{'fun'};

				if (($pattern0 ne $patterni) ||
				    ($field0 ne $fieldi) ||
				    ($fun0 ne $funi)) {
					printf STDERR "Different bytes.\n";
					printf STDERR " [%s]#%d.%d = %s=%s/%s.\n",
						$secnames[ 0], $j, $k, $pattern0, $field0, $fun0;
					printf STDERR " [%s]#%d.%d = %s=%s/%s.\n",
						$secnames[$i], $j, $k, $patterni, $fieldi, $funi;
					print STDERR "\n";
#					exit 1;
				}
			}


			# cmp data (name/width)
			my $data0  = $sec0[$j]->{'data'}  // '';
			my $datai  = $seci[$j]->{'data'}  // '';
			my $width0 = $sec0[$j]->{'width'} // '';
			my $widthi = $seci[$j]->{'width'} // '';
			if (($data0 ne $datai) || ($width0 ne $widthi)) {
				printf STDERR "Different data.\n";
				printf STDERR " [%s]#%d = <%s:%s>, [%s]#%d = <%s:%s>.\n",
					$secnames[ 0], $j, $data0, $width0,
					$secnames[$i], $j, $datai, $widthi;
				print STDERR "\n";
#				exit 1;
			}

			# cmp check-function
			my $checkfun0 = $sec0[$j]->{'checkfun'} // '';
			my $checkfuni = $seci[$j]->{'checkfun'} // '';
			if ($checkfun0 ne $checkfuni) {
				printf STDERR "Different check-funs.\n";
				printf STDERR " [%s]#%d = '%s', [%s]#%d = '%s'.\n",
					$secnames[ 0], $j, $sec0[$j]->{'checkfun'},
					$secnames[$i], $j, $seci[$j]->{'checkfun'};
				print STDERR "\n";
#				exit 1;
			}
		}

	}
}



# generate mask/val from pattern
#
#               mask  val
#  00xxxxxx => (0xC0, 0x00)
#  10011111 => (0xFF, 0x9F)
#  1001xxxx => (0xF0, 0x90)
#  1001____ => (0xF0, 0x90)
sub pattern($) {
	my ($s) = @_;

	# always 8 chars, only [01x]
	#
	# ($mask, $val) = (0, 0);
	# loop through, left to right
	#   0: $mask <-- 1, $val <-- 0
	#   1: $mask <-- 1, $val <-- 1
	#   x: $mask <-- 0, $val <-- 0
	#   _: $mask <-- 0, $val <-- 0
	#
	my ($mask, $val) = (0, 0);
	foreach my $ch (split('', $s)) {
		if      ($ch eq '0') {
			$mask = $mask*2 + 1;
			$val  = $val *2 + 0;
		} elsif ($ch eq '1') {
			$mask = $mask*2 + 1;
			$val  = $val *2 + 1;
		} else {  # 'x', '_'
			$mask = $mask*2 + 0;
			$val  = $val *2 + 0;
		}
	}

	($mask, $val);
}


########
#
# asm

sub handle_asm_syntax($) {
	my ($syntax) = @_;

	printf "/* encode a single operand from a buffer to a string\n";
	printf "\n";
	printf "   (uses the transactional recursive descent parser)\n";
	printf "\n";
	printf "   b[]     filled in by function\n";
	printf "   pc      address of operand\n";
	printf "   width   operand size\n";
	printf "   ifp     integer/fp type of operand\n";
	printf "   ---\n";
	printf "   positive: how many bytes the operand consumed\n";
	printf "   -1: reserved addressing mode\n";
	printf "   <-1, 0 and >9 are illegal return values\n";
	printf " */\n";
	printf "STATIC int op_asm_%s(uint8_t b[MAX_OPLEN], uint32_t pc, int width, enum ifp ifp)\n", $syntax;
	printf "{\n";
	printf "\tstruct fields\tfields = {.Rn=0};\n";
	printf "\n";
	printf "\tparse_skipws();\t/* parse_chx() doesn't skip whitespace */\n";
	printf "\n";

	# loop through each case
	foreach my $case (@{$file{'asm.' . $syntax}}) {
		my @bytes = @{$case->{'bytes'}};

		# comment so we can see what pattern is being matched
		print "\t/* ";
		for (my $i=0; $i < scalar @bytes; $i++) {
			my %byte = %{$bytes[$i]};
			my $pattern = $byte{'pattern'};
			print "$pattern ";
		}
		if (defined $case->{'data'}) {
			my $data  = $case->{'data'};
			my $width = $case->{'width'};

			print "<$data:$width> ";
		}
		print "*/\n";


		my $action = $case->{'action'};

		# trim whitespace
		$action =~ s/^\s+//;
		$action =~ s/\s+$//;

		# FIXME action lists w/ guards
		printf "\tparse_begin();\n";
		{
			# Pattern language:
			#
			# 1) where is whitespace allowed?  indicate w/ '_'
			# 2) field type (regname, immediate, 1/2/4/8) (= name of parser function)
			# 3) field name in struct
			#
			# '%' fields are skipped (can't be encoded by assembler)
			#
			# Examples:
			#   "(Rn)[Rx]"   "(_<Rn:reg>_)_[_<Rx:reg>_]"  "[_<Rn:reg>_+_<Rx:reg>_*_<width>_]"
			#
			# fname:type are compiled to:
			#   parse_type(fields.fname, width, ifp);
			#
			# width is compiled to:
			#   parse_width(width);
			#
			# '_' is compiled to:
			#   parse_skipws();
			#
			# everything else is compiled to:
			#   parse_chx(..);

# FIXME allow (but ignore) space in actions
			my $s = $action;
			while ($s ne '') {
				if      ($s =~ s/^_//) {
					printf "\tparse_skipws();\n";
				} elsif ($s =~ s/^<([a-zA-Z0-9_]+):([a-zA-Z0-9_]+)>//) {
					printf "\tparse_%s(&fields.%s, pc, width, ifp);\n", $2, $1;
				} elsif ($s =~ s/^<width>//) {
					printf "\tparse_width(width);\n";
				} else {
					printf "\tparse_chx('%s');\n", substr($s, 0, 1);
					$s =~ s/^.//;
				}
			}
		}


		# parsed ok?
		printf "\tif (parse_ok) {\n";
		printf "\t\tparse_commit();\n";
		printf "\n";

		# check validity
		if (defined $case->{'checkfun'}) {
			printf "\t\tif (!check_%s(fields, width))\n", $case->{'checkfun'};
			printf "\t\t\treturn -1; /* reserved addressing mode */\n";
			printf "\n";
		}

		# convert to bytes according to pattern/data/fields!

		# opspec bytes
		for (my $i=0; $i < scalar @bytes; $i++) {
			my %byte = %{$bytes[$i]};
			my $pattern = $byte{'pattern'};
			my ($mask, $val) = pattern($pattern);

			if ($pattern =~ /x/ && !defined $byte{'field'}) {
				fprintf STDERR "Error -- pattern |%s| has no field\n",
					$pattern;
				exit 1;
			}

			if ($mask == 0xFF) {
				printf "\t\tb[$i] = 0x%02X;\n", $val;
			} else {
				printf "\t\tb[$i] = 0x%02X | (fields.%s & ~0x%02X);\n", $val, $byte{'field'}, $mask;
			}
		}
		my $bytecnt = scalar @bytes;

		# is there an immediate field?
		if (defined $case->{'data'}) {
			my $data  = $case->{'data'};
			my $width = $case->{'width'};

			if      ($width eq '8') {
				printf "\t\tb[%d] = BYTE(fields.%s, 0);\n", $bytecnt++, $data;
			} elsif ($width eq '16') {
				printf "\t\tb[%d] = BYTE(fields.%s, 0);\n", $bytecnt++, $data;
				printf "\t\tb[%d] = BYTE(fields.%s, 1);\n", $bytecnt++, $data;
			} elsif ($width eq '32') {
				printf "\t\tb[%d] = BYTE(fields.%s, 0);\n", $bytecnt++, $data;
				printf "\t\tb[%d] = BYTE(fields.%s, 1);\n", $bytecnt++, $data;
				printf "\t\tb[%d] = BYTE(fields.%s, 2);\n", $bytecnt++, $data;
				printf "\t\tb[%d] = BYTE(fields.%s, 3);\n", $bytecnt++, $data;

			} elsif ($width eq 'width') {
				printf "\t\tswitch (width) {\n";
				printf "\t\tcase 1: b[%2d] = BYTE(fields.%s.val[0], 0); break;\n", $bytecnt++, $data;
				$bytecnt -= 1;
				printf "\t\tcase 2: b[%2d] = BYTE(fields.%s.val[0], 0);\n", $bytecnt++, $data;
				printf "\t\t        b[%2d] = BYTE(fields.%s.val[0], 1); break;\n", $bytecnt++, $data;
				$bytecnt -= 2;

				printf "\t\tcase 4: b[%2d] = BYTE(fields.%s.val[0], 0);\n", $bytecnt++, $data;
				printf "\t\t        b[%2d] = BYTE(fields.%s.val[0], 1);\n", $bytecnt++, $data;
				printf "\t\t        b[%2d] = BYTE(fields.%s.val[0], 2);\n", $bytecnt++, $data;
				printf "\t\t        b[%2d] = BYTE(fields.%s.val[0], 3); break;\n", $bytecnt++, $data;
				$bytecnt -= 4;

				printf "\t\tcase 8: b[%2d] = BYTE(fields.%s.val[0], 0);\n", $bytecnt++, $data;
				printf "\t\t        b[%2d] = BYTE(fields.%s.val[0], 1);\n", $bytecnt++, $data;
				printf "\t\t        b[%2d] = BYTE(fields.%s.val[0], 2);\n", $bytecnt++, $data;
				printf "\t\t        b[%2d] = BYTE(fields.%s.val[0], 3);\n", $bytecnt++, $data;
				printf "\t\t        b[%2d] = BYTE(fields.%s.val[1], 0);\n", $bytecnt++, $data;
				printf "\t\t        b[%2d] = BYTE(fields.%s.val[1], 1);\n", $bytecnt++, $data;
				printf "\t\t        b[%2d] = BYTE(fields.%s.val[1], 2);\n", $bytecnt++, $data;
				printf "\t\t        b[%2d] = BYTE(fields.%s.val[1], 3); break;\n", $bytecnt++, $data;
				$bytecnt -= 8;

				printf "\t\tcase 16:b[%2d] = BYTE(fields.%s.val[0], 0);\n", $bytecnt++, $data;
				printf "\t\t        b[%2d] = BYTE(fields.%s.val[0], 1);\n", $bytecnt++, $data;
				printf "\t\t        b[%2d] = BYTE(fields.%s.val[0], 2);\n", $bytecnt++, $data;
				printf "\t\t        b[%2d] = BYTE(fields.%s.val[0], 3);\n", $bytecnt++, $data;
				printf "\t\t        b[%2d] = BYTE(fields.%s.val[1], 0);\n", $bytecnt++, $data;
				printf "\t\t        b[%2d] = BYTE(fields.%s.val[1], 1);\n", $bytecnt++, $data;
				printf "\t\t        b[%2d] = BYTE(fields.%s.val[1], 2);\n", $bytecnt++, $data;
				printf "\t\t        b[%2d] = BYTE(fields.%s.val[1], 3);\n", $bytecnt++, $data;
				printf "\t\t        b[%2d] = BYTE(fields.%s.val[2], 0);\n", $bytecnt++, $data;
				printf "\t\t        b[%2d] = BYTE(fields.%s.val[2], 1);\n", $bytecnt++, $data;
				printf "\t\t        b[%2d] = BYTE(fields.%s.val[2], 2);\n", $bytecnt++, $data;
				printf "\t\t        b[%2d] = BYTE(fields.%s.val[2], 3);\n", $bytecnt++, $data;
				printf "\t\t        b[%2d] = BYTE(fields.%s.val[3], 0);\n", $bytecnt++, $data;
				printf "\t\t        b[%2d] = BYTE(fields.%s.val[3], 1);\n", $bytecnt++, $data;
				printf "\t\t        b[%2d] = BYTE(fields.%s.val[3], 2);\n", $bytecnt++, $data;
				printf "\t\t        b[%2d] = BYTE(fields.%s.val[3], 3); break;\n", $bytecnt++, $data;
				$bytecnt -= 16;

				printf "\t\tdefault:\n";
				printf "\t\t        assert(0);\n";
				printf "\t\t}\n";
			} else {
				printf STDERR "Illegal data width (<%s:%s>)\n",
					$data, $width;
				exit 1;
			}
		}


		# and we're done!
		if (defined $case->{'width'} && ($case->{'width'} eq 'width')) {
			printf "\t\treturn $bytecnt + width;\n";
		} else {
			printf "\t\treturn $bytecnt;\n";
		}
		printf "\t} else {\n";
		printf "\t\tparse_rollback();\n";
		printf "\t}\n";

		printf "\n";
	}

	printf "\n";
	printf "\treturn -1;\n";
	printf "}\n";
}


sub handle_asm() {
	printf "/* generated by operands.pl from operands.spec -- %s */\n", strftime("%Y-%m-%d %H:%M:%S", localtime);
	printf "\n";

	my $cnt=0;
	foreach my $section (sort keys (%file)) {
		if ($section =~ /^asm\.(.*)/) {
			my $syntax = $1;

			if ($cnt != 0) {
				print "\n\n";
			}
			handle_asm_syntax($syntax);
			$cnt += 1;
		}
	}
}



########
#
# dis

sub handle_dis_syntax($) {
	my ($syntax) = @_;

	printf "/* decode a single operand from a buffer to a string\n";
	printf "\n";
	printf "   b[]      9 bytes from the instruction stream\n";
	printf "   pc       the address of the operand\n";
	printf "   width    1/2/4/8, depending on the instruction\n";
	printf "   ifp      int or fp (f/d/g/h)\n";
	printf "   ---\n";
	printf "   positive: how many bytes the operand consumed\n";
	printf "   -1: reserved addressing mode\n";
	printf "   <-1, 0 and >9 are illegal return values\n";
	printf " */\n";
	printf "STATIC struct dis_ret op_dis_%s(uint8_t b[MAX_OPLEN], uint32_t pc, int width, enum ifp ifp)\n", $syntax;
	printf "{\n";

	# loop through the cases for 'sim' section
	foreach my $case (@{$file{'dis.' . $syntax}}) {
		my @bytes = @{$case->{'bytes'}};

		my $bytecnt = scalar @bytes;

		# comment so we can see what pattern is being matched
		print "\t/* ";
		for (my $i=0; $i < scalar @bytes; $i++) {
			my %byte = %{$bytes[$i]};
			my $pattern = $byte{'pattern'};
			print "$pattern ";
		}
		if (defined $case->{'data'}) {
			my $data  = $case->{'data'};
			my $width = $case->{'width'};

			print "<$data:$width> ";
		}
		print "*/\n";

		# match byte by byte
		for (my $i=0; $i < scalar @bytes; $i++) {
			my %byte = %{$bytes[$i]};
			my $pattern = $byte{'pattern'};

			printf "\t%sif ((b[$i] & 0x%02X) == 0x%02X) {\n",
				"\t" x $i, pattern($pattern);
		}

		my $indent = "\t" x (1 + scalar @bytes);

		printf "%sstruct dis_ret\tret;\n", $indent;
		printf "%sunsigned\tidx = 0;\n", $indent;
		printf "\n";

		# struct fields?
		if (defined $case->{'data'}) {
			printf "%sstruct fields\tfields;\n", $indent;
			goto done;
		}
		for (my $i=0; $i < scalar @bytes; $i++) {
			my %byte = %{$bytes[$i]};
			my $field   = $byte{'field'};

			if ($field ne '') {
				printf "%sstruct fields\tfields;\n", $indent;
				goto done;
			}
		}
done:

		# field captures
		for (my $i=0; $i < scalar @bytes; $i++) {
			my %byte = %{$bytes[$i]};
			my $pattern = $byte{'pattern'};
			my $field   = $byte{'field'};
			my $fun     = $byte{'fun'};

			# if there is a field, capture it
			if ($field ne '') {
				my ($mask) = pattern($pattern);

				printf "%sfields.%s = b[$i] & ~0x%02X;\n",
					$indent, $field, $mask;
			}

			# convert the field?
			if ($fun ne '') {
				printf "%s%s(&fields, width, ifp);\n",
					$indent, $fun;
			}
		}

		# is there an immediate field?
		if (defined $case->{'data'}) {
			my $data  = $case->{'data'};
			my $width = $case->{'width'};

			if      ($width eq '8') {
				printf "%sfields.%s = B(b[%d]);\n", $indent, $data, $bytecnt;
				$bytecnt += 1;
			} elsif ($width eq '16') {
				printf "%sfields.%s = W(b[%d]);\n", $indent, $data, $bytecnt;
				$bytecnt += 2;
			} elsif ($width eq '32') {
				printf "%sfields.%s = L(b[%d]);\n", $indent, $data, $bytecnt;
				$bytecnt += 4;
			} elsif ($width eq 'width') {
				printf "%sswitch (width) {\n", $indent;
				printf "%scase 1: fields.%s.val[0] = B(b[%2d]); break;\n", $indent, $data, $bytecnt;
				printf "%scase 2: fields.%s.val[0] = W(b[%2d]); break;\n", $indent, $data, $bytecnt;
				printf "%scase 4: fields.%s.val[0] = L(b[%2d]); break;\n", $indent, $data, $bytecnt;
				printf "%scase 8: fields.%s.val[0] = L(b[%2d]); /* lo */\n", $indent, $data, $bytecnt;
				printf "%s        fields.%s.val[1] = L(b[%2d]); /* hi */\n", $indent, $data, $bytecnt+4;
				printf "%s        break;\n", $indent;
				printf "%scase 16:fields.%s.val[0] = L(b[%2d]); /* lo */\n", $indent, $data, $bytecnt;
				printf "%s        fields.%s.val[1] = L(b[%2d]); /* hi */\n", $indent, $data, $bytecnt+4;
				printf "%s        fields.%s.val[2] = L(b[%2d]); /* lo */\n", $indent, $data, $bytecnt+8;
				printf "%s        fields.%s.val[3] = L(b[%2d]); /* hi */\n", $indent, $data, $bytecnt+12;
				printf "%s        break;\n", $indent;
				printf "%sdefault:\n", $indent;
				printf "%s        assert(0);\n", $indent;
				printf "%s}\n", $indent;
			} else {
				printf STDERR "Illegal data width (<%s:%s>)\n",
					$data, $width;
				exit 1;
			}
		}

		# check validity
		if (defined $case->{'checkfun'}) {
			printf "\n";
			printf "%sif (!check_%s(fields, width))\n", $indent, $case->{'checkfun'};
			printf "%s\treturn -1; /* reserved addressing mode */\n", $indent;
		}
		printf "\n";

		my $action = $case->{'action'};

		# action code = template w/ fields (and formatting functions)

		$action =~ s/^\s+//;
		$action =~ s/\s+$//;

# FIXME allow (but ignore) space in actions
		{
			# pattern language:
			#
			#  <fname:convfunc>	--> convfunc(fields.fname)
			#  <width>		--> '1'/'2'/'4'/'8'
			#  everything else	--> direct output

			my $s = $action;
			while ($s ne '') {
				if    ($s =~ s/^<width>//) {
					print  $indent, "if (idx < sizeof(ret.str))\n";
					print  $indent, "\tidx += snprintf(ret.str+idx, sizeof(ret.str)-idx,\"%d\", width);\n";
				} elsif ($s =~ s/^<([a-zA-Z0-9_]+):([a-zA-Z0-9_]+)>//) {
					#          ---------   ---------
					print  $indent, "if (idx < sizeof(ret.str))\n";
					print  $indent, "\tidx += snprintf(ret.str+idx, sizeof(ret.str)-idx, \"%s\", str_$2(fields.$1, pc, width, ifp).str);\n";
				} else {
					printf "%sret.str[idx++] = '%s';\n", $indent, substr($s, 0, 1);
					$s =~ s/^.//;
				}
			}
		}

		printf "\n";

		# terminate string
		printf "%sret.str[idx] = '\\0';\n", $indent;

		# #bytes consumed
		if (defined $case->{'width'} && ($case->{'width'} eq 'width')) {
			print $indent, "ret.cnt = $bytecnt + width;\n";
		} else {
			print $indent, "ret.cnt = $bytecnt;\n";
		}

		print $indent, "return ret;\n";

BRACES:
		# closing braces
		for (my $i=scalar @bytes-1; $i>=0; $i--) {
			print "\t", "\t" x $i, "}\n";
		}

		print "\n";
	}
	printf "\t/* no match found */\n";
	printf "\treturn (struct dis_ret) {.cnt=-1}; /* reserved addressing mode */\n";
	printf "}\n\n";
}


sub handle_dis() {
	printf "/* generated by operands.pl from operands.spec -- %s */\n", strftime("%Y-%m-%d %H:%M:%S", localtime);
	printf "\n";
	printf "struct dis_ret {\n";
	printf "\tint\tcnt;\n";
	printf "\tchar\tstr[100];\n";
	printf "};\n";
	printf "\n";

	my $cnt=0;
	foreach my $section (sort keys (%file)) {
		if ($section =~ /^dis\.(.*)/) {
			my $syntax = $1;

			if ($cnt != 0) {
				print "\n\n";
			}
			handle_dis_syntax($syntax);
			$cnt += 1;
		}
	}
}



########
#
# sim


# strip '_'
sub shortlbl($) {
	my ($s) = @_;

	$s =~ s/[_ ]//g;
	$s;
}


# '-addr-[addr]'                 => '_ADDR__ADDR_'
# '-addr-[[Rn+disp]+index(Rx)]]' => '_ADDR___RNXDISP_XINDEX_RX__'
# ...
sub clabel($) {
	my ($s) = @_;

	$s =~ s/\+/x/g;
	$s =~ s/[\[\]\(\)-]/_/g;
	$s;
}


sub handle_sim() {
	printf "/* generated by operands.pl from operands.spec -- %s */\n", strftime("%Y-%m-%d %H:%M:%S", localtime);
	printf "\n";
	printf "struct sim_ret {\n";
	printf "\tint\tcnt;\n";
	printf "\tint\tcl; /* class/label */\n";
	printf "};\n";
	printf "\n";
	printf "/* decode a single operand from a buffer to classification/label/fields\n";
	printf "\n";
	printf "   (classification as %%, I, R, M)\n";
	printf "\n";
	printf "   class_label is CLASS_IMM, CLASS_REG or a microcode label (\"CLASS_ADDR\").\n";
	printf "\n";
	printf "   ---\n";
	printf "   cnt >  0: how many bytes the operand consumed\n";
	printf "   cnt = -1: reserved addressing mode\n";
	printf "   cnt < -1, cnt = 0 and cnt > 9 are illegal\n";
	printf "   cl is only valid if cnt > 0\n";
	printf " */\n";
	printf "STATIC struct sim_ret op_sim(uint8_t b[MAX_OPLEN], struct fields *fields, int width, enum ifp ifp)\n";
	printf "{\n";

	# loop through the cases for 'sim' section
	#   write a matcher for the case using first byte
	foreach my $case (@{$file{'sim'}}) {
		my @bytes = @{$case->{'bytes'}};

		my $bytecnt = scalar @bytes;

		# comment so we can see what pattern is being matched
		print "\t/* ";
		for (my $i=0; $i < scalar @bytes; $i++) {
			my %byte = %{$bytes[$i]};
			my $pattern = $byte{'pattern'};
			print "$pattern ";
		}
		if (defined $case->{'data'}) {
			my $data  = $case->{'data'};
			my $width = $case->{'width'};

			print "<$data:$width> ";
		}
		print "*/\n";

		# match byte by byte
		for (my $i=0; $i < scalar @bytes; $i++) {
			my %byte = %{$bytes[$i]};
			my $pattern = $byte{'pattern'};

			printf "\t%sif ((b[$i] & 0x%02X) == 0x%02X) {\n",
				"\t" x $i, pattern($pattern);
		}

		my $indent = "\t" x (1 + scalar @bytes);

		# field captures
		for (my $i=0; $i < scalar @bytes; $i++) {
			my %byte = %{$bytes[$i]};
			my $pattern = $byte{'pattern'};
			my $field   = $byte{'field'};
			my $fun     = $byte{'fun'};

			# if there is a field, capture it
			if ($field ne '') {
				my ($mask) = pattern($pattern);

				printf "%sfields->%s = b[$i] & ~0x%02X;\n",
					$indent, $field, $mask;
			}

			# convert the field?
			if ($fun ne '') {
				printf "%s%s(fields, width, ifp);\n",
					$indent, $fun;
			}
		}

		# is there an immediate field?
		if (defined $case->{'data'}) {
			my $data  = $case->{'data'};
			my $width = $case->{'width'};

			if      ($width eq '8') {
				printf "%sfields->%s = B(b[%d]);\n", $indent, $data, $bytecnt;
				$bytecnt += 1;
			} elsif ($width eq '16') {
				printf "%sfields->%s = W(b[%d]);\n", $indent, $data, $bytecnt;
				$bytecnt += 2;
			} elsif ($width eq '32') {
				printf "%sfields->%s = L(b[%d]);\n", $indent, $data, $bytecnt;
				$bytecnt += 4;
			} elsif ($width eq 'width') {
				printf "%sswitch (width) {\n", $indent;
				printf "%scase  1: fields->%s.val[0] = B(b[%2d]); break;\n", $indent, $data, $bytecnt;
				printf "%scase  2: fields->%s.val[0] = W(b[%2d]); break;\n", $indent, $data, $bytecnt;
				printf "%scase  4: fields->%s.val[0] = L(b[%2d]); break;\n", $indent, $data, $bytecnt;
				printf "%scase  8: fields->%s.val[0] = L(b[%2d]);  /* lo */\n", $indent, $data, $bytecnt;
				printf "%s         fields->%s.val[1] = L(b[%2d]);  /* hi */\n", $indent, $data, $bytecnt+4;
				printf "%s         break;\n", $indent;
				printf "%scase 16: fields->%s.val[0] = L(b[%2d]);  /* lo */\n", $indent, $data, $bytecnt;
				printf "%s         fields->%s.val[1] = L(b[%2d]);  /*    */\n", $indent, $data, $bytecnt+4;
				printf "%s         fields->%s.val[2] = L(b[%2d]);  /*    */\n", $indent, $data, $bytecnt+8;
				printf "%s         fields->%s.val[3] = L(b[%2d]);  /* hi */\n", $indent, $data, $bytecnt+12;
				printf "%s         break;\n", $indent;
				printf "%sdefault:\n", $indent;
				printf "%s        assert(0);\n", $indent;
				printf "%s}\n", $indent;
			} else {
				printf STDERR "Illegal data width (<%s:%s>)\n",
					$data, $width;
				exit 1;
			}
		}

		# check validity
		if (defined $case->{'checkfun'}) {
			printf "\n";
			printf "%sif (!check_%s(*fields, width))\n", $indent, $case->{'checkfun'};
			printf "%s\treturn -1; /* reserved addressing mode */\n", $indent;
		}

		# action code = classification + maybe a microcode label
		# check for '%' (skip)
		my $action = $case->{'action'};
		$action =~ s/^\s+//;
		$action =~ s/\s+$//;
		if ($action eq '%') {
			printf "%s/* this pattern is not allowed */\n", $indent;
			printf "%sreturn -1; /* reserved addressing mode */\n", $indent;
			printf "\n";
			goto BRACES;
		}

		# return/set classification/label
		my $cl;
		if ($action eq 'I') {
			$cl = "CLASS_IMM";
		} elsif ($action eq 'R') {
			$cl = "CLASS_REG";
		} elsif ($action =~ /^M:\s*([a-zA-Z_+\[\]\(\) -]+)$/) {
			my $lbl = $1;
#			printf "M: |%s|\n", $1;

			open(my $F, "<:encoding(UTF-8)", "src/ucode.vu") or
			  die "Could not open src/ucode.vu";

			while (<$F>) {
				if (/^-addr-([a-zA-Z_+\[\]\(\)-]+):/) {
#					printf "  LBL: |%s|\n", $1;

					if (shortlbl($1) eq shortlbl($lbl)) {
						$cl = "LBL_" . uc clabel("addr-" . $1);
						goto FOUND;
					}
				}
			}
			printf STDERR "[sim] -- invalid µlabel.\n";
			printf STDERR " |%s|%s|\n", $lbl, shortlbl($lbl);
FOUND:
			close($F);
		} else {
			printf STDERR "[sim] -- invalid class/µlabel.\n";
			printf STDERR " |%s|\n", $action;
		}


		# #bytes consumed
		if (defined $case->{'width'} && ($case->{'width'} eq 'width')) {
			print $indent, "return (struct sim_ret) {.cnt = $bytecnt + width, .cl = $cl};\n";
		} else {
			print $indent, "return (struct sim_ret) {.cnt = $bytecnt, .cl = $cl};\n";
		}

BRACES:
		# closing braces
		for (my $i=scalar @bytes-1; $i>=0; $i--) {
			print "\t", "\t" x $i, "}\n";
		}

		print "\n";
	}
	printf "\t/* no match found */\n";
	printf "\treturn (struct sim_ret) {.cnt = -1}; /* reserved addressing mode */\n";
	printf "}\n\n";
}



########
#
# val

sub handle_val() {
	printf "/* generated by operands.pl from operands.spec -- %s*/\n", strftime("%Y-%m-%d %H:%M:%S", localtime);
	printf "\n";
	printf "/* validate a single operand\n";
	printf "\n";
	printf "   ---\n";
	printf "   true    valid operand\n";
	printf "   false   \"reserved addressing mode\"\n";
	printf " */\n";
	printf "STATIC bool op_val(uint8_t b[MAX_OPLEN], int width)\n";
	printf "{\n";
	printf "\tstruct fields fields;\n";
	printf "\n";

	# loop through the cases for 'sim' section
	#   write a matcher for the case using first byte
	foreach my $case (@{$file{'val'}}) {
		my @bytes = @{$case->{'bytes'}};

		my $bytecnt = scalar @bytes;

		# comment so we can see what pattern is being matched
		print "\t/* ";
		for (my $i=0; $i < scalar @bytes; $i++) {
			my %byte = %{$bytes[$i]};
			my $pattern = $byte{'pattern'};
			print "$pattern ";
		}
		if (defined $case->{'data'}) {
			my $data  = $case->{'data'};
			my $width = $case->{'width'};

			print "<$data:$width> ";
		}
		print "*/\n";

		# match byte by byte
		for (my $i=0; $i < scalar @bytes; $i++) {
			my %byte = %{$bytes[$i]};
			my $pattern = $byte{'pattern'};

			printf "\t%sif ((b[$i] & 0x%02X) == 0x%02X) {\n",
				"\t" x $i, pattern($pattern);
		}

		my $indent = "\t" x (1 + scalar @bytes);

		# field captures
		for (my $i=0; $i < scalar @bytes; $i++) {
			my %byte = %{$bytes[$i]};
			my $pattern = $byte{'pattern'};
			my $field   = $byte{'field'};

			# if there is a field, capture it
			if ($field ne '') {
				my ($mask) = pattern($pattern);

				printf "%sfields.%s = b[$i] & ~0x%02X;\n",
					$indent, $field, $mask;
			}
		}

		# we found a match, probably an invalid operand
		if (defined $case->{'checkfun'}) {
			printf "\n";
			printf "%sif (!check_%s(fields, width))\n", $indent, $case->{'checkfun'};
			printf "%s\treturn false; /* reserved addressing mode */\n", $indent;
		} else {
			printf "%sreturn false;\n", $indent;
		}

		# closing braces
		for (my $i=scalar @bytes-1; $i>=0; $i--) {
			print "\t", "\t" x $i, "}\n";
		}

		print "\n";
	}

	# valid instruction
	printf "\treturn true; /* valid addressing mode */\n";
	printf "}\n\n";
}




########
#
# main

sub help() {
	print "./operands.pl <flag>  < input > output\n";
	print "\n";
	print "flag:\n";
	print "    --asm     generate code to assemble an operand\n";
	print "    --dis     generate code to disassemble an operand\n";
	print "    --sim     generate code to decode an operand\n";
	print "    --val     generate code to validate an operand\n";
	print "\n";
	print "    --dump    show how the sections were interpreted\n";
	print "    --check   check that [asm], [dis], and [sim] sections are in agreement\n";
	print "    --output  show the [asm] and [dis] sections with cleaned up actions\n";
	exit;
}


if ($#ARGV == -1) {
	help();
}

if      ($ARGV[0] eq "--check") {
	shift @ARGV;

	read_file();
	verify_file();
} elsif ($ARGV[0] eq "--dump") {
	shift @ARGV;

	read_file();
	dump_file();
} elsif ($ARGV[0] eq "--output") {
	shift @ARGV;
	output_file();
} elsif ($ARGV[0] eq "--asm") {
	shift @ARGV;

	read_file();
	handle_asm();
} elsif ($ARGV[0] eq "--dis") {
	shift @ARGV;

	read_file();
	handle_dis();
} elsif ($ARGV[0] eq "--sim") {
	shift @ARGV;

	read_file();
	handle_sim();
} elsif ($ARGV[0] eq "--val") {
	shift @ARGV;

	read_file();
	handle_val();
} else {
	help();
}


