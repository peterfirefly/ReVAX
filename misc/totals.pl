#!/usr/bin/perl

# filter for 'wc' output -- makes a nice horizontal line above the totals.

use strict;
use warnings;

while (<>) {
	if (/total/ and not /totals.pl/) {
		print " ", "-" x ((length $_) - 2), "\n", $_;
	} else {
		print $_;
	}
}

