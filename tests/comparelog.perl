#!/usr/bin/perl -w
use strict;

open FH, $ARGV[0] or die $!;
my @re = <FH>;
close FH;

open FH, $ARGV[1] or die $!;
my @lines = <FH>;
close FH;

while (@lines && $lines[0] !~ $re[0]) { shift @lines; }

for (my $i = 0; $i < scalar(@re); $i++)
{
    if (scalar(@lines) <= $i) { die "Line " . ($i + 1) . " missing\n"; }
    if ($lines[$i] !~ $re[$i]) { die "Line " . ($i + 1) . " does not match\n"; }
}
