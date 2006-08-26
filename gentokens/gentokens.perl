#!/usr/bin/perl -w
# Extracts GLenum defines from the header files, and writes C code containing
# a table of them. The table also contains information on the GL versions
# and GL extensions that define the tokens.
#
# There are two ways in which GLenum can be implemented: as a C enum or as
# an integer type with #defined values. These rules attempt to handle both
# cases. There are also certain things that look like enums but are not.
# These are the GL_VERSION_1_1 etc strings, and bitfields like
# GL_CLIENT_ALL_ATTRIB_BITS. To make things more confusing, both real
# enums and bitfields can end in _BITS. We also need to consider extension
# suffices.
#
# The problem is further complicated by issues of uniqueness. We associate
# three names with each token:
# - the true name i.e. the name in the #define
# - the base name, which is the true name with any extension suffix removed
# - the group name. All tokens with the same base name AND VALUE are given
# a common group name, which the lexicographically smallest true name amongst
# the group. This is usually the base name (for extensions that have
# been promoted) or the true name (for extensions that haven't). There
# are, however, a few cases where the value changed during promotion or
# where the names are only coincidentally the same.
#
# The input token table is internally stored as a hash of arrays,
# indexed by true name. The array elements are:
# 0: the base name
# 1: value
# 2: version, or undef for pure extensions
# 3: extension, or undef for pure core
#
# The intermediate is similar, but indexed by group name and
# with the first field omitted. It is then flattened into an
# array.

use strict;

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

sub is_enum_name($)
{
    local $_ = shift;
    return !(/^GL_VERSION_[0-9_]+$/
             || /^GL_GLEXT_\w+$/
             || /^GL_[A-Z0-9_]+_BIT(_[A-Z0-9]+)?$/
             || /^GL(_[A-Z0-9_])?_ALL_[A-Z0-9_]+_BITS(_[A-Z0-9]+)?$/);
}

sub compare_token_values
{
    # Some tokens are defined in multiple extensions. Usually alphabetical
    # will do want we want (core first, ARB second, other after), but if
    # the token was renamed then we have a problem. The table below forces
    # certain names to sort later.
    # We also control the sort order for core tokens with values 0 or 1,
    # since there is a lot of aliasing there. The tokens that are sorted
    # later are handled by special dumping functions.
    my %name_map = ("GL_ATTRIB_ARRAY_SIZE_NV" => 1,
                    "GL_ATTRIB_ARRAY_STRIDE_NV" => 1,
                    "GL_ATTRIB_ARRAY_TYPE_NV" => 1,
                    "GL_ATTRIB_ARRAY_POINTER_NV" => 1,
                    "GL_PIXEL_COUNT_AVAILABLE_NV" => 1,
                    "GL_PIXEL_COUNT_NV" => 1,
                    "GL_CURRENT_OCCLUSION_QUERY_ID_NV" => 1,
                    "GL_PIXEL_COUNTER_BITS_NV" => 1,
                    "GL_TEXTURE_COMPONENTS" => 1,
                    "GL_CURRENT_ATTRIB_NV" => 1,

                    "GL_TRUE" => 1,
                    "GL_FALSE" => 1,
                    "GL_NO_ERROR" => 1,
                    "GL_ZERO" => 1,
                    "GL_ONE" => 1);

    return $a->[1] <=> $b->[1] if $a->[1] != $b->[1];

    return 0 if (exists $name_map{$a->[0]} && exists $name_map{$b->[0]});
    return 1 if (exists $name_map{$a->[0]});
    return -1 if (exists $name_map{$b->[0]});
    return $a->[0] cmp $b->[0];
}

sub dump_toks(@)
{
    my $first = 1;
    for my $i (@_)
    {
        my ($group, $value, $ver, $ext) = @$i;
        ($ver, $ext) = text_versions($ver, $ext);

        print ",\n" unless $first; $first = 0;
        printf("    { \"%s\", 0x%.4x, %s, %s }", $group, $value, $ver, $ext);
    }
}

my (%inverse, %intoks, %toks, $ver, $ext, $suffix);

# Load input
$ver = "GL_VERSION_1_1";
while (<>)
{
    if (/^#ifndef (GL_VERSION_[0-9_]+)/)
    {
        $ver = $1;
    }
    elsif (/^#ifndef (GL_([0-9A-Z]+)_\w+)/)
    {
        $ext = $1;
        $suffix = $2;
        $ver = undef;
    }
    elsif (/^#endif/)
    {
        $ext = $suffix = undef;
        $ver = "GL_VERSION_1_1";
    }
    elsif (/^#define (GL_[0-9A-Z_]+)\s+((0x)?[0-9A-Fa-f]+)/ && is_enum_name($1))
    {
        my $name = $1;
        my $base = $name;
        my $val = $2;
        $val = oct($val) if $val =~ /^0/;
        if (defined($ext)) { $base =~ s/_$suffix$//; }
        if (exists($intoks{$name}))
        {
            print("Inconsistent values for $name\n") unless ($val == $intoks{$name}[1]);
            $intoks{$name}[2] = $ver unless defined($intoks{$name}[2]);
            $intoks{$name}[3] = $ext unless defined($intoks{$name}[3]);
        }
        else
        {
            $intoks{$name} = [$base, $val, $ver, $ext];
        }
    }
}

# Construct inverse table to determine group names
for my $i (keys %intoks)
{
    my $base = $intoks{$i}[0];
    my $val = $intoks{$i}[1];
    if (!exists($inverse{$base}{$val})
        || $i lt $inverse{$base}{$val})
    {
        $inverse{$base}{$val} = $i;
    }
}

# Build new tokens table
for my $i (keys %intoks)
{
    my ($base, $val, $ver, $ext) = @{$intoks{$i}};
    my $group = $inverse{$base}{$val};
    if (exists($toks{$group}))
    {
        $toks{$group}[1] = $ver unless defined($toks{$group}[1]);
        $toks{$group}[2] = $ext unless defined($toks{$group}[2]);
    }
    else
    {
        $toks{$group} = [$val, $ver, $ext];
    }
}

my @outtoks = map { [$_, @{$toks{$_}}] } keys %toks;

print "/* Generated at ", scalar(localtime), " by $0. Do not edit. */\n";
print <<'EOF'
#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <GL/gl.h>
#include <GL/glext.h>
#include <stdlib.h> /* for NULL */
#include "src/gltokens.h"

const gl_token bugle_gl_tokens_name[] =
{
EOF
;
dump_toks(sort { $a->[0] cmp $b->[0] } @outtoks);

print <<'EOF'

};

const gl_token bugle_gl_tokens_value[] =
{
EOF
;
dump_toks(sort compare_token_values @outtoks);
print <<'EOF'

};

EOF
;
print "int bugle_gl_token_count = ", scalar(@outtoks), ";\n";
