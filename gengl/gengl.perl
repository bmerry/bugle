#!/usr/bin/perl -w
use strict;
use warnings;
use Getopt::Long;

my @extension_force = (
    "GL_EXT_texture_rectangle",  # Doesn't exist, but ATI exposes it instead of GL_ARB_texture_rectangle
    "GL_VERSION_1_1",            # Base versions
    "GL_ES_VERSION_2_0",
    "GL_VERSION_ES_CM_1_0",
    "GL_VERSION_ES_CM_1_1",
    "GLX_VERSION_1_2",
    "WGL_VERSION_1_0",
    "EGL_VERSION_1_3"
);

my %function_force_version = (
    "glXGetCurrentDisplay" => "GLX_VERSION_1_2",      # glxext.h claims 1.3
    "glFramebufferTextureLayerEXT" => "EXTGROUP_framebuffer_texture_layer"
);

my $function_regex = qr/([ew]?gl[A-Z]\w+)\s*\(/;
my $wgl_function_regex = qr/^wgl/;
my $egl_function_regex = qr/^egl/;

my %extension_chains = (
    "GL_ATI_draw_buffers" => "GL_ARB_draw_buffers",
    "GL_ATI_stencil_two_side" => "GL_EXT_stencil_two_side",
    "GL_ARB_depth_texture" => "GL_VERSION_1_4",
    "GL_ARB_draw_buffers" => "GL_VERSION_2_0",
    "GL_ARB_fragment_program" => undef,
    "GL_ARB_fragment_shader" => ["GL_VERSION_2_0", "GL_ES_VERSION_2_0"],
    "GL_ARB_imaging" => undef,
    "GL_ARB_multisample" => ["GL_VERSION_1_3", "GL_VERSION_ES_CM_1_0", "GL_ES_VERSION_2_0"],
    "GL_ARB_multitexture" => ["GL_VERSION_1_3", "GL_VERSION_ES_CM_1_0", "GL_ES_VERSION_2_0"],
    "GL_ARB_occlusion_query" => "GL_VERSION_1_5",
    "GL_ARB_pixel_buffer_object" => "GL_VERSION_2_1",
    "GL_ARB_point_parameters" => ["GL_VERSION_1_4", "GL_VERSION_ES_CM_1_1", "GL_ES_VERSION_2_0"],
    "GL_ARB_point_sprite" => "GL_VERSION_2_0",
    "GL_ARB_shader_objects" => ["GL_VERSION_2_0", "GL_ES_VERSION_2_0"],
    "GL_ARB_shading_language_100" => ["GL_VERSION_2_0", "GL_ES_VERSION_2_0"],
    "GL_ARB_shadow" => "GL_VERSION_1_4",
    "GL_ARB_texture_border_clamp" => "GL_VERSION_1_3",
    "GL_ARB_texture_compression" => ["GL_VERSION_1_3", "GL_VERSION_ES_CM_1_0", "GL_ES_VERSION_2_0"],
    "GL_ARB_texture_cube_map" => ["GL_VERSION_1_3", "GL_ES_VERSION_2_0"],
    "GL_ARB_texture_env_add" => ["GL_VERSION_1_3", "GL_VERSION_ES_CM_1_0"],
    "GL_ARB_texture_env_dot3" => "GL_VERSION_1_3",
    "GL_ARB_texture_env_combine" => "GL_VERSION_1_3",
    "GL_ARB_texture_env_crossbar" => "GL_VERSION_1_4",
    "GL_ARB_texture_non_power_of_two" => "GL_VERSION_2_0",
    "GL_ARB_texture_rectangle" => undef,
    "GL_ARB_transpose_matrix" => "GL_VERSION_1_3",
    "GL_ARB_vertex_buffer_object" => ["GL_VERSION_1_5", "GL_VERSION_ES_CM_1_1", "GL_ARB_vertex_buffer_object"],
    "GL_ARB_vertex_program" => undef,
    "GL_ARB_vertex_shader" => ["GL_VERSION_2_0", "GL_ES_VERSION_2_0"],
    "GL_ARB_window_pos" => "GL_VERSION_1_4",
    "GL_EXT_bgra" => "GL_VERSION_1_2",
    "GL_EXT_bindable_uniform" => undef,
    "GL_EXT_blend_equation_separate" => ["GL_VERSION_2_0", "GL_ES_VERSION_2_0"],
    "GL_EXT_blend_func_separate" => ["GL_VERSION_1_4", "GL_ES_VERSION_2_0"],
    "GL_EXT_draw_range_elements" => "GL_VERSION_1_2",
    "GL_EXT_fog_coord" => "GL_VERSION_1_4",
    "GL_EXT_framebuffer_object" => "GL_ES_VERSION_2_0",
    "GL_EXT_multi_draw_arrays" => "GL_VERSION_1_4",
    "GL_EXT_packed_pixels" => "GL_VERSION_1_2",
    "GL_EXT_pixel_buffer_object" => "GL_ARB_pixel_buffer_object",
    "GL_EXT_point_parameters" => "GL_ARB_point_parameters",
    "GL_EXT_rescale_normal" => "GL_VERSION_1_2",
    "GL_EXT_secondary_color" => "GL_VERSION_1_4",
    "GL_EXT_separate_specular_color" => "GL_VERSION_1_2",
    "GL_EXT_shadow_funcs" => "GL_VERSION_1_5",
    "GL_EXT_stencil_two_side" => ["GL_VERSION_2_0", "GL_ES_VERSION_2_0"],
    "GL_EXT_stencil_wrap" => ["GL_VERSION_1_4", "GL_ES_VERSION_2_0"],
    "GL_EXT_texture3D" => "GL_VERSION_1_2",
    "GL_EXT_texture_buffer_object" => undef,
    "GL_EXT_texture_cube_map" => "GL_ARB_texture_cube_map",
    "GL_EXT_texture_env_combine" => "GL_ARB_texture_env_combine",
    "GL_EXT_texture_filter_anisotropic" => undef,
    "GL_EXT_texture_lod_bias" => "GL_VERSION_1_4",
    "GL_EXT_texture_rectangle" => "GL_ARB_texture_rectangle",
    "GL_EXT_texture_sRGB" => "GL_VERSION_2_1",
    "GL_SGIS_generate_mipmap" => "GL_VERSION_1_4",
    "GL_SGIS_texture_edge_clamp" => ["GL_VERSION_1_2", "GL_ES_VERSION_2_0"],
    "GL_SGIS_texture_lod" => "GL_VERSION_1_2",
    "GL_NV_blend_square" => ["GL_VERSION_1_4", "GL_ES_VERSION_2_0"],
    "GL_NV_texture_rectangle" => "GL_EXT_texture_rectangle",
    "GLX_ARB_get_proc_address" => "GLX_VERSION_1_4",
    "GLX_EXT_import_context" => "GLX_VERSION_1_3",
    "GLX_SGI_make_current_read" => "GLX_VERSION_1_3",
    "GLX_SGIX_fbconfig" => "GLX_VERSION_1_3"
);

my %extension_groups = (
    # This got promoted to core from imaging subset in 1.4
    "EXTGROUP_blend_color" => ["GL_EXT_blend_color", "GL_ARB_imaging", "GL_VERSION_1_4"],
    # GL_ARB_vertex_program and GL_ARB_fragment_program have a lot of overlap
    "EXTGROUP_old_program" => ["GL_ARB_vertex_program", "GL_ARB_fragment_program"],
    # Extensions that define GL_MAX_TEXTURE_IMAGE_UNITS and GL_MAX_TEXTURE_COORDS
    "EXTGROUP_texunits" => ["GL_ARB_fragment_program", "GL_ARB_vertex_shader", "GL_ARB_fragment_shader", "GL_VERSION_2_0"],
    # Extensions that have GL_VERTEX_PROGRAM_POINT_SIZE and GL_VERTEX_PROGRAM_TWO_SIDE
    "EXTGROUP_vp_options" => ["GL_ARB_vertex_program", "GL_ARB_vertex_shader", "GL_VERSION_2_0"],
    # Extensions that define generic vertex attributes
    "EXTGROUP_vertex_attrib" => ["GL_ARB_vertex_program", "GL_ARB_vertex_shader", "GL_VERSION_2_0", "GL_ES_VERSION_2_0"],
    # Extensions that define FramebufferTextureLayerEXT - needed because some
    # versions of Mesa headers do odd things with this function
    "EXTGROUP_framebuffer_texture_layer" => ["GL_EXT_texture_array", "GL_NV_geometry_program4"]
);

# Aliases that are not automatically detected
my @function_aliases = (
    'glGetProgramivNV'               => 'glGetProgramivARB',
    'glIsProgramNV'                  => 'glIsProgramARB',
    'glCreateShaderObjectARB'        => 'glCreateShader',
    'glCreateProgramObjectARB'       => 'glCreateProgram',
    'glUseProgramObjectARB'          => 'glUseProgram',
    'glAttachObjectARB'              => 'glAttachShader',
    'glDetachObjectARB'              => 'glDetachShader',
    'glGetAttachedObjectsARB'        => 'glGetAttachedShaders',
    'glXCreateContextWithConfigSGIX' => 'glXCreateNewContext',
    'glXMakeCurrentReadSGI'          => 'glXMakeContextCurrent'
);

# Things that syntactically look like aliases, but don't work
my %function_non_aliases = (
    # GL_EXT_vertex_array had one extra parameter
    "glColorPointerEXT" => 1,
    "glEdgeFlagPointerEXT" => 1,
    "glIndexPointerEXT" => 1,
    "glNormalPointerEXT" => 1,
    "glTexCoordPointerEXT" => 1,
    "glVertexPointerEXT" => 1,
    "glVertexAttribPointerNV" => 1,

    # This takes a GLint, not a GLenum
    "glHintPGI" => 1,

    # Takes a GLuint, not a GLenum
    "glGetProgramStringNV" => 1,

    # Mesa takes a colormap
    "glXCreateGLXPixmapMESA" => 1,

    # Are quite different from the ATI or NV versions
    "glVertexArrayRangeAPPLE" => 1,
    "glFlushVertexArrayRangeAPPLE" => 1,
    "glSetFenceAPPLE" => 1,

    "glXFreeMemoryMESA" => 1,
    "glXAllocateMemoryMESA" => 1,

    # EXT version (newer) has extra parameters
    "glXBindTexImageARB" => 1,
    "glXReleaseTexImageARB" => 1,

    # These have totally unrelated semantics to the core 2.0 version
    # (but they should alias each other, which is handled later)
    "glGetProgramivARB" => 1,
    "glGetProgramivNV" => 1,
    "glIsProgramNV" => 1,
    "glIsProgramARB" => 1,

    # grep glDrawElementArray /usr/include/GL/*
    # /usr/include/GL/glext.h:GLAPI void APIENTRY glDrawElementArrayATI (GLenum, GLsizei);
    # /usr/include/GL/glext.h:GLAPI void APIENTRY glDrawElementArrayAPPLE (GLenum, GLint, GLsizei);
    # /usr/include/GL/glext.h:extern void APIENTRY glDrawElementArrayNV(GLenum mode, GLint first, GLsizei count);
    "glDrawElementArrayATI" => 1,

    # grep glDrawRangeElementArray /usr/include/GL/*
    # /usr/include/GL/glext.h:GLAPI void APIENTRY glDrawRangeElementArrayATI (GLenum, GLuint, GLuint, GLsizei);
    # /usr/include/GL/glext.h:GLAPI void APIENTRY glDrawRangeElementArrayAPPLE (GLenum, GLuint, GLuint, GLint, GLsizei);
    # /usr/include/GL/glext.h:extern void APIENTRY glDrawRangeElementArrayNV(GLenum mode, GLuint start, GLuint end, GLint first, GLsizei count);
    "glDrawRangeElementArrayATI" => 1,

    # grep glXBindSwapBarrier /usr/include/GL/*
    # /usr/include/GL/glxext.h:extern void glXBindSwapBarrierSGIX (Display *, GLXDrawable, int);
    # /usr/include/GL/glxext.h:extern Bool glXBindSwapBarrierNV(Display *dpy, GLuint group, GLuint barrier);
    "glXBindSwapBarrierNV" => 1,

    # NV version has only 3 args
    "glGetTransformFeedbackVaryingNV" => 1
);

# Enumerants that we don't want to be the chosen return for
# bugle_api_enum_name, unless there are no other options
my $enum_bad_name_regex = qr/^(?:
GL_ATTRIB_ARRAY_SIZE_NV
|GL_ATTRIB_ARRAY_STRIDE_NV
|GL_ATTRIB_ARRAY_TYPE_NV
|GL_ATTRIB_ARRAY_POINTER_NV
|GL_PIXEL_COUNT_AVAILABLE_NV
|GL_PIXEL_COUNT_NV
|GL_CURRENT_OCCLUSION_QUERY_ID_NV
|GL_PIXEL_COUNTER_BITS_NV
|GL_TEXTURE_COMPONENTS
|GL_CURRENT_ATTRIB_NV
|GL_MAP._VERTEX_ATTRIB._._NV  # heavily reused by ARB_vertex_program

|GL_NO_ERROR
|GL_ZERO
|GL_ONE

|GLX_BAD.*
|GLX_HYPERPIPE_.*
|GLX_PIPE_RECT_.*
|GLX_SYNC_SWAP_.*
|GLX_SYNC_FRAME_.*
|GLX_3DFX_.*_MODE_.*
)$/x;

# Normally we assume that things defined to '1' are not really enumerants,
# but there are exceptions
my $enum_good_name_regex = qr/^(?:
GLX_USE_GL
|GLX_VENDOR
)$/x;

# Enums whose *values* appear in both GL1.1 and an extension. Normally
# we assume that values defined in extensions are not part of 1.1 (because
# otherwise we get some false positives from Mesa), but these are exceptions.
my $enum_force_11_regex = qr/^(?:
GL_MODELVIEW            # aliases MODELVIEW0_ARB
|GL_LINES               # these three have aliases in SUN_triangle_list
|GL_LINE_LOOP
|GL_LINE_STRIP
)$/x;

# Things that perhaps look like enumerants, but aren't.
# NB: this regex must not have capturing brackets, because it is used in
# a situation where $n must not be overwritten.
my $enum_not_name_regex = qr/^(?:
    E?GL_(?:ES_)?VERSION_[0-9_]+
    |GL_VERSION_ES_(?:CM_|CL_)[0-9]+
    |GL_GLEXT_\w+
    |EGL_EGLEXT_\w+
    |GLX_GLXEXT_\w+
    |WGL_WGLEXT_\w+
    |E?GLX?_[A-Z0-9_]+_BIT(?:_[A-Z0-9]+)?
    |GLX_BUFFER_CLOBBER_MASK(?:_[A-Z0-9]+)?
    |GL(?:_[A-Z0-9_]+)?_ALL_[A-Z0-9_]+_BITS(?:_[A-Z0-9]+)?
    |E?GL_FALSE
    |E?GL_TRUE
    |WGL_SWAP_OVERLAY\d+
    |WGL_SWAP_UNDERLAY\d+
    )$/x;

sub base_name($$)
{
    my ($name, $suffix) = @_;
    if ($suffix ne '')
    {
        $name =~ s/\Q$suffix\E$//;
    }
    return $name;
}

# Maps an extension name to an API
sub extension_block($)
{
    my $ext = shift;
    my $block = 'BUGLE_API_EXTENSION_BLOCK_GLWIN';
    if ($ext =~ /^GL_/)
    {
        $block = 'BUGLE_API_EXTENSION_BLOCK_GL';
    }
    return $block;
}

sub enum_name_collate($)
{
    my $name = shift;
    if (!defined($name))
    {
        return 'z';
    }
    elsif ($name =~ /$enum_bad_name_regex/)
    {
        return "b$name";
    }
    else
    {
        return "a$name";
    }
}

# Helper for sorting extension names into a preferred order:
# - GL_[ES_]VERSION_[ES_CM_]* (numerically)
# - GL_ARB_*
# - GL_EXT_*
# - GL_*
# - Similarly for GLX/EGL/WGL, but sorted to end
sub extension_collate($)
{
    my $e = shift;
    if ($e =~ /^(?:GLX|WGL|EGL)_/)
    {
        $e =~ s/^[A-Z]*_/GL_/;
        return 'z' . &extension_collate($e);
    }

    if ($e =~ /^GL_(?:ES_)?VERSION_(?:ES_CM_)?(\d+)_(\d+)/)
    {
        return sprintf("a_%04d_%04d", $1, $2);
    }
    elsif ($e =~ /^GL_(ARB|EXT)_/)
    {
        return "b$1_" . $e;
    }
    else
    {
        return "c$e";
    }
}

sub follow_chain_r($$)
{
    my ($start, $set) = @_;
    return if (!defined($start) || exists($set->{$start}));
    $set->{$start} = 1;
    if (exists($extension_chains{$start}))
    {
        my @follow = ($extension_chains{$start});
        if (ref($follow[0]))
        {
            @follow = @{$follow[0]};
        }
        for my $ext (@follow)
        {
            &follow_chain_r($ext, $set);
        }
    }
}

# Takes the name of an extension, and returns a list of extensions that
# subsume it.
sub follow_chain($)
{
    my $start = shift;
    my %all = ();
    follow_chain_r($start, \%all);
    return keys %all;
}

my @func_ids;

sub load_ids($)
{
    my ($fname) = @_;
    if (!defined($fname) || $fname eq '')
    {
        die("--header is required for this mode");
    }
    open(H, '<', $fname) or die("Could not open $fname: $!");
    while (<H>)
    {
        if (/^#define FUNC_(\w+)\s+(\d+)/)
        {
            my ($func, $value) = ($1, $2);
            $func_ids[$value] = $func;
        }
    }
    close(H);
}

sub extension_write_group($$)
{
    my ($ext, $list) = @_;
    print "static const bugle_api_extension extension_group_" . $ext . "[] =\n";
    print "{\n";
    print map { "    BUGLE_" . $_ . ",\n"} @$list;
    print "    NULL_EXTENSION\n";
    print "};\n\n";
}

my %extensions = (map { $_ => 1 } @extension_force);
my %enums = ();
my %functions = ();
my %groups = ();
my $mode = '';
my $defines = undef;

GetOptions('h|header=s' => \$defines, 'm|mode=s' => \$mode);

foreach my $in_header (@ARGV)
{
    my $cur_ext;
    my $cur_suffix = '';
    my $base_ext;

    open(H, '<', $in_header) or die "Cannot open $in_header";
    $cur_suffix = '';
    if ($in_header =~ /glx\.h|glxext\.h/)
    {
        $base_ext = 'GLX_VERSION_1_2';
    }
    elsif ($in_header =~ /windows\.h|wingdi\.h|wglext\.h/)
    {
        $base_ext = 'WGL_VERSION_1_0';
    }
    elsif ($in_header =~ /egl\.h|eglext\.h/)
    {
        $base_ext = 'EGL_VERSION_1_3';
    }
    else
    {
        $base_ext = 'GL_VERSION_1_1';
    }

    $cur_ext = $base_ext;
    while (<H>)
    {
        if (/^#ifndef ([EW]?GLX?_([0-9A-Z]+)_\w+)/
            && !/GL_GLEXT_/ && !/GLX_GLXEXT_/)
        {
            $cur_ext = $1;
            $cur_suffix = '';
            if (!/^#ifndef [EW]?GLX?_(?:ES_)?VERSION/
                && /^#ifndef [EW]?GLX?_([0-9A-Z]+)_\w+/)
            {
                $cur_suffix = $1;
            }
            $extensions{$cur_ext} = 1;
        }
        elsif (/^#endif/)
        {
            $cur_ext = $base_ext;
        }
        elsif (/^#define ([EW]?GLX?_[0-9A-Za-z_]+)\s+((?:0x)?[0-9A-Fa-f]+)\s*(?:\/\*.*\*\/)?\s*$/)
        {
            my $name = $1;
            my $value = $2;
            if (($name =~ /$enum_good_name_regex/ || $value ne '1')
                && $name !~ /$enum_not_name_regex/)
            {
                $value = oct($value) if $value =~ /^0/; # Convert from hex
                my $block = extension_block($cur_ext);
                if (enum_name_collate($name) lt enum_name_collate($enums{$block}->{$value}->[0]))
                {
                    $enums{$block}->{$value}->[0] = $name;
                }
                $enums{$block}->{$value}->[1]->{$cur_ext} = 1;
            }
        }
        elsif (/$function_regex/)
        {
            my $name = $1;
            my $base = base_name($name, $cur_suffix);

            if ($name =~ /$wgl_function_regex/)
            {
                # Fake version, since WGL isn't versioned
                $functions{$name} = "WGL_VERSION_1_0";
            }
            elsif ($name =~ /$egl_function_regex/)
            {
                # Guess, since egl.h doesn't list versions
                $functions{$name} = "EGL_VERSION_1_3";
            }
            # In some cases the headers are wrong. Use our own table for those
            # cases
            elsif (exists($function_force_version{$name}))
            {
                $functions{$name} = $function_force_version{$name};
            }
            elsif (!defined($functions{$name})
                || $functions{$name} eq 'GL_VERSION_1_1'
                || $functions{$name} eq 'GLX_VERSION_1_2'
                || $functions{$name} eq 'WGL_VERSION_1_0'
                || $functions{$name} eq 'EGL_VERSION_1_3')
            {
                # Mesa headers define assorted functions from higher GL versions
                # without the regexs that we match against. In this case it will
                # appear that the function is 1.1 when really it is something else.
                $functions{$name} = $cur_ext;
            }
            elsif ($cur_ext ne 'GL_VERSION_1_1'
                   && $cur_ext ne 'GLX_VERSION_1_2'
                   && $cur_ext ne 'WGL_VERSION_1_0'
                   && $cur_ext ne 'EGL_VERSION_1_3'
                   && $cur_ext ne $functions{$name})
            {
                die "$name is defined in both $cur_ext and $functions{$name}";
            }
            if (!exists($function_non_aliases{$name}))
            {
                $groups{$base}->{$name} = 1;
            }
        }
    }
}

my @extensions = sort { extension_collate($a) cmp extension_collate($b) } keys %extensions;
my %enums_flat = ();
foreach my $block (keys %enums)
{
    my $keyed = $enums{$block};
    $enums_flat{$block} = [ sort { $a->[0] <=> $b->[0] } map { [$_, @{$keyed->{$_}}] } keys %$keyed ];
}

if ($mode eq 'alias')
{
    for (my $i = 0; $i < scalar(@function_aliases); $i += 2)
    {
        printf("ALIAS %s %s\n", $function_aliases[$i], $function_aliases[$i + 1]);
    }
    foreach my $group (values %groups)
    {
        my @funcs = keys %$group;
        for (my $i = 1; $i < scalar(@funcs); $i++)
        {
            printf("ALIAS %s %s\n", $funcs[$i], $funcs[0]);
        }
    }
}
elsif ($mode eq 'header')
{
    my $index;
    load_ids($defines);

    print "/* Generated at ", scalar(localtime), " by $0. Do not edit. */\n";
    print <<'EOF';
#ifndef BUGLE_SRC_GLTABLES_H
#define BUGLE_SRC_GLTABLES_H
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <bugle/apireflect.h>

typedef struct
{
    const char *version;
    const char *name;
    const bugle_api_extension *group;
    api_block block;
} bugle_api_extension_data;

typedef struct
{
    GLenum value;
    const char *name;
    const bugle_api_extension *extensions;
} bugle_api_enum_data;

typedef struct
{
    bugle_api_extension extension;
} bugle_api_function_data;

EOF
    $index = 0;
    foreach my $ext (@extensions, keys %extension_groups)
    {
        printf "#define BUGLE_%s %d\n", $ext, $index;
        $index++;
    }
    printf "#define BUGLE_API_EXTENSION_COUNT %d\n\n", $index;
    print "extern const bugle_api_extension_data _bugle_api_extension_table[BUGLE_API_EXTENSION_COUNT];\n";
    print "extern const bugle_api_function_data _bugle_api_function_table[];\n";
    print "extern const bugle_api_enum_data * const _bugle_api_enum_table[];\n";
    print "extern const int _bugle_api_enum_count[];\n";
    print <<'EOF';
#endif /* BUGLE_SRC_APITABLES_H */
EOF
}
elsif ($mode eq 'c')
{
    load_ids($defines);

    my $first;

    print "/* Generated at ", scalar(localtime), " by $0. Do not edit. */\n";
    printf <<'EOF';
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <bugle/apireflect.h>
#include "src/apitables.h"
EOF
    foreach my $ext (@extensions)
    {
        my $cur = $ext;
        my @list = follow_chain($cur);
        @list = grep { exists($extensions{$_}) } @list;
        extension_write_group($ext, \@list);
    }
    foreach my $ext (keys %extension_groups)
    {
        my @list = @{$extension_groups{$ext}};
        @list = grep { exists($extensions{$_}) } @list;
        extension_write_group($ext, \@list);
    }

    print "const bugle_api_extension_data _bugle_api_extension_table[] =\n{\n";
    $first = 1;
    foreach my $ext (@extensions, keys %extension_groups)
    {
        my $ver = ($ext =~ /^E?GLX?_(?:ES_)?VERSION_(?:ES_CM_)?([0-9]+)_([0-9]+)/) ? "\"$1.$2\"" : "NULL";
        my $block = extension_block($ext);
        print ",\n" if !$first; $first = 0;
        printf "    { %s, \"%s\", extension_group_%s, %s }", $ver, $ext, $ext, $block;
    }
    print "\n};\n\n";

    print "const bugle_api_function_data _bugle_api_function_table[] =\n{\n";
    $first = 1;
    for (my $id = 0; $id < scalar(@func_ids); $id++)
    {
        my $funcname = $func_ids[$id];
        if (!exists($functions{$funcname}))
        {
            die "Could not find the extension for $funcname";
        }
        my $ext = $functions{$funcname};
        print ",\n" if !$first; $first = 0;
        printf "    { BUGLE_%s }", $ext;
    }
    print "\n};\n\n";

    foreach my $block (keys %enums_flat)
    {
        my $list = $enums_flat{$block};

        foreach my $enum (@$list)
        {
            print "static const bugle_api_extension enum_extensions_$enum->[1]" . "[] =\n{\n";
            $first = 1;
            # Again, Mesa headers cause us to think some things are in 1.1 when
            # they're also defined elsewhere.
            if (scalar(keys(%{$enum->[2]})) > 1
                && exists($enum->[2]->{GL_VERSION_1_1})
                && $enum->[1] !~ /$enum_force_11_regex/)
            {
                delete $enum->[2]->{GL_VERSION_1_1};
            }
            my @exts = sort { extension_collate($a) cmp extension_collate($b) } keys %{$enum->[2]};
            foreach my $ext (@exts)
            {
                print "    BUGLE_$ext,\n";
            }
            print "    NULL_EXTENSION\n};\n\n";
        }

        print "static const bugle_api_enum_data _bugle_api_enum_table_${block}[] =\n{\n";
        $first = 1;
        foreach my $enum (@$list)
        {
            my ($value, $name, $exts) = @$enum;
            print ",\n" unless $first; $first = 0;
            printf("    { 0x%.4x, \"%s\", enum_extensions_%s }", $value, $name, $name);
        }
        print "\n};\n\n";
    }

    # This must match the #define numbering of these blocks
    my @blocks = qw(BUGLE_API_EXTENSION_BLOCK_GL BUGLE_API_EXTENSION_BLOCK_GLWIN);

    $first = 1;
    print "const bugle_api_enum_data * const _bugle_api_enum_table[] =\n{\n";
    foreach my $block (@blocks)
    {
        print ",\n" unless $first; $first = 0;
        print "    _bugle_api_enum_table_$block";
    }
    print "\n};\n\n";

    $first = 1;
    print "const int _bugle_api_enum_count[] =\n{\n";
    foreach my $block (@blocks)
    {
        print ",\n" unless $first; $first = 0;
        my $count = defined($enums_flat{$block}) ? scalar(@{$enums_flat{$block}}) : 0;
        print "    ", $count;
    }
    print "\n};\n\n";
}
else
{
    die "--mode=(alias|header|c) is required\n";
}
