#!/usr/bin/perl -w
use strict;

die("Missing CPP") unless defined($ENV{'CPP'});
die("Usage: $0 <header> <header>...") unless $#ARGV >= 0;

my $cpp = $ENV{'CPP'};
my $srcdir = $ENV{'srcdir'};
$srcdir =~ s/'/'\\''/g;

while (@ARGV)
{
    my $header = shift @ARGV;
    my $path = '';
    open(PREPROC, '-|', "echo '#include <$header>' | $cpp -I. -I'$srcdir' - 2>/dev/null");
    while (<PREPROC>)
    {
        if (/^# \d+ "(.*$header)"/) { $path = $1; }
    }
    close PREPROC;

    if (!$path)
    {
        # Try a few standard places
        for my $i (qw(/usr/include /usr/X11R6/include /usr/local/include /usr/include/w32api))
        {
            if (-f "$i/$header") { $path = "$i/$header"; last; }
        }
    }
    die("Could not find header $path") unless $path;
    print "\"$path\" ";
}
