#!/usr/bin/perl -w
use strict;
use warnings;

my $dll = shift;
print "EXPORTS\n";
while (<>)
{
    if (/^#define FUNC_(egl.*) \d+/)
    {
        print "$1 = $dll.$1\n";
    }
}

__END__

=head1 Purpose

This script is used to generate a Windows .def file to be included in
libEGL.dll. It contains forwarding references to the implementations in
the main DLL. This is a temporary measure until bugle is capable of
outputting different sets of wrapper functions into different libraries,
which would be a portable solution, and also more friendly to dlopen-based
apps.

=head1 Usage

The input must be the defines.h file produced by budgie. The output is a
.def file that can be passed to the linker.
