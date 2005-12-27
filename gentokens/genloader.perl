#!/usr/bin/perl

# This script generates a quick-n-dirty extension loader library. It is
# definitely not intended as a replacement for GLee or GLEW; it is used
# purely for the test suite, because neither GLee nor GLEW is distributed
# under the GPL and I'm not sure whether their licences are compatible.

use strict;
use Getopt::Long;

my %vers = ();
my %exts = ();
my %funcs = ();
my $current;
my $header = '';
GetOptions('out-header' => \$header);

while (<>)
{
    if (/^#ifndef GL_VERSION_([0-9]+)_([0-9]+)/)
    {
        $vers{"$1.$2"} = "GL_VERSION_$1_$2";
        $current = "$1.$2";
    }
    elsif (/^#ifndef (GL_(?:[0-9A-Z]+)_\w+)/)
    {
        $exts{$1} = "$1";
        $current = "$1";
    }
    elsif (/^#endif/) { $current = ''; }
    elsif (/(gl\w+)\s*\(/ && $current)
    {
        if (!exists($funcs{$current})) { $funcs{$current} = {}; }
        $funcs{$current}->{$1} = "PFN\U$1\EPROC";
    }
}

my @vers = sort keys %vers;
my @exts = sort keys %exts;

if ($header)
{
    print <<EOF;
#ifndef BUGLE_LOADER_H
#define BUGLE_LOADER_H

#include <GL/gl.h>
#include <GL/glext.h>

#ifdef __cplusplus
extern "C" {
#endif

EOF
    foreach my $v (@vers)
    {
        print "#ifdef $vers{$v}\n";
        print "extern GLboolean BUGLE_$vers{$v};\n";
        foreach my $f (sort keys %{$funcs{$v}})
        {
            print "extern $funcs{$v}->{$f} bugle_$f;\n";
            print "#define $f bugle_$f\n";
        }
        print "#endif /* $vers{$v} */\n";
    }
    foreach my $e (@exts)
    {
        print "#ifdef $e\n";
        print "extern GLboolean BUGLE_$e;\n";
        foreach my $f (sort keys %{$funcs{$e}})
        {
            print "extern $funcs{$e}->{$f} bugle_$f;\n";
            print "#define $f bugle_$f\n";
        }
        print "#endif /* $e */\n";
    }
    print <<EOF;

int bugle_init(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* !BUGLE_LOADER_H */
EOF
}
else
{
    print <<EOF;
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glx.h>
#include <GL/glxext.h>
#include <string.h>
#include <stdlib.h>

extern __GLXextFuncPtr glXGetProcAddressARB(const GLubyte *);

EOF
    foreach my $v (@vers)
    {
        print "#ifdef $vers{$v}\n";
        print "GLboolean BUGLE_$vers{$v};\n";
        foreach my $f (sort keys %{$funcs{$v}})
        {
            print "$funcs{$v}->{$f} bugle_$f = NULL;\n";
        }
        print "#endif /* $vers{$v} */\n";
    }
    foreach my $e (@exts)
    {
        print "#ifdef $e\n";
        print "GLboolean BUGLE_$e;\n";
        foreach my $f (sort keys %{$funcs{$e}})
        {
            print "$funcs{$e}->{$f} bugle_$f = NULL;\n";
        }
        print "#endif /* $e */\n";
    }

    print <<EOF;
int bugle_init(void)
{
    const char *version;
    const char *extensions;
    char *extensions2;
    size_t len;

    version = (const char *) glGetString(GL_VERSION);
    extensions = (const char *) glGetString(GL_EXTENSIONS);
    len = strlen(extensions);
    extensions2 = (char *) malloc(len + 3);
    if (!extensions2) return -1;
    strcpy(extensions2 + 1, extensions);
    extensions2[0] = ' ';
    extensions2[len + 1] = ' ';
    extensions2[len + 2] = '\\0';
EOF
    foreach my $v (@vers)
    {
        print "#ifdef $vers{$v}\n";
        print "    if (strcmp(\"$v!\", version) > 0)\n";
        print "    {\n";
        print "        BUGLE_$vers{$v} = GL_TRUE;\n";
        foreach my $f (sort keys %{$funcs{$v}})
        {
            print "        bugle_$f = ($funcs{$v}->{$f}) glXGetProcAddressARB(\"$f\");\n";
        }
        print "    }\n";
        print "#endif /* $vers{$v} */\n";
    }
    foreach my $e (@exts)
    {
        print "#ifdef $e\n";
        print "    if (strstr(extensions2, \" $e \"))\n";
        print "    {\n";
        print "        BUGLE_$e = GL_TRUE;\n";
        foreach my $f (sort keys %{$funcs{$e}})
        {
            print "        bugle_$f = ($funcs{$e}->{$f}) glXGetProcAddressARB(\"$f\");\n";
        }
        print "    }\n";
        print "#endif /* $e */\n";
    }
    print <<EOF;
    free(extensions2);
    return 0;
}
EOF
}
