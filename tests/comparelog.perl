#!/usr/bin/perl -w
use strict;

undef $/;
open FH, $ARGV[0] or die $!;
my $re = <FH>;
close FH;

open FH, $ARGV[1] or die $!;
$_ = <FH>;
close FH;

/$re/ or die "Mismatch";
