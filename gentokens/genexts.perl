#!/usr/bin/perl -w
use strict;
use Getopt::Long;

my $outheader = '';
GetOptions('out-header' => \$outheader);

my %glexts = ();
my %glxexts = ();
while (<>)
{
    if (/^#ifndef (GL_[0-9A-Z]+_\w+)/
        && !/^#ifndef GL_VERSION_[0-9_]+/)
    {
        $glexts{$1} = 1;
    }
    if (/^#ifndef (GLX_[0-9A-Z]+_\w+)/
        && !/^#ifndef GLX_VERSION_[0-9_]+/)
    {
        $glxexts{$1} = 1;
    }
}

if ($outheader)
{
    print "/* Generated at ", scalar(localtime), " by $0. Do not edit. */\n";
    print <<EOF
#ifndef BUGLE_SRC_GLEXTS_H
#define BUGLE_SRC_GLEXTS_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
EOF
;

    my $i = 0;
    for my $e (keys %glexts)
    {
        print "#define BUGLE_" . $e . " " . $i . "\n";
        $i++;
    }
    print "\n";
    print "#define BUGLE_GLEXT_COUNT " . $i . "\n";
    print "extern const char * const bugle_glext_names[BUGLE_GLEXT_COUNT];\n";
    print "#endif /* !BUGLE_SRC_GLEXTS_H */\n";
}
else
{
    print "/* Generated at ", scalar(localtime), " by $0. Do not edit. */\n";
    print <<EOF
#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "src/glexts.h"

EOF
;
    print "const char * const bugle_glext_names[BUGLE_GLEXT_COUNT] =\n";
    print "{\n";
    print join(",\n", map { '    "' . $_ . '"' } keys %glexts), "\n";
    print "};\n";
}
