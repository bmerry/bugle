#!/usr/bin/perl -w
use strict;
use Getopt::Long;

sub write_group($$)
{
    my $ext = $_[0];
    my $list = $_[1];
    print "static const int group_" . $ext . "[] =\n";
    print "{\n";
    print map { "    BUGLE_" . $_ . ",\n"} @$list;
    print "    -1\n";
    print "};\n\n";
}

my $outheader = '';
GetOptions('out-header' => \$outheader);

# These extensions do not exist in the registry nor in glext.h, but
# are advertised by some drivers. They are listed here because they
# are equivalent to some registered extension, and we wish drivers that
# expose these extensions to benefit from the equivalent extension. An
# example of this is that ATI drivers expose GL_EXT_texture_rectangle,
# which does not appear in the registry but is equivalent to
# GL_NV_texture_rectangle which does.
my @force = ("GL_EXT_texture_rectangle");

# Chains, for the simple cases.
my %chains = ("GL_ATI_draw_buffers" => "GL_ARB_draw_buffers",
              "GL_ARB_depth_texture" => "GL_VERSION_1_4",
              "GL_ARB_draw_buffers" => "GL_VERSION_2_0",
              "GL_ARB_fragment_program" => undef,
              "GL_ARB_fragment_shader" => "GL_VERSION_2_0",
              "GL_ARB_imaging" => undef,
              "GL_ARB_multisample" => "GL_VERSION_1_3",
              "GL_ARB_multitexture" => "GL_VERSION_1_3",
              "GL_ARB_occlusion_query" => "GL_VERSION_1_5",
              "GL_ARB_pixel_buffer_object" => "GL_VERSION_2_1",
              "GL_ARB_point_parameters" => "GL_VERSION_1_4",
              "GL_ARB_point_sprite" => "GL_VERSION_2_0",
              "GL_ARB_shader_objects" => "GL_VERSION_2_0",
              "GL_ARB_shading_language_100" => "GL_VERSION_2_0",
              "GL_ARB_shadow" => "GL_VERSION_1_4",
              "GL_ARB_texture_compression" => "GL_VERSION_1_3",
              "GL_ARB_texture_cube_map" => "GL_VERSION_1_3",
              "GL_ARB_texture_env_combine" => "GL_VERSION_1_3",
              "GL_ARB_texture_rectangle" => undef,
              "GL_ARB_vertex_buffer_object" => "GL_VERSION_1_5",
              "GL_ARB_vertex_program" => undef,
              "GL_ARB_vertex_shader" => "GL_VERSION_2_0",
              "GL_EXT_blend_equation_separate" => "GL_VERSION_2_0",
              "GL_EXT_blend_func_separate" => "GL_VERSION_1_4",
              "GL_EXT_fog_coord" => "GL_VERSION_1_4",
              "GL_EXT_pixel_buffer_object" => "GL_ARB_pixel_buffer_object",
              "GL_EXT_point_parameters" => "GL_ARB_point_parameters",
              "GL_EXT_rescale_normal" => "GL_VERSION_1_2",
              "GL_EXT_secondary_color" => "GL_VERSION_1_4",
              "GL_EXT_separate_specular_color" => "GL_VERSION_1_2",
              "GL_EXT_texture3D" => "GL_VERSION_1_2",
              "GL_EXT_texture_cube_map" => "GL_ARB_texture_cube_map",
              "GL_EXT_texture_env_combine" => "GL_ARB_texture_env_combine",
              "GL_EXT_texture_filter_anisotropic" => undef,
              "GL_EXT_texture_lod_bias" => "GL_VERSION_1_4",
              "GL_EXT_texture_rectangle" => "GL_ARB_texture_rectangle",
              "GL_EXT_texture_sRGB" => "GL_VERSION_2_1",
              "GL_SGIS_generate_mipmap" => "GL_VERSION_1_4",
              "GL_SGIS_texture_lod" => "GL_VERSION_1_2",
              "GL_NV_texture_rectangle" => "GL_EXT_texture_rectangle"
             );

my %groups = (# This got promoted to core from imaging subset in 1.4
              "EXTGROUP_blend_color" => ["GL_EXT_blend_color", "GL_ARB_imaging", "GL_VERSION_1_4"],
              # GL_ARB_vertex_program and GL_ARB_fragment_program have a lot of overlap
              "EXTGROUP_old_program" => ["GL_ARB_vertex_program", "GL_ARB_fragment_program"],
              # Extensions that define GL_MAX_TEXTURE_IMAGE_UNITS and GL_MAX_TEXTURE_COORDS
              "EXTGROUP_texunits" => ["GL_ARB_fragment_program", "GL_ARB_vertex_shader", "GL_ARB_fragment_shader", "GL_VERSION_2_0"],
              # Extensions that have GL_VERTEX_PROGRAM_POINT_SIZE and GL_VERTEX_PROGRAM_TWO_SIDE
              "EXTGROUP_vp_options" => ["GL_ARB_vertex_program", "GL_ARB_vertex_shader", "GL_VERSION_2_0"],
              # Extensions that define generic vertex attributes
              "EXTGROUP_vertex_attrib" => ["GL_ARB_vertex_program", "GL_ARB_vertex_shader", "GL_VERSION_2_0"]
             );


my %glext_hash = ("GL_VERSION_1_1" => 1, map { $_ => 1 } @force);
my %indices = ();
while (<>)
{
    if (/^#ifndef (GL_[0-9A-Z]+_\w+)/) { $glext_hash{$1} = 1; }
}
my @glext = sort { $a cmp $b } keys %glext_hash;

my $ext_index = 0;
if ($outheader)
{
    print "/* Generated at ", scalar(localtime), " by $0. Do not edit. */\n";
    print <<EOF
#ifndef BUGLE_SRC_GLEXTS_H
#define BUGLE_SRC_GLEXTS_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stddef.h>

typedef struct
{
    const char *gl_string;
    const char *glext_string;
} bugle_ext;
EOF
;

    for my $e (@glext, keys %groups)
    {
        print "#define BUGLE_" . $e . " " . $ext_index . "\n";
        $indices{$e} = $ext_index;
        $ext_index++;
    }
    print "\n";
    print "#define BUGLE_EXT_COUNT " . $ext_index . "\n";
    print "extern const bugle_ext bugle_exts[BUGLE_EXT_COUNT];\n";
    print "extern const int * const bugle_extgroups[BUGLE_EXT_COUNT];\n";

    print "#endif /* !BUGLE_SRC_GLEXTS_H */\n";
}
else
{
    for my $e (@glext, keys %groups)
    {
        $indices{$e} = $ext_index;
        $ext_index++;
    }

    print "/* Generated at ", scalar(localtime), " by $0. Do not edit. */\n";
    print <<EOF
#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "src/glexts.h"

EOF
;
    print "const bugle_ext bugle_exts[BUGLE_EXT_COUNT] =\n";
    print "{\n";
    for my $e (@glext)
    {
        if ($e =~ /^GL_VERSION_([0-9]+)_([0-9]+)/)
        {
            print "    { \"$1.$2\", NULL },\n";
        }
        else
        {
            print "    { NULL, \"$e\" },\n";
        }
    }
    print join(",\n", ("    { NULL, NULL }") x scalar(keys %groups)), "\n";
    print "};\n\n";

    for my $e (@glext)
    {
        my $cur = $e;
        my @list = ($cur);
        while (defined($chains{$cur}))
        {
            $cur = $chains{$cur};
            push @list, ($cur);
        }
        @list = grep { exists($indices{$_}) } @list;
        write_group($e, \@list);
    }
    for my $e (keys %groups)
    {
        my @list = @{$groups{$e}};
        @list = grep { exists($indices{$_}) } @list;
        write_group($e, \@list);
    }

    print "const int * const bugle_extgroups[BUGLE_EXT_COUNT] =\n";
    print "{\n";
    print join(",\n", map { "    group_" . $_ } (@glext, keys %groups)), "\n};\n";
}
