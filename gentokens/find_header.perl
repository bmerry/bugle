#!/usr/bin/perl -w
use strict;

die("Missing CPP") unless defined($ENV{'CPP'});
die("Usage: $0 <header>") unless $#ARGV >= 0;

my $cpp = $ENV{'CPP'};
my $header = $ARGV[0];
my $path = '';
open(PREPROC, '-|', "echo '#include <$header>' | $cpp -");
while (<PREPROC>)
{
    if (/^# \d+ "(.*$header)"/) { $path = $1; }
}

if (!$path)
{
    # Try a few standard places
    for my $i (qw(/usr/include /usr/X11R6/include /usr/local/include))
    {
        if (-f "$i/$header") { $path = "$i/$header"; last; }
    }
}
die("Could not find header") unless $path;
print "$path\n";
