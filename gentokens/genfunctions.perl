#!/usr/bin/perl -w
# Extracts names of OpenGL functions, together with the GL versions and
# extentions they belong to.
# There is a required option: --header types.h, where types.h contains the
# defines of the functions known to the C code. The option --out-header
# should be given to get a header file, no extra option to get the C file.
# Can all use --alias to get a list of aliases to paste into gl.bc.

use strict;
use Getopt::Long;

sub text_versions($$)
{
    my $ver = $_[0];
    my $ext = $_[1];
    
    if (defined($ver)) { $ver = '"' . $ver . '"'; }
    elsif (defined($ext)) { $ver = "NULL"; }
    else { $ver = '"GL_VERSION_1_1"'; }
    if (defined($ext)) { $ext = '"' . $ext . '"'; }
    else { $ext = "NULL"; }
    return ($ver, $ext);
}

my ($ver, $ext, $suffix, @table, %index);

# Load known function names
my $header = '';
my $outheader = '';
my $aliases = '';
GetOptions('header=s' => \$header, 'out-header' => \$outheader, 'alias' => \$aliases);
if ($header)
{
    open(H, "<$header");
    while (<H>)
    {
        if (/^#define FUNC_(\w+)\s+(\d+)/)
        {
            my ($func, $value) = ($1, $2);
            $table[$value] = [$func, undef, undef, undef];
            $index{$func} = $table[$value];
        }
    }
}

# Load input
while (<>)
{
    if (/^#ifndef (GL_VERSION_[0-9_]+)/) { $ver = $1; }
    elsif (/^#ifndef (GL_([0-9A-Z]+)_\w+)/) 
    { 
        $ext = $1;
        $suffix = $2;
    }
    elsif (/^#endif/)
    {
        $ver = $ext = $suffix = undef;
    }
    elsif ((/(gl\w+)\s*\(/
            || /^extern .+ ()(glX\w+)\s*\(/)
           && (!$header || exists($index{$1})))
    {
        my $name = $1;
        my ($t_ver, $t_ext) = text_versions($ver, $ext);
        if (!exists($index{$name})) 
        {
            $index{$name} = [$name, undef, undef, undef];
        }
        my $ref = $index{$name};
        my $base = $name;
        if (defined($suffix)) { $base =~ s/$suffix$//; }
        if (!exists($index{$base})) { $base = $name; }
        $ref->[1] = $base;
        $ref->[2] = $t_ver;
        $ref->[3] = $t_ext;
    }
}

if ($aliases)
{
    for my $i (values %index)
    {
        if ($i->[0] ne $i->[1])
        {
            # Unfortunately, there are cases where what look like
            # aliases have different signatures, and hence cannot
            # be grouped
            my %exclude = ("glColorPointerEXT" => 1,
                           "glEdgeFlagPointerEXT" => 1,
                           "glIndexPointerEXT" => 1,
                           "glNormalPointerEXT" => 1,
                           "glTexCoordPointerEXT" => 1,
                           "glVertexPointerEXT" => 1,
                           "glVertexAttribPointerNV" => 1,
                           "glHintPGI" => 1,
                           "glShaderSourceARB" => 1,
                           "glCompileShaderARB" => 1,
                           "glLinkProgramARB" => 1,
                           "glValidateProgramARB" => 1,
                           "glGetUniformLocationARB" => 1,
                           "glActiveUniformARB" => 1,
                           "glGetUniformfvARB" => 1,
                           "glGetUniformivARB" => 1,
                           "glGetShaderSourceARB" => 1
                          );
            if (!exists($exclude{$i->[0]}))
            {
                print "ALIAS ", $i->[0], " ", $i->[1], "\n";
            }
        }
    }
    exit 0;
}

print "/* Generated at ", scalar(localtime), " by $0. Do not edit. */\n";
if ($outheader)
{
    print <<'EOF'
#ifndef BUGLE_SRC_GLFUNCS_H
#define BUGLE_SRC_GLFUNCS_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "src/utils.h"

typedef struct
{
    const char *version;     /* Min core version that defines this function (NULL for extension) */
    const char *extension;   /* Extension that defines this function (NULL for core) */
} gl_function;

extern const gl_function bugle_gl_function_table[NUMBER_OF_FUNCTIONS];
        
EOF
        ;
    print "\n";
    print "#endif /* !BUGLE_SRC_GLFUNCS_H */\n";
}
else
{
    print <<'EOF'
#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <GL/gl.h>
#include <GL/glext.h>
#include "src/utils.h"
#include "src/glfuncs.h"
EOF
        ;
    print "gl_function const bugle_gl_function_table[NUMBER_OF_FUNCTIONS] =\n{\n";
    my $first = 1;
    for my $i (@table)
    {
        print ",\n" unless $first; $first = 0;
        print "    { ", $i->[2], ", ", $i->[3], " }";
    }
    print "\n};\n";
}
