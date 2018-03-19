#!/usr/bin/perl

# Copyright 2018  Peter Lund <firefly@vax64.dk>
#
# Licensed under GPL v2.

# find and report trailing whitespace

use strict;
use warnings;

my $NORM = "\033[0m";
my $BOLD = "\033[41m";	# red background

my $found = 0;

while (<>) {
	chomp;
	if (/^(.*?)(\s+)$/ && ($2 ne "\014")) {	# ignore ^L (form feed)
		$found = 1;
		print sprintf("[%-15s - %4s] ", $ARGV, $.), $1, $BOLD, $2, $NORM, "\n";
	}
} continue {
	close ARGV if eof;	# reset line numbers
}

exit 1		if $found;
