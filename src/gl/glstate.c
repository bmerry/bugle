/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2012  Bruce Merry
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* This should be complete for GL 4.2 with the exception of anything marked TODO, plus
 * - State that can be either pure integer or float and has to be queried
 *   with the right query to get the right result.
 * - Using GetInteger64v for certain state where GetIntegerv might be out of
 *   range.
 * - GL3-style transform feedback state and per-program XFB varyings
 * - ARB_viewport_array state (scissor, viewport, depth range being arrays)
 * - Check that TexBO tex objects list only the appropriate state
 * - Make blend state per-draw-buffer
 * - ARB_separate_shader_objects state
 * - ARB_shader_subroutines
 * - ARB_shader_atomic_counters
 * - ARB_sync
 * - UBO binding points and most other UBO state
 * - GetInternalformat queries
 *
 * The following will probably never get done, since it's deprecated:
 * - Multiple modelview matrices (for ARB vertex/fragment program)
 * - Paletted textures (need to extend color_table_parameter, which is
 *   complicated by the fact that the palette belongs to the texture not
 *   the target).
 * - Matrix stacks.
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <bugle/gl/glheaders.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <bugle/log.h>
#include <bugle/memory.h>
#include <bugle/string.h>
#include <bugle/io.h>
#include <bugle/gl/glsl.h>
#include <bugle/gl/glstate.h>
#include <bugle/gl/glutils.h>
#include <bugle/gl/globjects.h>
#include <bugle/gl/glextensions.h>
#include <bugle/gl/glfbo.h>
#include <budgie/types.h>
#include <budgie/reflect.h>
#include <budgie/call.h>
#include "budgielib/defines.h"
#include "apitables.h"
#include "platform/types.h"

#define STATE_NAME(x) #x, x
#define STATE_NAME_EXT(x, ext) #x, x ## ext

/* Flags are divided up as follows:
 * 0-7: fetch function indicator
 * 8-15: multiplexor state
 * 16-27: conditional selection flags
 * 28-31: other
 */
#define STATE_MODE_GLOBAL                   0x00000001    /* glGet* */
#define STATE_MODE_ENABLED                  0x00000002    /* glIsEnabled */
#define STATE_MODE_TEXTURE_ENV              0x00000003    /* glGetTexEnv(GL_TEXTURE_ENV, ...) */
#define STATE_MODE_TEXTURE_FILTER_CONTROL   0x00000004    /* glGetTexEnv(GL_TEXTURE_FILTER_CONTROL, ...) */
#define STATE_MODE_POINT_SPRITE             0x00000005    /* glGetTexEnv(GL_POINT_SPRITE, ...) */
#define STATE_MODE_TEX_PARAMETER            0x00000006    /* glGetTexParameter */
#define STATE_MODE_TEX_LEVEL_PARAMETER      0x00000007    /* glGetTexLevelParameter */
#define STATE_MODE_TEX_GEN                  0x00000008    /* glGetTexGen */
#define STATE_MODE_LIGHT                    0x00000009    /* glGetLight */
#define STATE_MODE_MATERIAL                 0x0000000a    /* glGetMaterial */
#define STATE_MODE_CLIP_PLANE               0x0000000b    /* glGetClipPlane */
#define STATE_MODE_POLYGON_STIPPLE          0x0000000c    /* glGetPolygonStipple */
#define STATE_MODE_COLOR_TABLE_PARAMETER    0x0000000d    /* glGetColorTableParameter */
#define STATE_MODE_CONVOLUTION_PARAMETER    0x0000000e    /* glGetConvolutionParameter */
#define STATE_MODE_HISTOGRAM_PARAMETER      0x0000000f    /* glGetHistogramParameter */
#define STATE_MODE_MINMAX_PARAMETER         0x00000010    /* glGetMinmaxParameter */
#define STATE_MODE_VERTEX_ATTRIB            0x00000011    /* glGetVertexAttrib */
#define STATE_MODE_QUERY                    0x00000012    /* glGetQuery */
#define STATE_MODE_QUERY_OBJECT             0x00000013    /* glGetQueryObject */
#define STATE_MODE_BUFFER_PARAMETER         0x00000014    /* glGetBufferParameter */
#define STATE_MODE_SHADER                   0x00000015    /* glGetShader */
#define STATE_MODE_PROGRAM                  0x00000016    /* glGetProgram */
#define STATE_MODE_SHADER_INFO_LOG          0x00000017    /* glGetShaderInfoLog */
#define STATE_MODE_PROGRAM_INFO_LOG         0x00000018    /* glGetProgramInfoLog */
#define STATE_MODE_SHADER_SOURCE            0x00000019    /* glGetShaderSource */
#define STATE_MODE_UNIFORM                  0x0000001a    /* glGetActiveUniform, glGetUniform */
#define STATE_MODE_ATTRIB_LOCATION          0x0000001b    /* glGetAttribLocation */
#define STATE_MODE_ATTACHED_SHADERS         0x0000001c    /* glGetAttachedShaders */
#define STATE_MODE_OLD_PROGRAM              0x0000001d    /* glGetProgramivARB */
#define STATE_MODE_PROGRAM_ENV_PARAMETER    0x0000001e    /* glGetProgramEnvParameterARB */
#define STATE_MODE_PROGRAM_LOCAL_PARAMETER  0x0000001f    /* glGetProgramLocalParameterARB */
#define STATE_MODE_COMPRESSED_TEXTURE_FORMATS 0x00000020  /* glGetIntegerv(GL_COMPRESSED_TEXTURE_FORMATS, ...) */
#define STATE_MODE_RENDERBUFFER_PARAMETER   0x00000021    /* glGetRenderbufferParameterivEXT */
#define STATE_MODE_FRAMEBUFFER_ATTACHMENT_PARAMETER 0x00000022 /* glGetFramebufferAttachmentPArameterivEXT */
#define STATE_MODE_INDEXED                  0x00000023    /* glGetIntegerIndexedvEXT etc */
#define STATE_MODE_ENABLED_INDEXED          0x00000024    /* glIsEnabledIndexedEXT */
#define STATE_MODE_SAMPLE_MASK_VALUE        0x00000025    /* Extracts individual bits from GL_SAMPLE_MASK_VALUE */
#define STATE_MODE_MULTISAMPLE              0x00000026    /* glGetMultisamplefv */
#define STATE_MODE_MASK                     0x000000ff

#define STATE_MULTIPLEX_ACTIVE_TEXTURE      0x00000100    /* Set active texture */
#define STATE_MULTIPLEX_BIND_TEXTURE        0x00000200    /* Set current texture object */
#define STATE_MULTIPLEX_BIND_BUFFER         0x00000400    /* Set current buffer object (GL_ARRAY_BUFFER) */
#define STATE_MULTIPLEX_BIND_PROGRAM        0x00000800    /* Set current low-level program */
#define STATE_MULTIPLEX_BIND_READ_FRAMEBUFFER    0x00001000    /* Set current read framebuffer object */
#define STATE_MULTIPLEX_BIND_DRAW_FRAMEBUFFER    0x00002000    /* Set current draw framebuffer object */
#define STATE_MULTIPLEX_BIND_RENDERBUFFER   0x00004000    /* Set current renderbuffer object */
#define STATE_MULTIPLEX_BIND_VERTEX_ARRAY   0x00008000    /* Set current vertex array object */

#define STATE_SELECT_NO_1D                  0x00010000    /* Ignore for 1D targets like GL_CONVOLUTION_1D */
#define STATE_SELECT_NO_2D                  0x00020000    /* Ignore for 2D targets like GL_TEXTURE_2D */
#define STATE_SELECT_NON_ZERO               0x00040000    /* Ignore if some field is 0 */
#define STATE_SELECT_NO_PROXY               0x00080000    /* Ignore for proxy targets */
#define STATE_SELECT_COMPRESSED             0x00100000    /* Only applies for compressed textures */
#define STATE_SELECT_VERTEX                 0x00200000    /* Applies only if GL_ARB_vertex_shader is present. */
#define STATE_SELECT_FRAGMENT               0x00400000    /* Applies only to fragment programs/shaders. */
#define STATE_SELECT_TEXTURE_ENV            0x00800000    /* Texture unit state that applies to texture application (limited to GL_MAX_TEXTURE_UNITS_ARB) */
#define STATE_SELECT_TEXTURE_COORD          0x01000000    /* Texture unit state that applies to texture coordinates (limited to GL_MAX_TEXTURE_COORDS_ARB) */
#define STATE_SELECT_TEXTURE_IMAGE          0x02000000    /* Texture unit state that applies to texture images (limited to GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS_ARB) */

/* conveniences */
#define STATE_GLOBAL STATE_MODE_GLOBAL
#define STATE_ENABLED STATE_MODE_ENABLED
#define STATE_TEXTURE_ENV (STATE_MODE_TEXTURE_ENV | STATE_MULTIPLEX_ACTIVE_TEXTURE)
#define STATE_TEXTURE_FILTER_CONTROL (STATE_MODE_TEXTURE_FILTER_CONTROL | STATE_MULTIPLEX_ACTIVE_TEXTURE)
#define STATE_POINT_SPRITE (STATE_MODE_POINT_SPRITE | STATE_MULTIPLEX_ACTIVE_TEXTURE)
#define STATE_TEX_UNIT (STATE_MODE_GLOBAL | STATE_MULTIPLEX_ACTIVE_TEXTURE)
#define STATE_TEX_UNIT_ENABLED (STATE_MODE_ENABLED | STATE_MULTIPLEX_ACTIVE_TEXTURE)
#define STATE_TEX_PARAMETER (STATE_MODE_TEX_PARAMETER | STATE_MULTIPLEX_BIND_TEXTURE)
#define STATE_TEX_LEVEL_PARAMETER (STATE_MODE_TEX_LEVEL_PARAMETER | STATE_MULTIPLEX_BIND_TEXTURE)
#define STATE_TEX_GEN (STATE_MODE_TEX_GEN | STATE_MULTIPLEX_ACTIVE_TEXTURE)
#define STATE_LIGHT STATE_MODE_LIGHT
#define STATE_MATERIAL STATE_MODE_MATERIAL
#define STATE_CLIP_PLANE STATE_MODE_CLIP_PLANE
#define STATE_POLYGON_STIPPLE STATE_MODE_POLYGON_STIPPLE
#define STATE_COLOR_TABLE_PARAMETER STATE_MODE_COLOR_TABLE_PARAMETER
#define STATE_CONVOLUTION_PARAMETER STATE_MODE_CONVOLUTION_PARAMETER
#define STATE_HISTOGRAM_PARAMETER STATE_MODE_HISTOGRAM_PARAMETER
#define STATE_MINMAX_PARAMETER STATE_MODE_MINMAX_PARAMETER
#define STATE_VERTEX_ATTRIB STATE_MODE_VERTEX_ATTRIB
#define STATE_QUERY STATE_MODE_QUERY
#define STATE_QUERY_OBJECT STATE_MODE_QUERY_OBJECT
#define STATE_BUFFER_PARAMETER (STATE_MODE_BUFFER_PARAMETER | STATE_MULTIPLEX_BIND_BUFFER)
#define STATE_SHADER STATE_MODE_SHADER
#define STATE_PROGRAM STATE_MODE_PROGRAM
#define STATE_SHADER_INFO_LOG STATE_MODE_SHADER_INFO_LOG
#define STATE_PROGRAM_INFO_LOG STATE_MODE_PROGRAM_INFO_LOG
#define STATE_SHADER_SOURCE STATE_MODE_SHADER_SOURCE
#define STATE_UNIFORM STATE_MODE_UNIFORM
#define STATE_ATTRIB_LOCATION STATE_MODE_ATTRIB_LOCATION
#define STATE_ATTACHED_SHADERS STATE_MODE_ATTACHED_SHADERS
#define STATE_OLD_PROGRAM (STATE_MODE_OLD_PROGRAM | STATE_MULTIPLEX_BIND_PROGRAM)
#define STATE_PROGRAM_ENV_PARAMETER STATE_MODE_PROGRAM_ENV_PARAMETER
#define STATE_PROGRAM_LOCAL_PARAMETER (STATE_MODE_PROGRAM_LOCAL_PARAMETER | STATE_MULTIPLEX_BIND_PROGRAM)
#define STATE_COMPRESSED_TEXTURE_FORMATS STATE_MODE_COMPRESSED_TEXTURE_FORMATS
#define STATE_FRAMEBUFFER_ATTACHMENT_PARAMETER (STATE_MODE_FRAMEBUFFER_ATTACHMENT_PARAMETER | STATE_MULTIPLEX_BIND_DRAW_FRAMEBUFFER)
#define STATE_DRAW_FRAMEBUFFER_PARAMETER (STATE_MODE_GLOBAL | STATE_MULTIPLEX_BIND_DRAW_FRAMEBUFFER)
#define STATE_READ_FRAMEBUFFER_PARAMETER (STATE_MODE_GLOBAL | STATE_MULTIPLEX_BIND_READ_FRAMEBUFFER)
#define STATE_RENDERBUFFER_PARAMETER (STATE_MODE_RENDERBUFFER_PARAMETER | STATE_MULTIPLEX_BIND_RENDERBUFFER)
#define STATE_INDEXED STATE_MODE_INDEXED
#define STATE_ENABLED_INDEXED STATE_MODE_ENABLED_INDEXED
#define STATE_VERTEX_ARRAY (STATE_MODE_GLOBAL | STATE_MULTIPLEX_BIND_VERTEX_ARRAY)
#define STATE_VERTEX_ARRAY_ENABLED (STATE_MODE_ENABLED | STATE_MULTIPLEX_BIND_VERTEX_ARRAY)
#define STATE_VERTEX_ARRAY_ATTRIB (STATE_VERTEX_ATTRIB | STATE_MULTIPLEX_BIND_VERTEX_ARRAY)
#define STATE_SAMPLE_MASK_VALUE STATE_MODE_SAMPLE_MASK_VALUE
#define STATE_MULTISAMPLE (STATE_MODE_MULTISAMPLE | STATE_MULTIPLEX_BIND_DRAW_FRAMEBUFFER)

typedef struct
{
    const char *name;
    GLenum token;
    int extensions;
} enum_list;

static const enum_list material_enums[] =
{
    { STATE_NAME(GL_FRONT), BUGLE_GL_VERSION_1_1 },
    { STATE_NAME(GL_BACK), BUGLE_GL_VERSION_1_1 },
    { NULL, GL_NONE, 0 }
};

static const enum_list cube_map_face_enums[] =
{
    { STATE_NAME(GL_TEXTURE_CUBE_MAP_POSITIVE_X), BUGLE_GL_ARB_texture_cube_map },
    { STATE_NAME(GL_TEXTURE_CUBE_MAP_NEGATIVE_X), BUGLE_GL_ARB_texture_cube_map },
    { STATE_NAME(GL_TEXTURE_CUBE_MAP_POSITIVE_Y), BUGLE_GL_ARB_texture_cube_map },
    { STATE_NAME(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y), BUGLE_GL_ARB_texture_cube_map },
    { STATE_NAME(GL_TEXTURE_CUBE_MAP_POSITIVE_Z), BUGLE_GL_ARB_texture_cube_map },
    { STATE_NAME(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z), BUGLE_GL_ARB_texture_cube_map },
    { NULL, GL_NONE, 0 }
};

static const enum_list tex_gen_enums[] =
{
    { STATE_NAME(GL_S), BUGLE_GL_VERSION_1_1 },
    { STATE_NAME(GL_T), BUGLE_GL_VERSION_1_1 },
    { STATE_NAME(GL_R), BUGLE_GL_VERSION_1_1 },
    { STATE_NAME(GL_Q), BUGLE_GL_VERSION_1_1 },
    { NULL, GL_NONE, 0 }
};

static const enum_list color_table_parameter_enums[] =
{
    { STATE_NAME(GL_COLOR_TABLE), BUGLE_GL_ARB_imaging },
    { STATE_NAME(GL_POST_CONVOLUTION_COLOR_TABLE), BUGLE_GL_ARB_imaging },
    { STATE_NAME(GL_POST_COLOR_MATRIX_COLOR_TABLE), BUGLE_GL_ARB_imaging },
    { NULL, GL_NONE, 0 }
};

static const enum_list proxy_color_table_parameter_enums[] =
{
    { STATE_NAME(GL_PROXY_COLOR_TABLE), BUGLE_GL_ARB_imaging },
    { STATE_NAME(GL_PROXY_POST_CONVOLUTION_COLOR_TABLE), BUGLE_GL_ARB_imaging },
    { STATE_NAME(GL_PROXY_POST_COLOR_MATRIX_COLOR_TABLE), BUGLE_GL_ARB_imaging },
    { NULL, GL_NONE, 0 }
};

static const enum_list convolution_parameter_enums[] =
{
    { STATE_NAME(GL_CONVOLUTION_1D), BUGLE_GL_ARB_imaging },
    { STATE_NAME(GL_CONVOLUTION_2D), BUGLE_GL_ARB_imaging },
    { NULL, GL_NONE, 0 }
};

static const enum_list histogram_parameter_enums[] =
{
    { STATE_NAME(GL_HISTOGRAM), BUGLE_GL_ARB_imaging },
    { NULL, GL_NONE, 0 }
};

static const enum_list proxy_histogram_parameter_enums[] =
{
    { STATE_NAME(GL_PROXY_HISTOGRAM), BUGLE_GL_ARB_imaging },
    { NULL, GL_NONE, 0 }
};

static const enum_list minmax_parameter_enums[] =
{
    { STATE_NAME(GL_MINMAX), BUGLE_GL_ARB_imaging },
    { NULL, GL_NONE, 0 }
};

static const enum_list query_enums[] =
{
    { STATE_NAME(GL_SAMPLES_PASSED), BUGLE_GL_ARB_occlusion_query },
    { STATE_NAME(GL_TIME_ELAPSED), BUGLE_GL_EXT_timer_query },
    { STATE_NAME(GL_PRIMITIVES_GENERATED), BUGLE_GL_EXT_transform_feedback },
    { STATE_NAME(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN), BUGLE_GL_EXT_transform_feedback },
    { NULL, GL_NONE, 0 }
};

/* Note: currently we have two copies of this state, one for per-VAO state and
 * one for the current state (or the only state if VAOs are not present).
 *
 * They are not quite the same, because CURRENT_VERTEX_ATTRIB is global rather
 * than per-VAO.
 */
static const state_info vertex_attrib_state[] =
{
    { STATE_NAME(GL_VERTEX_ATTRIB_ARRAY_ENABLED), TYPE_9GLboolean, -1, BUGLE_EXTGROUP_vertex_attrib, -1, STATE_VERTEX_ATTRIB },
    { STATE_NAME(GL_VERTEX_ATTRIB_ARRAY_SIZE), TYPE_5GLint, -1, BUGLE_EXTGROUP_vertex_attrib, -1, STATE_VERTEX_ATTRIB },
    { STATE_NAME(GL_VERTEX_ATTRIB_ARRAY_STRIDE), TYPE_5GLint, -1, BUGLE_EXTGROUP_vertex_attrib, -1, STATE_VERTEX_ATTRIB },
    { STATE_NAME(GL_VERTEX_ATTRIB_ARRAY_TYPE), TYPE_6GLenum, -1, BUGLE_EXTGROUP_vertex_attrib, -1, STATE_VERTEX_ATTRIB },
    { STATE_NAME(GL_VERTEX_ATTRIB_ARRAY_NORMALIZED), TYPE_9GLboolean, -1, BUGLE_EXTGROUP_vertex_attrib, -1, STATE_VERTEX_ATTRIB },
    { STATE_NAME(GL_VERTEX_ATTRIB_ARRAY_INTEGER), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_3_0, -1, STATE_VERTEX_ATTRIB },
    { STATE_NAME(GL_VERTEX_ATTRIB_ARRAY_POINTER), TYPE_P6GLvoid, -1, BUGLE_EXTGROUP_vertex_attrib, -1, STATE_VERTEX_ATTRIB },
    { STATE_NAME(GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_buffer_object, -1, STATE_VERTEX_ATTRIB },
    { STATE_NAME(GL_VERTEX_ATTRIB_ARRAY_DIVISOR), TYPE_5GLint, -1, BUGLE_GL_ARB_instanced_arrays, -1, STATE_VERTEX_ATTRIB },
    { STATE_NAME(GL_CURRENT_VERTEX_ATTRIB), TYPE_8GLdouble, 4, BUGLE_EXTGROUP_vertex_attrib, -1, STATE_VERTEX_ATTRIB | STATE_SELECT_NON_ZERO },
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info vertex_array_attrib_state[] =
{
    { STATE_NAME(GL_VERTEX_ATTRIB_ARRAY_ENABLED), TYPE_9GLboolean, -1, BUGLE_EXTGROUP_vertex_attrib, -1, STATE_VERTEX_ARRAY_ATTRIB },
    { STATE_NAME(GL_VERTEX_ATTRIB_ARRAY_SIZE), TYPE_5GLint, -1, BUGLE_EXTGROUP_vertex_attrib, -1, STATE_VERTEX_ARRAY_ATTRIB },
    { STATE_NAME(GL_VERTEX_ATTRIB_ARRAY_STRIDE), TYPE_5GLint, -1, BUGLE_EXTGROUP_vertex_attrib, -1, STATE_VERTEX_ARRAY_ATTRIB },
    { STATE_NAME(GL_VERTEX_ATTRIB_ARRAY_TYPE), TYPE_6GLenum, -1, BUGLE_EXTGROUP_vertex_attrib, -1, STATE_VERTEX_ARRAY_ATTRIB },
    { STATE_NAME(GL_VERTEX_ATTRIB_ARRAY_NORMALIZED), TYPE_9GLboolean, -1, BUGLE_EXTGROUP_vertex_attrib, -1, STATE_VERTEX_ARRAY_ATTRIB },
    { STATE_NAME(GL_VERTEX_ATTRIB_ARRAY_INTEGER), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_3_0, -1, STATE_VERTEX_ARRAY_ATTRIB },
    { STATE_NAME(GL_VERTEX_ATTRIB_ARRAY_POINTER), TYPE_P6GLvoid, -1, BUGLE_EXTGROUP_vertex_attrib, -1, STATE_VERTEX_ARRAY_ATTRIB },
    { STATE_NAME(GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_buffer_object, -1, STATE_VERTEX_ARRAY_ATTRIB },
    { STATE_NAME(GL_VERTEX_ATTRIB_ARRAY_DIVISOR), TYPE_5GLint, -1, BUGLE_GL_ARB_instanced_arrays, -1, STATE_VERTEX_ARRAY_ATTRIB },
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info material_state[] =
{
    { STATE_NAME(GL_AMBIENT), TYPE_8GLdouble, 4, BUGLE_GL_VERSION_1_1, -1, STATE_MATERIAL },
    { STATE_NAME(GL_DIFFUSE), TYPE_8GLdouble, 4, BUGLE_GL_VERSION_1_1, -1, STATE_MATERIAL },
    { STATE_NAME(GL_SPECULAR), TYPE_8GLdouble, 4, BUGLE_GL_VERSION_1_1, -1, STATE_MATERIAL },
    { STATE_NAME(GL_EMISSION), TYPE_8GLdouble, 4, BUGLE_GL_VERSION_1_1, -1, STATE_MATERIAL },
    { STATE_NAME(GL_SHININESS), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_MATERIAL },
    { STATE_NAME(GL_COLOR_INDEXES), TYPE_8GLdouble, 3, BUGLE_GL_VERSION_1_1, -1, STATE_MATERIAL },
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info light_state[] =
{
    { STATE_NAME(GL_AMBIENT), TYPE_8GLdouble, 4, BUGLE_GL_VERSION_1_1, -1, STATE_LIGHT },
    { STATE_NAME(GL_DIFFUSE), TYPE_8GLdouble, 4, BUGLE_GL_VERSION_1_1, -1, STATE_LIGHT },
    { STATE_NAME(GL_SPECULAR), TYPE_8GLdouble, 4, BUGLE_GL_VERSION_1_1, -1, STATE_LIGHT },
    { STATE_NAME(GL_POSITION), TYPE_8GLdouble, 4, BUGLE_GL_VERSION_1_1, -1, STATE_LIGHT },
    { STATE_NAME(GL_CONSTANT_ATTENUATION), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_LIGHT },
    { STATE_NAME(GL_LINEAR_ATTENUATION), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_LIGHT },
    { STATE_NAME(GL_QUADRATIC_ATTENUATION), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_LIGHT },
    { STATE_NAME(GL_SPOT_DIRECTION), TYPE_8GLdouble, 3, BUGLE_GL_VERSION_1_1, -1, STATE_LIGHT },
    { STATE_NAME(GL_SPOT_EXPONENT), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_LIGHT },
    { STATE_NAME(GL_SPOT_CUTOFF), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_LIGHT },
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info tex_unit_state[] =
{
    { STATE_NAME(GL_TEXTURE_COORD_ARRAY), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_UNIT_ENABLED | STATE_SELECT_TEXTURE_COORD },
    { STATE_NAME(GL_TEXTURE_COORD_ARRAY_SIZE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_COORD },
    { STATE_NAME(GL_TEXTURE_COORD_ARRAY_TYPE), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_COORD },
    { STATE_NAME(GL_TEXTURE_COORD_ARRAY_STRIDE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_COORD },
    { STATE_NAME(GL_TEXTURE_COORD_ARRAY_POINTER), TYPE_P6GLvoid, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_COORD },
    { STATE_NAME(GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_buffer_object, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_COORD },
    { STATE_NAME(GL_COORD_REPLACE), TYPE_9GLboolean, -1, BUGLE_GL_ARB_point_sprite, -1, STATE_POINT_SPRITE | STATE_SELECT_TEXTURE_COORD },
    { STATE_NAME(GL_TEXTURE_ENV_MODE), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_TEXTURE_ENV_COLOR), TYPE_8GLdouble, 4, BUGLE_GL_VERSION_1_1, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV  },
    { STATE_NAME(GL_TEXTURE_LOD_BIAS), TYPE_8GLdouble, -1, BUGLE_GL_EXT_texture_lod_bias, -1, STATE_TEXTURE_FILTER_CONTROL | STATE_SELECT_TEXTURE_IMAGE },
    { STATE_NAME(GL_TEXTURE_GEN_S), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_UNIT_ENABLED | STATE_SELECT_TEXTURE_COORD },
    { STATE_NAME(GL_TEXTURE_GEN_T), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_UNIT_ENABLED | STATE_SELECT_TEXTURE_COORD },
    { STATE_NAME(GL_TEXTURE_GEN_R), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_UNIT_ENABLED | STATE_SELECT_TEXTURE_COORD },
    { STATE_NAME(GL_TEXTURE_GEN_Q), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_UNIT_ENABLED | STATE_SELECT_TEXTURE_COORD },
    { STATE_NAME(GL_COMBINE_RGB), TYPE_6GLenum, -1, BUGLE_GL_EXT_texture_env_combine, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_COMBINE_ALPHA), TYPE_6GLenum, -1, BUGLE_GL_EXT_texture_env_combine, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_SOURCE0_RGB), TYPE_6GLenum, -1, BUGLE_GL_EXT_texture_env_combine, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_SOURCE1_RGB), TYPE_6GLenum, -1, BUGLE_GL_EXT_texture_env_combine, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_SOURCE2_RGB), TYPE_6GLenum, -1, BUGLE_GL_EXT_texture_env_combine, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_SOURCE0_ALPHA), TYPE_6GLenum, -1, BUGLE_GL_EXT_texture_env_combine, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_SOURCE1_ALPHA), TYPE_6GLenum, -1, BUGLE_GL_EXT_texture_env_combine, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_SOURCE2_ALPHA), TYPE_6GLenum, -1, BUGLE_GL_EXT_texture_env_combine, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_OPERAND0_RGB), TYPE_6GLenum, -1, BUGLE_GL_EXT_texture_env_combine, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_OPERAND1_RGB), TYPE_6GLenum, -1, BUGLE_GL_EXT_texture_env_combine, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_OPERAND2_RGB), TYPE_6GLenum, -1, BUGLE_GL_EXT_texture_env_combine, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_OPERAND0_ALPHA), TYPE_6GLenum, -1, BUGLE_GL_EXT_texture_env_combine, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_OPERAND1_ALPHA), TYPE_6GLenum, -1, BUGLE_GL_EXT_texture_env_combine, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_OPERAND2_ALPHA), TYPE_6GLenum, -1, BUGLE_GL_EXT_texture_env_combine, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_RGB_SCALE), TYPE_8GLdouble, -1, BUGLE_GL_EXT_texture_env_combine, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_ALPHA_SCALE), TYPE_8GLdouble, -1, BUGLE_GL_EXT_texture_env_combine, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_CURRENT_TEXTURE_COORDS), TYPE_8GLdouble, 4, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_COORD },
    { STATE_NAME(GL_CURRENT_RASTER_TEXTURE_COORDS), TYPE_8GLdouble, 4, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_COORD },
    { STATE_NAME(GL_TEXTURE_MATRIX), TYPE_8GLdouble, 16, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_COORD },
    { STATE_NAME(GL_TEXTURE_STACK_DEPTH), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_COORD },
    { STATE_NAME(GL_TEXTURE_1D), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_UNIT_ENABLED | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_TEXTURE_2D), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_UNIT_ENABLED | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_TEXTURE_3D), TYPE_9GLboolean, -1, BUGLE_GL_EXT_texture3D, -1, STATE_TEX_UNIT_ENABLED | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_TEXTURE_CUBE_MAP), TYPE_9GLboolean, -1, BUGLE_GL_ARB_texture_cube_map, -1, STATE_TEX_UNIT_ENABLED | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_TEXTURE_RECTANGLE), TYPE_9GLboolean, -1, BUGLE_GL_NV_texture_rectangle, -1, STATE_TEX_UNIT_ENABLED | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_TEXTURE_BINDING_1D), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_IMAGE },
    { STATE_NAME(GL_TEXTURE_BINDING_2D), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_IMAGE },
    { STATE_NAME(GL_TEXTURE_BINDING_3D), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_2, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_IMAGE }, /* Note: GL_EXT_texture3D doesn't define this! */
    { STATE_NAME(GL_TEXTURE_BINDING_1D_ARRAY), TYPE_5GLint, -1, BUGLE_GL_EXT_texture_array, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_IMAGE },
    { STATE_NAME(GL_TEXTURE_BINDING_2D_ARRAY), TYPE_5GLint, -1, BUGLE_GL_EXT_texture_array, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_IMAGE },
    { STATE_NAME(GL_TEXTURE_BINDING_CUBE_MAP_ARRAY), TYPE_5GLint, -1, BUGLE_GL_ARB_texture_cube_map_array, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_IMAGE },
    { STATE_NAME(GL_TEXTURE_BINDING_RECTANGLE), TYPE_5GLint, -1, BUGLE_GL_NV_texture_rectangle, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_IMAGE },
    { STATE_NAME(GL_TEXTURE_BINDING_BUFFER), TYPE_5GLint, -1, BUGLE_GL_ARB_texture_buffer_object, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_IMAGE },
    { STATE_NAME(GL_TEXTURE_BINDING_CUBE_MAP), TYPE_5GLint, -1, BUGLE_GL_ARB_texture_cube_map, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_IMAGE },
    { STATE_NAME(GL_TEXTURE_BINDING_2D_MULTISAMPLE), TYPE_5GLint, -1, BUGLE_GL_ARB_texture_multisample, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_IMAGE },
    { STATE_NAME(GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY), TYPE_5GLint, -1, BUGLE_GL_ARB_texture_multisample, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_IMAGE },
    { STATE_NAME(GL_SAMPLER_BINDING), TYPE_5GLint, -1, BUGLE_GL_ARB_sampler_objects, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_IMAGE },
    /* FIXME: GetTexImage */
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info tex_gen_state[] =
{
    { STATE_NAME(GL_EYE_PLANE), TYPE_8GLdouble, 4, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_GEN },
    { STATE_NAME(GL_OBJECT_PLANE), TYPE_8GLdouble, 4, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_GEN },
    { STATE_NAME(GL_TEXTURE_GEN_MODE), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_GEN },
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info tex_level_parameter_state[] =
{
    { STATE_NAME(GL_TEXTURE_WIDTH), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_LEVEL_PARAMETER },
    { STATE_NAME(GL_TEXTURE_HEIGHT), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_LEVEL_PARAMETER | STATE_SELECT_NO_1D },
    { STATE_NAME(GL_TEXTURE_DEPTH), TYPE_5GLint, -1, BUGLE_GL_EXT_texture3D, -1, STATE_TEX_LEVEL_PARAMETER | STATE_SELECT_NO_1D | STATE_SELECT_NO_2D },
    { STATE_NAME(GL_TEXTURE_BORDER), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_LEVEL_PARAMETER },
    { STATE_NAME(GL_TEXTURE_SAMPLES), TYPE_5GLint, -1, BUGLE_GL_ARB_texture_multisample, -1, STATE_TEX_LEVEL_PARAMETER },
    { STATE_NAME(GL_TEXTURE_FIXED_SAMPLE_LOCATIONS), TYPE_9GLboolean, -1, BUGLE_GL_ARB_texture_multisample, -1, STATE_TEX_LEVEL_PARAMETER },
    { STATE_NAME(GL_TEXTURE_INTERNAL_FORMAT), TYPE_16GLcomponentsenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_LEVEL_PARAMETER },
    { STATE_NAME(GL_TEXTURE_RED_SIZE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_LEVEL_PARAMETER },
    { STATE_NAME(GL_TEXTURE_GREEN_SIZE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_LEVEL_PARAMETER },
    { STATE_NAME(GL_TEXTURE_BLUE_SIZE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_LEVEL_PARAMETER },
    { STATE_NAME(GL_TEXTURE_ALPHA_SIZE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_LEVEL_PARAMETER },
    { STATE_NAME(GL_TEXTURE_LUMINANCE_SIZE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_LEVEL_PARAMETER },
    { STATE_NAME(GL_TEXTURE_INTENSITY_SIZE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_LEVEL_PARAMETER },
    { STATE_NAME(GL_TEXTURE_DEPTH_SIZE), TYPE_5GLint, -1, BUGLE_GL_ARB_depth_texture, -1, STATE_TEX_LEVEL_PARAMETER },
    { STATE_NAME(GL_TEXTURE_STENCIL_SIZE), TYPE_5GLint, -1, BUGLE_GL_EXT_packed_depth_stencil, -1, STATE_TEX_LEVEL_PARAMETER },
    { STATE_NAME(GL_TEXTURE_SHARED_SIZE), TYPE_5GLint, -1, BUGLE_GL_EXT_texture_shared_exponent, -1, STATE_TEX_LEVEL_PARAMETER },
#ifdef GL_EXT_palette_texture
    { STATE_NAME(GL_TEXTURE_INDEX_SIZE_EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_palette_texture, -1, STATE_TEX_LEVEL_PARAMETER },
#endif
    { STATE_NAME(GL_TEXTURE_RED_TYPE), TYPE_6GLenum, -1, BUGLE_GL_ARB_texture_float, -1, STATE_TEX_LEVEL_PARAMETER },
    { STATE_NAME(GL_TEXTURE_GREEN_TYPE), TYPE_6GLenum, -1, BUGLE_GL_ARB_texture_float, -1, STATE_TEX_LEVEL_PARAMETER },
    { STATE_NAME(GL_TEXTURE_BLUE_TYPE), TYPE_6GLenum, -1, BUGLE_GL_ARB_texture_float, -1, STATE_TEX_LEVEL_PARAMETER },
    { STATE_NAME(GL_TEXTURE_ALPHA_TYPE), TYPE_6GLenum, -1, BUGLE_GL_ARB_texture_float, -1, STATE_TEX_LEVEL_PARAMETER },
    { STATE_NAME(GL_TEXTURE_LUMINANCE_TYPE), TYPE_6GLenum, -1, BUGLE_GL_ARB_texture_float, -1, STATE_TEX_LEVEL_PARAMETER },
    { STATE_NAME(GL_TEXTURE_INTENSITY_TYPE), TYPE_6GLenum, -1, BUGLE_GL_ARB_texture_float, -1, STATE_TEX_LEVEL_PARAMETER },
    { STATE_NAME(GL_TEXTURE_DEPTH_TYPE), TYPE_6GLenum, -1, BUGLE_GL_ARB_texture_float, -1, STATE_TEX_LEVEL_PARAMETER },
    { STATE_NAME(GL_TEXTURE_COMPRESSED), TYPE_9GLboolean, -1, BUGLE_GL_ARB_texture_compression, -1, STATE_TEX_LEVEL_PARAMETER },
    { STATE_NAME(GL_TEXTURE_COMPRESSED_IMAGE_SIZE), TYPE_5GLint, -1, BUGLE_GL_ARB_texture_compression, -1, STATE_TEX_LEVEL_PARAMETER | STATE_SELECT_NO_PROXY | STATE_SELECT_COMPRESSED },
    { STATE_NAME(GL_TEXTURE_BUFFER_DATA_STORE_BINDING), TYPE_5GLint, -1, BUGLE_GL_ARB_texture_buffer_object, -1, STATE_TEX_LEVEL_PARAMETER },
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info tex_parameter_state[] =
{
    { STATE_NAME(GL_TEXTURE_SWIZZLE_R), TYPE_6GLenum, -1, BUGLE_GL_ARB_texture_swizzle, -1, STATE_TEX_PARAMETER },
    { STATE_NAME(GL_TEXTURE_SWIZZLE_G), TYPE_6GLenum, -1, BUGLE_GL_ARB_texture_swizzle, -1, STATE_TEX_PARAMETER },
    { STATE_NAME(GL_TEXTURE_SWIZZLE_B), TYPE_6GLenum, -1, BUGLE_GL_ARB_texture_swizzle, -1, STATE_TEX_PARAMETER },
    { STATE_NAME(GL_TEXTURE_SWIZZLE_A), TYPE_6GLenum, -1, BUGLE_GL_ARB_texture_swizzle, -1, STATE_TEX_PARAMETER },
    { STATE_NAME(GL_TEXTURE_BORDER_COLOR), TYPE_8GLdouble, 4, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_PARAMETER },
    { STATE_NAME(GL_TEXTURE_MIN_FILTER), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_PARAMETER },
    { STATE_NAME(GL_TEXTURE_MAG_FILTER), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_PARAMETER },
    { STATE_NAME(GL_TEXTURE_WRAP_S), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_PARAMETER },
    { STATE_NAME(GL_TEXTURE_WRAP_T), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_PARAMETER | STATE_SELECT_NO_1D },
    { STATE_NAME(GL_TEXTURE_WRAP_R), TYPE_6GLenum, -1, BUGLE_GL_EXT_texture3D, -1, STATE_TEX_PARAMETER | STATE_SELECT_NO_1D | STATE_SELECT_NO_2D },
    { STATE_NAME(GL_TEXTURE_PRIORITY), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_PARAMETER },
    { STATE_NAME(GL_TEXTURE_RESIDENT), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_PARAMETER },
    { STATE_NAME(GL_TEXTURE_MIN_LOD), TYPE_8GLdouble, -1, BUGLE_GL_SGIS_texture_lod, -1, STATE_TEX_PARAMETER },
    { STATE_NAME(GL_TEXTURE_MAX_LOD), TYPE_8GLdouble, -1, BUGLE_GL_SGIS_texture_lod, -1, STATE_TEX_PARAMETER },
    { STATE_NAME(GL_TEXTURE_BASE_LEVEL), TYPE_5GLint, -1, BUGLE_GL_SGIS_texture_lod, -1, STATE_TEX_PARAMETER },
    { STATE_NAME(GL_TEXTURE_MAX_LEVEL), TYPE_5GLint, -1, BUGLE_GL_SGIS_texture_lod, -1, STATE_TEX_PARAMETER },
    { STATE_NAME(GL_TEXTURE_LOD_BIAS), TYPE_8GLdouble, -1, BUGLE_GL_EXT_texture_lod_bias, -1, STATE_TEX_PARAMETER },
    { STATE_NAME(GL_DEPTH_TEXTURE_MODE), TYPE_6GLenum, -1, BUGLE_GL_ARB_depth_texture, -1, STATE_TEX_PARAMETER },
    { STATE_NAME(GL_TEXTURE_COMPARE_MODE), TYPE_6GLenum, -1, BUGLE_GL_ARB_shadow, -1, STATE_TEX_PARAMETER },
    { STATE_NAME(GL_TEXTURE_COMPARE_FUNC), TYPE_6GLenum, -1, BUGLE_GL_ARB_shadow, -1, STATE_TEX_PARAMETER },
    { STATE_NAME(GL_GENERATE_MIPMAP), TYPE_9GLboolean, -1, BUGLE_GL_SGIS_generate_mipmap, -1, STATE_TEX_PARAMETER },
    { STATE_NAME(GL_TEXTURE_IMMUTABLE_FORMAT), TYPE_9GLboolean, -1, BUGLE_GL_ARB_texture_storage, -1, STATE_TEX_PARAMETER },
    { STATE_NAME(GL_IMAGE_FORMAT_COMPATIBILITY_TYPE), TYPE_6GLenum, -1, BUGLE_GL_ARB_shader_image_load_store, -1, STATE_TEX_PARAMETER },

    { STATE_NAME(GL_TEXTURE_MAX_ANISOTROPY_EXT), TYPE_8GLdouble, -1, BUGLE_GL_EXT_texture_filter_anisotropic, -1, STATE_TEX_PARAMETER },
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info tex_buffer_state[] =
{
#ifdef GL_EXT_texture_buffer_object
    { STATE_NAME(GL_TEXTURE_BUFFER_DATA_STORE_BINDING_EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_texture_buffer_object, -1, STATE_GLOBAL | STATE_MULTIPLEX_BIND_TEXTURE },
    { STATE_NAME(GL_TEXTURE_BUFFER_FORMAT_EXT), TYPE_6GLenum, -1, BUGLE_GL_EXT_texture_buffer_object, -1, STATE_GLOBAL | STATE_MULTIPLEX_BIND_TEXTURE },
#endif
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info color_table_parameter_state[] =
{
    { STATE_NAME(GL_COLOR_TABLE_FORMAT), TYPE_6GLenum, -1, BUGLE_GL_ARB_imaging, -1, STATE_COLOR_TABLE_PARAMETER },
    { STATE_NAME(GL_COLOR_TABLE_WIDTH), TYPE_5GLint, -1, BUGLE_GL_ARB_imaging, -1, STATE_COLOR_TABLE_PARAMETER },
    { STATE_NAME(GL_COLOR_TABLE_RED_SIZE), TYPE_5GLint, -1, BUGLE_GL_ARB_imaging, -1, STATE_COLOR_TABLE_PARAMETER },
    { STATE_NAME(GL_COLOR_TABLE_GREEN_SIZE), TYPE_5GLint, -1, BUGLE_GL_ARB_imaging, -1, STATE_COLOR_TABLE_PARAMETER },
    { STATE_NAME(GL_COLOR_TABLE_BLUE_SIZE), TYPE_5GLint, -1, BUGLE_GL_ARB_imaging, -1, STATE_COLOR_TABLE_PARAMETER },
    { STATE_NAME(GL_COLOR_TABLE_ALPHA_SIZE), TYPE_5GLint, -1, BUGLE_GL_ARB_imaging, -1, STATE_COLOR_TABLE_PARAMETER },
    { STATE_NAME(GL_COLOR_TABLE_LUMINANCE_SIZE), TYPE_5GLint, -1, BUGLE_GL_ARB_imaging, -1, STATE_COLOR_TABLE_PARAMETER },
    { STATE_NAME(GL_COLOR_TABLE_INTENSITY_SIZE), TYPE_5GLint, -1, BUGLE_GL_ARB_imaging, -1, STATE_COLOR_TABLE_PARAMETER },
    { STATE_NAME(GL_COLOR_TABLE_SCALE), TYPE_8GLdouble, 4, BUGLE_GL_ARB_imaging, -1, STATE_COLOR_TABLE_PARAMETER | STATE_SELECT_NO_PROXY },
    { STATE_NAME(GL_COLOR_TABLE_BIAS), TYPE_8GLdouble, 4, BUGLE_GL_ARB_imaging, -1, STATE_COLOR_TABLE_PARAMETER | STATE_SELECT_NO_PROXY },
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info convolution_parameter_state[] =
{
    { STATE_NAME(GL_CONVOLUTION_BORDER_COLOR), TYPE_8GLdouble, 4, BUGLE_GL_ARB_imaging, -1, STATE_CONVOLUTION_PARAMETER },
    { STATE_NAME(GL_CONVOLUTION_BORDER_MODE), TYPE_6GLenum, -1, BUGLE_GL_ARB_imaging, -1, STATE_CONVOLUTION_PARAMETER },
    { STATE_NAME(GL_CONVOLUTION_FILTER_SCALE), TYPE_8GLdouble, 4, BUGLE_GL_ARB_imaging, -1, STATE_CONVOLUTION_PARAMETER },
    { STATE_NAME(GL_CONVOLUTION_FILTER_BIAS), TYPE_8GLdouble, 4, BUGLE_GL_ARB_imaging, -1, STATE_CONVOLUTION_PARAMETER },
    { STATE_NAME(GL_CONVOLUTION_FORMAT), TYPE_6GLenum, -1, BUGLE_GL_ARB_imaging, -1, STATE_CONVOLUTION_PARAMETER },
    { STATE_NAME(GL_CONVOLUTION_WIDTH), TYPE_5GLint, -1, BUGLE_GL_ARB_imaging, -1, STATE_CONVOLUTION_PARAMETER },
    { STATE_NAME(GL_CONVOLUTION_HEIGHT), TYPE_5GLint, -1, BUGLE_GL_ARB_imaging, -1, STATE_CONVOLUTION_PARAMETER | STATE_SELECT_NO_1D },
    { STATE_NAME(GL_MAX_CONVOLUTION_WIDTH), TYPE_5GLint, -1, BUGLE_GL_ARB_imaging, -1, STATE_CONVOLUTION_PARAMETER },
    { STATE_NAME(GL_MAX_CONVOLUTION_HEIGHT), TYPE_5GLint, -1, BUGLE_GL_ARB_imaging, -1, STATE_CONVOLUTION_PARAMETER | STATE_SELECT_NO_1D },
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info histogram_parameter_state[] =
{
    { STATE_NAME(GL_HISTOGRAM_WIDTH), TYPE_5GLint, -1, BUGLE_GL_ARB_imaging, -1, STATE_HISTOGRAM_PARAMETER },
    { STATE_NAME(GL_HISTOGRAM_FORMAT), TYPE_6GLenum, -1, BUGLE_GL_ARB_imaging, -1, STATE_HISTOGRAM_PARAMETER },
    { STATE_NAME(GL_HISTOGRAM_RED_SIZE), TYPE_5GLint, -1, BUGLE_GL_ARB_imaging, -1, STATE_HISTOGRAM_PARAMETER },
    { STATE_NAME(GL_HISTOGRAM_GREEN_SIZE), TYPE_5GLint, -1, BUGLE_GL_ARB_imaging, -1, STATE_HISTOGRAM_PARAMETER },
    { STATE_NAME(GL_HISTOGRAM_BLUE_SIZE), TYPE_5GLint, -1, BUGLE_GL_ARB_imaging, -1, STATE_HISTOGRAM_PARAMETER },
    { STATE_NAME(GL_HISTOGRAM_ALPHA_SIZE), TYPE_5GLint, -1, BUGLE_GL_ARB_imaging, -1, STATE_HISTOGRAM_PARAMETER },
    { STATE_NAME(GL_HISTOGRAM_LUMINANCE_SIZE), TYPE_5GLint, -1, BUGLE_GL_ARB_imaging, -1, STATE_HISTOGRAM_PARAMETER },
    { STATE_NAME(GL_HISTOGRAM_SINK), TYPE_9GLboolean, -1, BUGLE_GL_ARB_imaging, -1, STATE_HISTOGRAM_PARAMETER },
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info minmax_parameter_state[] =
{
    { STATE_NAME(GL_MINMAX_FORMAT), TYPE_6GLenum, -1, BUGLE_GL_ARB_imaging, -1, STATE_MINMAX_PARAMETER },
    { STATE_NAME(GL_MINMAX_SINK), TYPE_9GLboolean, -1, BUGLE_GL_ARB_imaging, -1, STATE_MINMAX_PARAMETER },
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

/* Per-program state that is only valid when a geometry shader is present.
 */
static const state_info program_geometry_state[] =
{
    { STATE_NAME(GL_GEOMETRY_VERTICES_OUT), TYPE_5GLint, -1, BUGLE_GL_ARB_geometry_shader4, -1, STATE_PROGRAM },
    { STATE_NAME(GL_GEOMETRY_INPUT_TYPE), TYPE_6GLenum, -1, BUGLE_GL_ARB_geometry_shader4, -1, STATE_PROGRAM },
    { STATE_NAME(GL_GEOMETRY_OUTPUT_TYPE), TYPE_6GLenum, -1, BUGLE_GL_ARB_geometry_shader4, -1, STATE_PROGRAM },
    { STATE_NAME(GL_GEOMETRY_SHADER_INVOCATIONS), TYPE_5GLint, -1, BUGLE_GL_ARB_gpu_shader5, -1, STATE_PROGRAM },
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

/* Per-program state that is only valid when tessellation control shader is present.
 */
static const state_info program_tess_control_state[] =
{
    { STATE_NAME(GL_TESS_CONTROL_OUTPUT_VERTICES), TYPE_5GLint, -1, BUGLE_GL_ARB_tessellation_shader, -1, STATE_PROGRAM },
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

/* Per-program state that is only valid when tessellation evaluation shader is present.
 */
static const state_info program_tess_evaluation_state[] =
{
    { STATE_NAME(GL_TESS_GEN_MODE), TYPE_6GLenum, -1, BUGLE_GL_ARB_tessellation_shader, -1, STATE_PROGRAM },
    { STATE_NAME(GL_TESS_GEN_SPACING), TYPE_6GLenum, -1, BUGLE_GL_ARB_tessellation_shader, -1, STATE_PROGRAM },
    { STATE_NAME(GL_TESS_GEN_VERTEX_ORDER), TYPE_6GLenum, -1, BUGLE_GL_ARB_tessellation_shader, -1, STATE_PROGRAM },
    { STATE_NAME(GL_TESS_GEN_POINT_MODE), TYPE_9GLboolean, -1, BUGLE_GL_ARB_tessellation_shader, -1, STATE_PROGRAM },
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static void spawn_program_geometry(const struct glstate *self,
                                   linked_list *children,
                                   const struct state_info *info);

static void spawn_program_tess_control(const struct glstate *self,
                                       linked_list *children,
                                       const struct state_info *info);

static void spawn_program_tess_evaluation(const struct glstate *self,
                                          linked_list *children,
                                          const struct state_info *info);

static const state_info program_state[] =
{
    { STATE_NAME(GL_DELETE_STATUS), TYPE_9GLboolean, -1, BUGLE_GL_ARB_shader_objects, -1, STATE_PROGRAM },
    { STATE_NAME(GL_INFO_LOG_LENGTH), TYPE_5GLint, -1, BUGLE_GL_ARB_shader_objects, -1, STATE_PROGRAM },
    { "InfoLog", GL_NONE, TYPE_P6GLchar, -1, BUGLE_GL_ARB_shader_objects, -1, STATE_PROGRAM_INFO_LOG },
    { STATE_NAME(GL_ATTACHED_SHADERS), TYPE_5GLint, -1, BUGLE_GL_ARB_shader_objects, -1, STATE_PROGRAM },
    { STATE_NAME(GL_ACTIVE_UNIFORMS), TYPE_5GLint, -1, BUGLE_GL_ARB_shader_objects, -1, STATE_PROGRAM },
    { STATE_NAME(GL_LINK_STATUS), TYPE_9GLboolean, -1, BUGLE_GL_ARB_shader_objects, -1, STATE_PROGRAM },
    { STATE_NAME(GL_VALIDATE_STATUS), TYPE_9GLboolean, -1, BUGLE_GL_ARB_shader_objects, -1, STATE_PROGRAM },
    { STATE_NAME(GL_PROGRAM_BINARY_LENGTH), TYPE_5GLint, -1, BUGLE_GL_ARB_get_program_binary, -1, STATE_PROGRAM },
    { STATE_NAME(GL_PROGRAM_BINARY_RETRIEVABLE_HINT), TYPE_9GLboolean, -1, BUGLE_GL_ARB_get_program_binary, -1, STATE_PROGRAM },
    { STATE_NAME(GL_PROGRAM_SEPARABLE), TYPE_9GLboolean, -1, BUGLE_GL_ARB_separate_shader_objects, -1, STATE_PROGRAM },
    { STATE_NAME(GL_ACTIVE_UNIFORM_MAX_LENGTH), TYPE_5GLint, -1, BUGLE_GL_ARB_shader_objects, -1, STATE_PROGRAM },
    { "Attached", GL_NONE, TYPE_6GLuint, 0, BUGLE_GL_ARB_shader_objects, -1, STATE_ATTACHED_SHADERS },
    { STATE_NAME(GL_ACTIVE_ATTRIBUTES), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_shader, -1, STATE_PROGRAM },
    { STATE_NAME(GL_ACTIVE_ATTRIBUTE_MAX_LENGTH), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_shader, -1, STATE_PROGRAM },
    { "", GL_NONE, NULL_TYPE, 0, BUGLE_GL_ARB_geometry_shader4, -1, STATE_PROGRAM, spawn_program_geometry },
    { "", GL_NONE, NULL_TYPE, 0, BUGLE_GL_ARB_tessellation_shader, -1, STATE_PROGRAM, spawn_program_tess_control },
    { "", GL_NONE, NULL_TYPE, 0, BUGLE_GL_ARB_tessellation_shader, -1, STATE_PROGRAM, spawn_program_tess_evaluation },
    { STATE_NAME(GL_TRANSFORM_FEEDBACK_BUFFER_MODE), TYPE_6GLenum, -1, BUGLE_GL_EXT_transform_feedback, -1, STATE_PROGRAM },
    { STATE_NAME(GL_TRANSFORM_FEEDBACK_VARYINGS), TYPE_5GLint, -1, BUGLE_GL_EXT_transform_feedback, -1, STATE_PROGRAM },
    { STATE_NAME(GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH), TYPE_5GLint, -1, BUGLE_GL_EXT_transform_feedback, -1, STATE_PROGRAM },
    { STATE_NAME(GL_ACTIVE_UNIFORM_BLOCKS), TYPE_5GLint, -1, BUGLE_GL_ARB_uniform_buffer_object, -1, STATE_PROGRAM },
    { STATE_NAME(GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH), TYPE_5GLint, -1, BUGLE_GL_ARB_uniform_buffer_object, -1, STATE_PROGRAM },
    { STATE_NAME(GL_ACTIVE_ATOMIC_COUNTER_BUFFERS), TYPE_5GLint, -1, BUGLE_GL_ARB_shader_atomic_counters, -1, STATE_PROGRAM },
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info shader_state[] =
{
    { STATE_NAME(GL_DELETE_STATUS), TYPE_9GLboolean, -1, BUGLE_GL_ARB_shader_objects, -1, STATE_SHADER },
    { STATE_NAME(GL_INFO_LOG_LENGTH), TYPE_5GLint, -1, BUGLE_GL_ARB_shader_objects, -1, STATE_SHADER },
    { "InfoLog", GL_NONE, TYPE_P6GLchar, -1, BUGLE_GL_ARB_shader_objects, -1, STATE_SHADER_INFO_LOG },
    { "ShaderSource", GL_NONE, TYPE_P6GLchar, -1, BUGLE_GL_ARB_shader_objects, -1, STATE_SHADER_SOURCE },
    { STATE_NAME(GL_SHADER_TYPE), TYPE_6GLenum, -1, BUGLE_GL_ARB_shader_objects, -1, STATE_SHADER },
    { STATE_NAME(GL_COMPILE_STATUS), TYPE_9GLboolean, -1, BUGLE_GL_ARB_shader_objects, -1, STATE_SHADER },
    { STATE_NAME(GL_SHADER_SOURCE_LENGTH), TYPE_5GLint, -1, BUGLE_GL_ARB_shader_objects, -1, STATE_SHADER },
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info generic_enable = { NULL, GL_NONE, TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED };

/* All the make functions _append_ nodes to children. That means you have
 * to initialise the list yourself.
 */
static void make_leaves_conditional(const glstate *self, const state_info *table,
                                    bugle_uint32_t flags, unsigned int mask,
                                    linked_list *children)
{
    glstate *child;
    const state_info *info;

    for (info = table; info->name; info++)
    {
        if ((info->flags & mask) == flags
            && (bugle_gl_has_extension_group(info->extensions))
            && (info->exclude == -1 || !bugle_gl_has_extension_group(info->exclude)))
        {
            if (info->spawn)
                info->spawn(self, children, info);
            else
            {
                child = BUGLE_MALLOC(glstate);
                *child = *self; /* copies contextual info */
                child->name = bugle_strdup(info->name);
                child->numeric_name = 0;
                child->enum_name = info->pname;
                child->info = info;
                child->spawn_children = NULL;
                bugle_list_append(children, child);
            }
        }
    }
}

static void make_leaves(const glstate *self, const state_info *table,
                        linked_list *children)
{
    make_leaves_conditional(self, table, 0, 0, children);
}

static void make_fixed(const glstate *self,
                       const enum_list *enums,
                       size_t offset,
                       void (*spawn)(const glstate *, linked_list *),
                       const state_info *info,
                       linked_list *children)
{
    size_t i;
    glstate *child;

    for (i = 0; enums[i].name; i++)
        if (bugle_gl_has_extension_group(enums[i].extensions))
        {
            child = BUGLE_MALLOC(glstate);
            *child = *self;
            child->info = info;
            child->name = bugle_strdup(enums[i].name);
            child->numeric_name = 0;
            child->enum_name = enums[i].token;
            *(GLenum *) (((char *) child) + offset) = enums[i].token;
            child->spawn_children = spawn;
            bugle_list_append(children, child);
        }
}

static void make_counted2(const glstate *self,
                          GLint count,
                          const char *format,
                          GLenum base,
                          size_t offset1, size_t offset2,
                          void (*spawn)(const glstate *, linked_list *),
                          const state_info *info,
                          linked_list *children)
{
    GLint i;
    glstate *child;

    for (i = 0; i < count; i++)
    {
        child = BUGLE_MALLOC(glstate);
        *child = *self;
        child->info = info;
        child->name = bugle_asprintf(format, (unsigned long) i);
        child->numeric_name = i;
        child->enum_name = 0;
        *(GLenum *) (((char *) child) + offset1) = base + i;
        *(GLenum *) (((char *) child) + offset2) = base + i;
        child->spawn_children = spawn;
        bugle_list_append(children, child);
    }
}

static void make_counted(const glstate *self,
                         GLint count,
                         const char *format,
                         GLenum base,
                         size_t offset,
                         void (*spawn)(const glstate *, linked_list *),
                         const state_info *info,
                         linked_list *children)
{
    make_counted2(self, count, format, base, offset, offset, spawn, info, children);
}

static void make_object(const glstate *self,
                        GLenum target,
                        const char *format,
                        GLuint id,
                        void (*spawn)(const glstate *, linked_list *),
                        const state_info *info,
                        linked_list *children)
{
    glstate *child;

    child = BUGLE_MALLOC(glstate);
    *child = *self;
    child->target = target;
    child->info = info;
    child->name = bugle_asprintf(format, (unsigned long) id);
    child->numeric_name = id;
    child->enum_name = 0;
    child->object = id;
    child->spawn_children = spawn;
    bugle_list_append(children, child);
}

typedef struct
{
    const glstate *self;
    GLenum target;
    const char *format;
    void (*spawn_children)(const glstate *, linked_list *);
    state_info *info;
    linked_list *children;
} make_objects_data;

static void make_objects_walker(GLuint object,
                                GLenum target,
                                void *vdata)
{
    const make_objects_data *data;

    data = (const make_objects_data *) vdata;
    if (data->target != GL_NONE && target != GL_NONE
        && target != data->target) return;
    make_object(data->self, target, data->format, object, data->spawn_children,
                data->info, data->children);
}

static void make_objects(const glstate *self,
                         bugle_globjects_type type,
                         GLenum target,
                         bugle_bool add_zero,
                         const char *format,
                         void (*spawn_children)(const glstate *, linked_list *),
                         state_info *info,
                         linked_list *children)
{
    make_objects_data data;

    data.self = self;
    data.target = target;
    data.format = format;
    data.spawn_children = spawn_children;
    data.info = info;
    data.children = children;

    if (add_zero)
        make_object(self, target, format, 0, spawn_children, info, children);
    bugle_globjects_walk(type, make_objects_walker, &data);
}

static void make_target(const glstate *self,
                        const char *name,
                        GLenum target,
                        GLenum binding,
                        void (*spawn_children)(const glstate *, linked_list *),
                        const state_info *info,
                        linked_list *children)
{
    glstate *child;

    child = BUGLE_MALLOC(glstate);
    *child = *self;
    child->name = bugle_strdup(name);
    child->numeric_name = 0;
    child->enum_name = target;
    child->target = target;
    child->face = target;   /* Changed at next level for cube maps */
    child->binding = binding;
    child->spawn_children = spawn_children;
    child->info = info;
    bugle_list_append(children, child);
}

/* Spawning callback functions. These are called either from the make
 * functions (2-argument functions) or from make_leaves conditional due to
 * a state_info with a callback (3-argument form).
 */

static void spawn_program_geometry(const struct glstate *self,
                                   linked_list *children,
                                   const struct state_info *info)
{
    /* There is no direct way to tell whether the program has a geometry
     * shader (GetAttachedShaders only tells us what is attached now, not
     * what was attached at link time). However, we only care so that we
     * can avoid triggering an error, so we just try one query and catch
     * the error.
     */
    if (bugle_gl_has_extension_group(BUGLE_GL_ARB_geometry_shader4))
    {
        GLint dummy;
        GLenum error;

        CALL(glGetProgramiv)(self->object, GL_GEOMETRY_VERTICES_OUT, &dummy);
        error = CALL(glGetError)();
        if (error == GL_NO_ERROR)
        {
            make_leaves(self, program_geometry_state, children);
        }
    }
}

static void spawn_program_tess_control(const struct glstate *self,
                                       linked_list *children,
                                       const struct state_info *info)
{
    /* See comments in spawn_program_geometry */
    if (bugle_gl_has_extension_group(BUGLE_GL_ARB_tessellation_shader))
    {
        GLint dummy;
        GLenum error;

        CALL(glGetProgramiv)(self->object, GL_TESS_CONTROL_OUTPUT_VERTICES, &dummy);
        error = CALL(glGetError)();
        if (error == GL_NO_ERROR)
        {
            make_leaves(self, program_tess_control_state, children);
        }
    }
}

static void spawn_program_tess_evaluation(const struct glstate *self,
                                          linked_list *children,
                                          const struct state_info *info)
{
    /* See comments in spawn_program_geometry */
    if (bugle_gl_has_extension_group(BUGLE_GL_ARB_tessellation_shader))
    {
        GLint dummy;
        GLenum error;

        CALL(glGetProgramiv)(self->object, GL_TESS_GEN_MODE, &dummy);
        error = CALL(glGetError)();
        if (error == GL_NO_ERROR)
        {
            make_leaves(self, program_tess_evaluation_state, children);
        }
    }
}

static void spawn_children_vertex_attrib(const glstate *self, linked_list *children)
{
    bugle_list_init(children, bugle_free);
    make_leaves_conditional(self, vertex_attrib_state,
                            0,
                            (self->level == 0) ? STATE_SELECT_NON_ZERO : 0,
                            children);
}

static void spawn_vertex_attrib_arrays(const struct glstate *self,
                                       linked_list *children,
                                       const struct state_info *info)
{
    GLint count;
    CALL(glGetIntegerv)(GL_MAX_VERTEX_ATTRIBS, &count);
    make_counted(self, count, "VertexAttrib[%lu]", 0, offsetof(glstate, level), spawn_children_vertex_attrib, NULL, children);
}

/* Like the above two, but for VAOs */
static void spawn_children_vertex_array_attrib(const glstate *self, linked_list *children)
{
    bugle_list_init(children, bugle_free);
    make_leaves_conditional(self, vertex_array_attrib_state,
                            0,
                            (self->level == 0) ? STATE_SELECT_NON_ZERO : 0,
                            children);
}

static void spawn_vertex_array_attrib_arrays(const struct glstate *self,
                                             linked_list *children,
                                             const struct state_info *info)
{
    GLint count;
    CALL(glGetIntegerv)(GL_MAX_VERTEX_ATTRIBS, &count);
    make_counted(self, count, "VertexAttrib[%lu]", 0, offsetof(glstate, level), spawn_children_vertex_array_attrib, NULL, children);
}

static void spawn_clip_planes(const struct glstate *self,
                              linked_list *children,
                              const struct state_info *info)
{
    static const state_info clip_distance =
    {
        NULL, GL_NONE, TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED
    };

    GLint count;
    CALL(glGetIntegerv)(GL_MAX_CLIP_PLANES, &count);
    make_counted2(self, count, "GL_CLIP_PLANE%lu", GL_CLIP_PLANE0,
                  offsetof(glstate, target), offsetof(glstate, enum_name),
                  NULL, info, children);
    make_counted(self, count, "GL_CLIP_DISTANCE%lu", GL_CLIP_DISTANCE0,
                  offsetof(glstate, enum_name), NULL, &clip_distance, children);
}

static void spawn_children_material(const glstate *self, linked_list *children)
{
    bugle_list_init(children, bugle_free);
    make_leaves(self, material_state, children);
}

static void spawn_materials(const struct glstate *self,
                            linked_list *children,
                            const struct state_info *info)
{
    make_fixed(self, material_enums, offsetof(glstate, target),
               spawn_children_material, NULL, children);
}

static void spawn_children_light(const glstate *self, linked_list *children)
{
    bugle_list_init(children, bugle_free);
    make_leaves(self, light_state, children);
}

static void spawn_lights(const glstate *self,
                         linked_list *children,
                         const struct state_info *info)
{
    GLint count;

    CALL(glGetIntegerv)(GL_MAX_LIGHTS, &count);
    make_counted2(self, count, "GL_LIGHT%lu", GL_LIGHT0,
                  offsetof(glstate, target), offsetof(glstate, enum_name),
                  spawn_children_light,
                  &generic_enable, children);
}

static int get_total_texture_units(void)
{
    GLint cur = 0, max = 1;

    if (bugle_gl_has_extension_group(BUGLE_GL_ARB_multitexture))
    {
        CALL(glGetIntegerv)(GL_MAX_TEXTURE_UNITS, &cur);
        if (cur > max) max = cur;
    }
    if (bugle_gl_has_extension_group(BUGLE_EXTGROUP_texunits))
    {
        CALL(glGetIntegerv)(GL_MAX_TEXTURE_IMAGE_UNITS, &cur);
        if (cur > max) max = cur;
        CALL(glGetIntegerv)(GL_MAX_TEXTURE_COORDS, &cur);
        if (cur > max) max = cur;
    }
    if (bugle_gl_has_extension_group(BUGLE_GL_ARB_vertex_shader))
    {
        CALL(glGetIntegerv)(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &cur);
        if (cur > max) max = cur;
    }
    /* NVIDIA 66.29 on NV20 fails on some of these calls. Clear the error. */
    CALL(glGetError)();
    return max;
}

/* Returns a mask of flags not to select from state tables, based on the
 * dimension of the target.
 */
static bugle_uint32_t texture_mask(GLenum target)
{
    bugle_uint32_t mask = 0;

    switch (target)
    {
    case GL_PROXY_TEXTURE_1D: mask |= STATE_SELECT_NO_PROXY; /* Fall through */
    case GL_TEXTURE_1D: mask |= STATE_SELECT_NO_1D; break;
    case GL_PROXY_TEXTURE_2D: mask |= STATE_SELECT_NO_PROXY; /* Fall through */
    case GL_TEXTURE_2D: mask |= STATE_SELECT_NO_2D; break;
    case GL_PROXY_TEXTURE_CUBE_MAP: mask |= STATE_SELECT_NO_PROXY; /* Fall through */
    case GL_TEXTURE_CUBE_MAP: mask |= STATE_SELECT_NO_2D; break;
#ifdef GL_NV_texture_rectangle
    case GL_PROXY_TEXTURE_RECTANGLE_NV: mask |= STATE_SELECT_NO_PROXY; /* Fall through */
    case GL_TEXTURE_RECTANGLE_NV: mask |= STATE_SELECT_NO_2D; break;
#endif
    case GL_PROXY_TEXTURE_3D: mask |= STATE_SELECT_NO_PROXY; break;
    case GL_PROXY_TEXTURE_1D_ARRAY: mask |= STATE_SELECT_NO_PROXY; /* Fall through */
    case GL_TEXTURE_1D_ARRAY: mask |= STATE_SELECT_NO_2D; break;
    case GL_PROXY_TEXTURE_2D_ARRAY: mask |= STATE_SELECT_NO_PROXY; break;
#ifdef GL_EXT_texture_buffer_object
    /* GL_TEXTURE_BUFFER_EXT has no proxy */
    case GL_TEXTURE_BUFFER_EXT: mask |= STATE_SELECT_NO_1D; break;
#endif
    }
    return mask;
}

static unsigned int get_texture_env_units(void)
{
    GLint ans = 1;

    if (bugle_gl_has_extension_group(BUGLE_GL_ARB_multitexture))
        CALL(glGetIntegerv)(GL_MAX_TEXTURE_UNITS_ARB, &ans);
    return ans;
}

static unsigned int get_texture_image_units(void)
{
    GLint cur = 0, max = 1;

    if (bugle_gl_has_extension_group(BUGLE_GL_ARB_multitexture))
    {
        CALL(glGetIntegerv)(GL_MAX_TEXTURE_UNITS, &cur);
        if (cur > max) max = cur;
    }
    if (bugle_gl_has_extension_group(BUGLE_EXTGROUP_texunits))
    {
        CALL(glGetIntegerv)(GL_MAX_TEXTURE_IMAGE_UNITS, &cur);
        if (cur > max) max = cur;
    }
    if (bugle_gl_has_extension_group(BUGLE_GL_ARB_vertex_shader))
    {
        CALL(glGetIntegerv)(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &cur);
        if (cur > max) max = cur;
    }
    /* NVIDIA 66.29 on NV20 fails on some of these calls. Clear the error. */
    CALL(glGetError)();
    return max;
}

static unsigned int get_texture_coord_units(void)
{
    GLint cur = 0, max = 1;

    if (bugle_gl_has_extension_group(BUGLE_GL_ARB_multitexture))
    {
        CALL(glGetIntegerv)(GL_MAX_TEXTURE_UNITS, &cur);
        if (cur > max) max = cur;
    }
    if (bugle_gl_has_extension_group(BUGLE_EXTGROUP_texunits))
    {
        CALL(glGetIntegerv)(GL_MAX_TEXTURE_COORDS, &cur);
        if (cur > max) max = cur;
    }
    /* NVIDIA 66.29 on NV20 fails on some of these calls. Clear the error. */
    CALL(glGetError)();
    return max;
}

static void spawn_children_tex_level_parameter(const glstate *self, linked_list *children)
{
    bugle_uint32_t mask = STATE_SELECT_COMPRESSED;

    if (!(mask & STATE_SELECT_NO_PROXY)
        && bugle_gl_has_extension_group(BUGLE_GL_ARB_texture_compression)
        && bugle_gl_begin_internal_render())
    {
        GLint old, compressed;

        if (self->binding)
        {
            CALL(glGetIntegerv)(self->binding, &old);
            CALL(glBindTexture)(self->target, self->object);
        }
        CALL(glGetTexLevelParameteriv)(self->face, self->level, GL_TEXTURE_COMPRESSED, &compressed);
        if (compressed) mask &= ~STATE_SELECT_COMPRESSED;
        if (self->binding) CALL(glBindTexture)(self->target, old);
        bugle_gl_end_internal_render("spawn_children_tex_level_parameter", BUGLE_TRUE);
    }
    bugle_list_init(children, bugle_free);
    make_leaves_conditional(self, tex_level_parameter_state, 0,
                            mask | texture_mask(self->target), children);
}

static void make_tex_levels(const glstate *self,
                            linked_list *children)
{
    GLint base, max, i;
    glstate *child;
    GLint old;

    if (self->binding)
    {
        CALL(glGetIntegerv)(self->binding, &old);
        CALL(glBindTexture)(self->target, self->object);
    }
    base = 0;
    max = 1000;
    if (self->binding && bugle_gl_has_extension_group(GL_SGIS_texture_lod)) /* No parameters for proxy textures */
    {
        CALL(glGetTexParameteriv)(self->target, GL_TEXTURE_BASE_LEVEL, &base);
        CALL(glGetTexParameteriv)(self->target, GL_TEXTURE_MAX_LEVEL, &max);
    }

    for (i = base; i <= base + max; i++)
    {
        GLint width;
        CALL(glGetTexLevelParameteriv)(self->face, i, GL_TEXTURE_WIDTH, &width);
        if (width <= 0) break;

        child = BUGLE_MALLOC(glstate);
        *child = *self;
        child->name = bugle_asprintf("level[%lu]", (unsigned long) i);
        child->numeric_name = i;
        child->enum_name = 0;
        child->info = NULL;
        child->level = i;
        child->spawn_children = spawn_children_tex_level_parameter;
        bugle_list_append(children, child);
    }

    if (self->binding) CALL(glBindTexture)(self->target, old);
}

static void spawn_children_cube_map_faces(const glstate *self, linked_list *children)
{
    bugle_list_init(children, bugle_free);
    make_tex_levels(self, children);
}

static void spawn_children_tex_parameter(const glstate *self, linked_list *children)
{
    bugle_list_init(children, bugle_free);
    if (self->binding) /* Zero indicates a proxy, which have no texture parameter state */
        make_leaves_conditional(self, tex_parameter_state, 0,
                                texture_mask(self->target), children);
    if (self->target == GL_TEXTURE_CUBE_MAP)
    {
        make_fixed(self, cube_map_face_enums, offsetof(glstate, face),
                   spawn_children_cube_map_faces, NULL, children);
        return;
    }
    make_tex_levels(self, children);
}

static void spawn_children_tex_target(const glstate *self, linked_list *children)
{
    if (self->binding) /* Zero here indicates a proxy target */
    {
        bugle_list_init(children, bugle_free);
        make_objects(self, BUGLE_GLOBJECTS_TEXTURE, self->target, BUGLE_TRUE, "%lu",
                     spawn_children_tex_parameter, NULL, children);
    }
    else
        spawn_children_tex_parameter(self, children);
}

static void spawn_children_tex_buffer(const glstate *self, linked_list *children)
{
    bugle_list_init(children, bugle_free);
    make_leaves(self, tex_buffer_state, children);
}

static void spawn_children_tex_buffer_target(const glstate *self, linked_list *children)
{
    bugle_list_init(children, bugle_free);
    make_objects(self, BUGLE_GLOBJECTS_TEXTURE, self->target, BUGLE_TRUE, "%lu",
                 spawn_children_tex_buffer, NULL, children);
}

static void spawn_children_tex_gen(const glstate *self, linked_list *children)
{
    bugle_list_init(children, bugle_free);
    make_leaves(self, tex_gen_state, children);
}

static void spawn_children_tex_unit(const glstate *self, linked_list *children)
{
    linked_list_node *i;
    glstate *child;
    bugle_uint32_t mask = 0;

    bugle_list_init(children, bugle_free);
    if (self->unit >= GL_TEXTURE0 + get_texture_env_units())
        mask |= STATE_SELECT_TEXTURE_ENV;
    if (self->unit >= GL_TEXTURE0 + get_texture_coord_units())
        mask |= STATE_SELECT_TEXTURE_COORD;
    if (self->unit >= GL_TEXTURE0 + get_texture_image_units())
        mask |= STATE_SELECT_TEXTURE_IMAGE;
    make_leaves_conditional(self, tex_unit_state, 0, mask, children);
    for (i = bugle_list_head(children); i; i = bugle_list_next(i))
    {
        child = (glstate *) bugle_list_data(i);
        switch (child->info->flags & STATE_MODE_MASK)
        {
        case STATE_MODE_TEXTURE_ENV:
            child->target = GL_TEXTURE_ENV;
            break;
        case STATE_MODE_TEXTURE_FILTER_CONTROL:
            child->target = GL_TEXTURE_FILTER_CONTROL;
            break;
        case STATE_MODE_POINT_SPRITE:
            child->target = GL_POINT_SPRITE;
            break;
        }
    }

    if (!(mask & STATE_SELECT_TEXTURE_COORD))
        make_fixed(self, tex_gen_enums, offsetof(glstate, target),
                   spawn_children_tex_gen, NULL, children);
}

static void spawn_texture_units(const glstate *self,
                                linked_list *children,
                                const struct state_info *info)
{
    if (bugle_gl_has_extension_group(BUGLE_GL_ARB_multitexture))
    {
        GLint count;
        count = get_total_texture_units();
        make_counted2(self, count, "GL_TEXTURE%lu", GL_TEXTURE0,
                      offsetof(glstate, unit), offsetof(glstate, enum_name),
                      spawn_children_tex_unit,
                      NULL, children);
    }
    else
    {
        make_leaves(self, tex_unit_state, children);
    }
}

static void spawn_textures(const glstate *self,
                           linked_list *children,
                           const struct state_info *info)
{
#ifdef GL_EXT_texture_buffer_object
    static const state_info texture_buffer =
    {
        NULL, GL_NONE, TYPE_5GLint, -1, BUGLE_GL_EXT_texture_buffer_object, -1, STATE_GLOBAL
    };
#endif

    make_target(self, "GL_TEXTURE_1D", GL_TEXTURE_1D,
                GL_TEXTURE_BINDING_1D, spawn_children_tex_target, NULL, children);
    make_target(self, "GL_PROXY_TEXTURE_1D", GL_PROXY_TEXTURE_1D,
                0, spawn_children_tex_target, NULL, children);
    make_target(self, "GL_TEXTURE_2D", GL_TEXTURE_2D,
                GL_TEXTURE_BINDING_2D, spawn_children_tex_target, NULL, children);
    make_target(self, "GL_PROXY_TEXTURE_2D", GL_PROXY_TEXTURE_2D,
                0, spawn_children_tex_target, NULL, children);
    if (bugle_gl_has_extension_group(BUGLE_GL_VERSION_1_2))
    {
        make_target(self, "GL_TEXTURE_3D", GL_TEXTURE_3D,
                        GL_TEXTURE_BINDING_3D, spawn_children_tex_target, NULL, children);
        make_target(self, "GL_PROXY_TEXTURE_3D", GL_PROXY_TEXTURE_3D,
                        0, spawn_children_tex_target, NULL, children);
    }
    if (bugle_gl_has_extension_group(BUGLE_GL_ARB_texture_cube_map))
    {
        make_target(self, "GL_TEXTURE_CUBE_MAP",
                    GL_TEXTURE_CUBE_MAP,
                    GL_TEXTURE_BINDING_CUBE_MAP,
                    spawn_children_tex_target, NULL, children);
        make_target(self, "GL_PROXY_TEXTURE_CUBE_MAP",
                    GL_PROXY_TEXTURE_CUBE_MAP,
                    0,
                    spawn_children_tex_target, NULL, children);
    }
    if (bugle_gl_has_extension_group(BUGLE_GL_NV_texture_rectangle))
    {
        make_target(self, "GL_TEXTURE_RECTANGLE",
                    GL_TEXTURE_RECTANGLE,
                    GL_TEXTURE_BINDING_RECTANGLE,
                    spawn_children_tex_target, NULL, children);
        make_target(self, "GL_PROXY_TEXTURE_RECTANGLE",
                    GL_PROXY_TEXTURE_RECTANGLE,
                    0,
                    spawn_children_tex_target, NULL, children);
    }
    if (bugle_gl_has_extension_group(BUGLE_GL_EXT_texture_array))
    {
        make_target(self, "GL_TEXTURE_1D_ARRAY",
                    GL_TEXTURE_1D_ARRAY,
                    GL_TEXTURE_BINDING_1D_ARRAY,
                    spawn_children_tex_target, NULL, children);
        make_target(self, "GL_PROXY_TEXTURE_1D_ARRAY",
                    GL_PROXY_TEXTURE_1D_ARRAY,
                    0,
                    spawn_children_tex_target, NULL, children);
        make_target(self, "GL_TEXTURE_2D_ARRAY",
                    GL_TEXTURE_2D_ARRAY,
                    GL_TEXTURE_BINDING_2D_ARRAY,
                    spawn_children_tex_target, NULL, children);
        make_target(self, "GL_PROXY_TEXTURE_2D_ARRAY",
                    GL_PROXY_TEXTURE_2D_ARRAY,
                    0,
                    spawn_children_tex_target, NULL, children);
    }
#ifdef GL_EXT_texture_buffer_object
    if (bugle_gl_has_extension_group(BUGLE_GL_EXT_texture_buffer_object))
    {
        make_target(self, "GL_TEXTURE_BUFFER_EXT",
                    GL_TEXTURE_BUFFER_EXT,
                    GL_TEXTURE_BINDING_BUFFER_EXT,
                    spawn_children_tex_buffer_target, &texture_buffer, children);
    }
#endif
}

static void spawn_children_blend(const struct glstate *self,
                                 linked_list *children)
{
#ifdef GL_EXT_draw_buffers2
    static const state_info blend = { STATE_NAME(GL_BLEND), TYPE_9GLboolean, -1, BUGLE_GL_EXT_draw_buffers2, -1, STATE_ENABLED_INDEXED };

    GLint count;
    CALL(glGetIntegerv)(GL_MAX_DRAW_BUFFERS_ATI, &count);
    bugle_list_init(children, bugle_free);
    make_counted(self, count, "%lu", 0, offsetof(glstate, numeric_name), NULL,
                 &blend, children);
#endif
}

static void spawn_blend(const struct glstate *self,
                        linked_list *children,
                        const struct state_info *info)
{
    make_target(self, "GL_BLEND", GL_BLEND, GL_BLEND,
                spawn_children_blend, NULL, children);
}

static void spawn_children_color_writemask(const struct glstate *self,
                                           linked_list *children)
{
    static const state_info color_writemask = { STATE_NAME(GL_COLOR_WRITEMASK), TYPE_9GLboolean, 4, BUGLE_GL_EXT_draw_buffers2, -1, STATE_INDEXED };

    GLint count;
    CALL(glGetIntegerv)(GL_MAX_DRAW_BUFFERS, &count);
    bugle_list_init(children, bugle_free);
    make_counted(self, count, "%lu", 0, offsetof(glstate, numeric_name), NULL,
                 &color_writemask, children);
}

static void spawn_color_writemask(const struct glstate *self,
                                  linked_list *children,
                                  const struct state_info *info)
{
    make_target(self, "GL_COLOR_WRITEMASK", GL_COLOR_WRITEMASK, GL_COLOR_WRITEMASK,
                spawn_children_color_writemask, NULL, children);
}

static void spawn_children_extensions(const struct glstate *self,
                                      linked_list *children)
{
    static const state_info extension =
    {
        STATE_NAME(GL_EXTENSIONS), TYPE_PKc, -1, BUGLE_GL_VERSION_3_0, -1, STATE_INDEXED
    };
    GLint count = 0;
    CALL(glGetIntegerv)(GL_NUM_EXTENSIONS, &count);
    bugle_list_init(children, bugle_free);
    make_counted(self, count, "%lu", 0, offsetof(glstate, level), NULL, &extension, children);
}

static void spawn_extensions(const struct glstate *self,
                             linked_list *children,
                             const struct state_info *info)
{
    const bugle_bool gl3 = bugle_gl_has_extension_group(BUGLE_GL_VERSION_3_0);
    make_target(self, "GL_EXTENSIONS", GL_EXTENSIONS, GL_EXTENSIONS,
                gl3 ? spawn_children_extensions : NULL, info, children);
}

static void spawn_draw_buffers(const struct glstate *self,
                               linked_list *children,
                               const struct state_info *info)
{
    GLint count;
    CALL(glGetIntegerv)(GL_MAX_DRAW_BUFFERS, &count);
    make_counted(self, count, "GL_DRAW_BUFFER%lu", GL_DRAW_BUFFER0,
                 offsetof(glstate, enum_name), NULL,
                 info, children);
}

static void spawn_children_color_table_parameter(const glstate *self, linked_list *children)
{
    bugle_list_init(children, bugle_free);
    switch (self->target)
    {
    case GL_COLOR_TABLE:
    case GL_POST_CONVOLUTION_COLOR_TABLE:
    case GL_POST_COLOR_MATRIX_COLOR_TABLE:
        make_leaves(self, color_table_parameter_state, children);
        break;
    case GL_PROXY_COLOR_TABLE:
    case GL_PROXY_POST_CONVOLUTION_COLOR_TABLE:
    case GL_PROXY_POST_COLOR_MATRIX_COLOR_TABLE:
        make_leaves_conditional(self, color_table_parameter_state, 0,
                                STATE_SELECT_NO_PROXY, children);
        break;
    }
}

static void spawn_color_tables(const struct glstate *self,
                               linked_list *children,
                               const struct state_info *info)
{
    make_fixed(self, color_table_parameter_enums, offsetof(glstate, target),
               spawn_children_color_table_parameter, &generic_enable, children);
    make_fixed(self, proxy_color_table_parameter_enums, offsetof(glstate, target),
               spawn_children_color_table_parameter, NULL, children);
}

static void spawn_children_convolution_parameter(const glstate *self, linked_list *children)
{
    bugle_list_init(children, bugle_free);
    if (self->target == GL_CONVOLUTION_1D)
        make_leaves_conditional(self, convolution_parameter_state,
                                0, STATE_SELECT_NO_1D, children);
    else
        make_leaves(self, convolution_parameter_state, children);
}

static void spawn_convolutions(const struct glstate *self,
                               linked_list *children,
                               const struct state_info *info)
{
    make_fixed(self, convolution_parameter_enums, offsetof(glstate, target),
               spawn_children_convolution_parameter, &generic_enable, children);
}

static void spawn_children_histogram_parameter(const glstate *self, linked_list *children)
{
    bugle_list_init(children, bugle_free);
    make_leaves(self, histogram_parameter_state, children);
}

static void spawn_histograms(const struct glstate *self,
                               linked_list *children,
                             const struct state_info *info)
{
    make_fixed(self, histogram_parameter_enums, offsetof(glstate, target),
               spawn_children_histogram_parameter, &generic_enable, children);
    make_fixed(self, proxy_histogram_parameter_enums, offsetof(glstate, target),
               spawn_children_histogram_parameter, NULL, children);
}

static void spawn_children_minmax_parameter(const glstate *self, linked_list *children)
{
    bugle_list_init(children, bugle_free);
    make_leaves(self, minmax_parameter_state, children);
}

static void spawn_minmax(const struct glstate *self,
                         linked_list *children,
                         const struct state_info *info)
{
    make_fixed(self, minmax_parameter_enums, offsetof(glstate, target),
               spawn_children_minmax_parameter, &generic_enable, children);
}

static void spawn_children_shader(const glstate *self, linked_list *children)
{
    bugle_list_init(children, bugle_free);
    make_leaves(self, shader_state, children);
}

static void spawn_children_program(const glstate *self, linked_list *children)
{
    GLint i, count, max, status;
    GLenum type;
    GLsizei length, size;
    glstate *child;
    static const state_info uniform_state =
    {
        NULL, GL_NONE, NULL_TYPE, -1, BUGLE_GL_ARB_shader_objects, -1, STATE_UNIFORM
    };
    static const state_info attrib_state =
    {
        NULL, GL_NONE, TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_shader, -1, STATE_ATTRIB_LOCATION
    };

    bugle_list_init(children, bugle_free);
    make_leaves(self, program_state, children);

    bugle_glGetProgramiv(self->object, GL_ACTIVE_UNIFORMS, &count);
    bugle_glGetProgramiv(self->object, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max);
    /* If the program failed to link, then uniforms cannot be queried */
    bugle_glGetProgramiv(self->object, GL_LINK_STATUS, &status);
    if (status != GL_FALSE)
        for (i = 0; i < count; i++)
        {
            child = BUGLE_MALLOC(glstate);
            *child = *self;
            child->spawn_children = NULL;
            child->info = &uniform_state;
            child->name = BUGLE_NMALLOC(max, GLcharARB);
            child->name[0] = '\0'; /* sanity, in case the query borks */
            child->enum_name = 0;
            child->level = i;      /* level is overloaded with the uniform index */
            bugle_glGetActiveUniform(self->object, i, max, &length, &size,
                                     &type, child->name);
            if (length)
            {
                child->numeric_name = bugle_glGetUniformLocation(self->object, child->name);
                /* Check for built-in state, which is returned by glGetActiveUniformARB
                 * but cannot be queried.
                 */
                if (child->numeric_name == -1) length = 0;
            }
            if (length) bugle_list_append(children, child);
            else bugle_free(child->name); /* failed query */
        }

    if (status != GL_FALSE && bugle_gl_has_extension_group(BUGLE_GL_ARB_vertex_shader))
    {
        bugle_glGetProgramiv(self->object, GL_ACTIVE_ATTRIBUTES,
                             &count);
        bugle_glGetProgramiv(self->object, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH,
                             &max);

        for (i = 0; i < count; i++)
        {
            child = BUGLE_MALLOC(glstate);
            *child = *self;
            child->spawn_children = NULL;
            child->info = &attrib_state;
            child->name = BUGLE_NMALLOC(max + 1, GLcharARB);
            child->name[0] = '\0';
            child->numeric_name = i;
            child->enum_name = 0;
            child->level = i;
            bugle_glGetActiveAttrib(self->object, i, max, &length, &size,
                                    &type, child->name);
            if (length) bugle_list_append(children, child);
            else bugle_free(child->name);
        }
    }
}

static void spawn_shaders(const struct glstate *self,
                          linked_list *children,
                          const struct state_info *info)
{
    make_objects(self, BUGLE_GLOBJECTS_SHADER, GL_NONE, BUGLE_FALSE,
                 "Shader[%lu]", spawn_children_shader, NULL, children);
}

static void spawn_programs(const struct glstate *self,
                           linked_list *children,
                           const struct state_info *info)
{
    make_objects(self, BUGLE_GLOBJECTS_PROGRAM, GL_NONE, BUGLE_FALSE,
                 "Program[%lu]", spawn_children_program, NULL, children);
}

static void spawn_children_sample_mask_value(const struct glstate *self,
                                             linked_list *children)
{
    static const state_info sample_mask_value =
    {
        NULL, GL_NONE, TYPE_9GLboolean, -1, BUGLE_GL_ARB_texture_multisample, -1, STATE_SAMPLE_MASK_VALUE
    };

    GLint samples = 0;
    CALL(glGetIntegerv)(GL_MAX_SAMPLES, &samples);
    bugle_list_init(children, bugle_free);
    make_counted(self, samples, "%lu", 0, offsetof(glstate, level), NULL, &sample_mask_value, children);
}

static void spawn_sample_mask_value(const struct glstate *self,
                                    linked_list *children,
                                    const struct state_info *info)
{
    make_target(self, "GL_SAMPLE_MASK_VALUE", GL_SAMPLE_MASK_VALUE, GL_SAMPLE_MASK_VALUE,
                spawn_children_sample_mask_value, NULL, children);
}

static void spawn_children_sample_position(const struct glstate *self,
                                           linked_list *children)
{
    static const state_info sample_position =
    {
        NULL, GL_NONE, TYPE_7GLfloat, 2, BUGLE_GL_ARB_texture_multisample, -1, STATE_MULTISAMPLE
    };

    GLuint old = bugle_gl_get_draw_framebuffer_binding();
    bugle_gl_bind_draw_framebuffer(self->object);

    GLint samples = 0;
    CALL(glGetIntegerv)(GL_SAMPLES, &samples);
    bugle_list_init(children, bugle_free);
    make_counted(self, samples, "%lu", 0, offsetof(glstate, level), NULL, &sample_position, children);

    bugle_gl_bind_draw_framebuffer(old);
}

static void spawn_sample_position(const struct glstate *self,
                                  linked_list *children,
                                  const struct state_info *info)
{
    make_target(self, "GL_SAMPLE_POSITION", GL_SAMPLE_POSITION, GL_SAMPLE_POSITION,
                spawn_children_sample_position, NULL, children);
}

/*** Main state table ***/

static const state_info global_state[] =
{
    { STATE_NAME(GL_CURRENT_COLOR), TYPE_8GLdouble, 4, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_CURRENT_SECONDARY_COLOR), TYPE_8GLdouble, 4, BUGLE_GL_EXT_secondary_color, -1, STATE_GLOBAL },
    { STATE_NAME(GL_CURRENT_INDEX), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_CURRENT_NORMAL), TYPE_8GLdouble, 3, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_CURRENT_FOG_COORD), TYPE_8GLdouble, -1, BUGLE_GL_EXT_fog_coord, -1, STATE_GLOBAL },
    { STATE_NAME(GL_CURRENT_RASTER_POSITION), TYPE_8GLdouble, 4, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_CURRENT_RASTER_DISTANCE), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_CURRENT_RASTER_COLOR), TYPE_8GLdouble, 4, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_CURRENT_RASTER_SECONDARY_COLOR), TYPE_8GLdouble, 4, BUGLE_GL_VERSION_2_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_CURRENT_RASTER_INDEX), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_CURRENT_RASTER_POSITION_VALID), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_EDGE_FLAG), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_PATCH_VERTICES), TYPE_5GLint, -1, BUGLE_GL_ARB_tessellation_shader, -1, STATE_GLOBAL },
    { STATE_NAME(GL_PATCH_DEFAULT_OUTER_LEVEL), TYPE_7GLfloat, 4, BUGLE_GL_ARB_tessellation_shader, -1, STATE_GLOBAL },
    { STATE_NAME(GL_PATCH_DEFAULT_INNER_LEVEL), TYPE_7GLfloat, 2, BUGLE_GL_ARB_tessellation_shader, -1, STATE_GLOBAL },
    { STATE_NAME(GL_CLIENT_ACTIVE_TEXTURE), TYPE_6GLenum, -1, BUGLE_GL_ARB_multitexture, -1, STATE_GLOBAL },
    { STATE_NAME(GL_VERTEX_ARRAY), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_VERTEX_ARRAY_SIZE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_VERTEX_ARRAY_TYPE), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_VERTEX_ARRAY_STRIDE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_VERTEX_ARRAY_POINTER), TYPE_P6GLvoid, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_NORMAL_ARRAY), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_NORMAL_ARRAY_TYPE), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_NORMAL_ARRAY_STRIDE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_NORMAL_ARRAY_POINTER), TYPE_P6GLvoid, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_FOG_COORD_ARRAY), TYPE_9GLboolean, -1, BUGLE_GL_EXT_fog_coord, -1, STATE_ENABLED },
    { STATE_NAME(GL_FOG_COORD_ARRAY_TYPE), TYPE_6GLenum, -1, BUGLE_GL_EXT_fog_coord, -1, STATE_GLOBAL },
    { STATE_NAME(GL_FOG_COORD_ARRAY_STRIDE), TYPE_5GLint, -1, BUGLE_GL_EXT_fog_coord, -1, STATE_GLOBAL },
    { STATE_NAME(GL_FOG_COORD_ARRAY_POINTER), TYPE_P6GLvoid, -1, BUGLE_GL_EXT_fog_coord, -1, STATE_GLOBAL },
    { STATE_NAME(GL_COLOR_ARRAY), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_COLOR_ARRAY_SIZE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_COLOR_ARRAY_TYPE), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_COLOR_ARRAY_STRIDE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_COLOR_ARRAY_POINTER), TYPE_P6GLvoid, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_SECONDARY_COLOR_ARRAY), TYPE_9GLboolean, -1, BUGLE_GL_EXT_secondary_color, -1, STATE_ENABLED },
    { STATE_NAME(GL_SECONDARY_COLOR_ARRAY_SIZE), TYPE_5GLint, -1, BUGLE_GL_EXT_secondary_color, -1, STATE_GLOBAL },
    { STATE_NAME(GL_SECONDARY_COLOR_ARRAY_TYPE), TYPE_6GLenum, -1, BUGLE_GL_EXT_secondary_color, -1, STATE_GLOBAL },
    { STATE_NAME(GL_SECONDARY_COLOR_ARRAY_STRIDE), TYPE_5GLint, -1, BUGLE_GL_EXT_secondary_color, -1, STATE_GLOBAL },
    { STATE_NAME(GL_SECONDARY_COLOR_ARRAY_POINTER), TYPE_P6GLvoid, -1, BUGLE_GL_EXT_secondary_color, -1, STATE_GLOBAL },
    { STATE_NAME(GL_INDEX_ARRAY), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_INDEX_ARRAY_TYPE), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_INDEX_ARRAY_STRIDE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_INDEX_ARRAY_POINTER), TYPE_P6GLvoid, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_EDGE_FLAG_ARRAY), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_EDGE_FLAG_ARRAY_STRIDE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_EDGE_FLAG_ARRAY_POINTER), TYPE_P6GLvoid, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { "Attribute Arrays", GL_NONE, NULL_TYPE, -1, BUGLE_EXTGROUP_vertex_attrib, -1, 0, spawn_vertex_attrib_arrays },
    { STATE_NAME(GL_ARRAY_BUFFER_BINDING), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_buffer_object, -1, STATE_GLOBAL },
    { STATE_NAME(GL_VERTEX_ARRAY_BUFFER_BINDING), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_buffer_object, -1, STATE_GLOBAL },
    { STATE_NAME(GL_NORMAL_ARRAY_BUFFER_BINDING), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_buffer_object, -1, STATE_GLOBAL },
    { STATE_NAME(GL_COLOR_ARRAY_BUFFER_BINDING), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_buffer_object, -1, STATE_GLOBAL },
    { STATE_NAME(GL_INDEX_ARRAY_BUFFER_BINDING), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_buffer_object, -1, STATE_GLOBAL },
    { STATE_NAME(GL_EDGE_FLAG_ARRAY_BUFFER_BINDING), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_buffer_object, -1, STATE_GLOBAL },
    { STATE_NAME(GL_SECONDARY_COLOR_ARRAY_BUFFER_BINDING), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_buffer_object, -1, STATE_GLOBAL },
    { STATE_NAME(GL_FOG_COORDINATE_ARRAY_BUFFER_BINDING), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_buffer_object, -1, STATE_GLOBAL },
    { STATE_NAME(GL_ELEMENT_ARRAY_BUFFER_BINDING), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_buffer_object, -1, STATE_GLOBAL },
    { STATE_NAME(GL_DRAW_INDIRECT_BUFFER_BINDING), TYPE_5GLint, -1, BUGLE_GL_ARB_draw_indirect, -1, STATE_GLOBAL },
    { STATE_NAME(GL_VERTEX_ARRAY_BINDING), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_array_object, -1, STATE_GLOBAL },
    { STATE_NAME(GL_PRIMITIVE_RESTART), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_3_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_PRIMITIVE_RESTART_INDEX), TYPE_5GLint, -1, BUGLE_GL_VERSION_3_1, -1, STATE_GLOBAL },
    /* FIXME: these are matrix stacks, not just single matrices */
    { STATE_NAME(GL_COLOR_MATRIX), TYPE_8GLdouble, 16, BUGLE_GL_ARB_imaging, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MODELVIEW_MATRIX), TYPE_8GLdouble, 16, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_PROJECTION_MATRIX), TYPE_8GLdouble, 16, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_VIEWPORT), TYPE_5GLint, 4, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_DEPTH_RANGE), TYPE_8GLdouble, 2, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_COLOR_MATRIX_STACK_DEPTH), TYPE_5GLint, -1, BUGLE_GL_ARB_imaging, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MODELVIEW_STACK_DEPTH), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_PROJECTION_STACK_DEPTH), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MATRIX_MODE), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_NORMALIZE), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_RESCALE_NORMAL), TYPE_9GLboolean, -1, BUGLE_GL_EXT_rescale_normal, -1, STATE_ENABLED },
    /* FIXME: clip plane enables */
    { STATE_NAME(GL_CLIP_PLANE0), TYPE_8GLdouble, 4, BUGLE_GL_VERSION_1_1, -1, STATE_CLIP_PLANE, spawn_clip_planes },
    { STATE_NAME(GL_DEPTH_CLAMP), TYPE_9GLboolean, -1, BUGLE_GL_ARB_depth_clamp, -1, STATE_ENABLED },
    { STATE_NAME(GL_TRANSFORM_FEEDBACK_BINDING), TYPE_5GLint, -1, BUGLE_GL_ARB_transform_feedback2, -1, STATE_GLOBAL },
    { STATE_NAME(GL_FOG_COLOR), TYPE_8GLdouble, 4, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_FOG_INDEX), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_FOG_DENSITY), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_FOG_START), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_FOG_END), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_FOG_MODE), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_FOG), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_FOG_COORD_SRC), TYPE_6GLenum, -1, BUGLE_GL_EXT_fog_coord, -1, STATE_GLOBAL },
    { STATE_NAME(GL_COLOR_SUM), TYPE_9GLboolean, -1, BUGLE_GL_EXT_secondary_color, -1, STATE_ENABLED },
    { STATE_NAME(GL_SHADE_MODEL), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_CLAMP_VERTEX_COLOR), TYPE_6GLenum, -1, BUGLE_GL_ARB_color_buffer_float, -1, STATE_GLOBAL },
    { STATE_NAME(GL_CLAMP_FRAGMENT_COLOR), TYPE_6GLenum, -1, BUGLE_GL_ARB_color_buffer_float, -1, STATE_GLOBAL },
    { STATE_NAME(GL_CLAMP_READ_COLOR), TYPE_6GLenum, -1, BUGLE_GL_ARB_color_buffer_float, -1, STATE_GLOBAL },
    { STATE_NAME(GL_PROVOKING_VERTEX), TYPE_6GLenum, -1, BUGLE_GL_ARB_provoking_vertex, -1, STATE_GLOBAL },
    { STATE_NAME(GL_LIGHTING), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_COLOR_MATERIAL), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_COLOR_MATERIAL_PARAMETER), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_COLOR_MATERIAL_FACE), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { "Materials", 0, NULL_TYPE, -1, BUGLE_GL_VERSION_1_1, -1, 0, spawn_materials },
    { STATE_NAME(GL_LIGHT_MODEL_AMBIENT), TYPE_8GLdouble, 4, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_LIGHT_MODEL_LOCAL_VIEWER), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_LIGHT_MODEL_TWO_SIDE), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_LIGHT_MODEL_COLOR_CONTROL), TYPE_6GLenum, -1, BUGLE_GL_EXT_separate_specular_color, -1, STATE_GLOBAL },
    { "Lights", 0, NULL_TYPE, -1, BUGLE_GL_VERSION_1_1, -1, 0, spawn_lights },
    { STATE_NAME(GL_RASTERIZER_DISCARD), TYPE_9GLboolean, -1, BUGLE_GL_EXT_transform_feedback, -1, STATE_ENABLED },
    { STATE_NAME(GL_POINT_SIZE), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_POINT_SMOOTH), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_POINT_SPRITE), TYPE_9GLboolean, -1, BUGLE_GL_ARB_point_sprite, -1, STATE_ENABLED },
    { STATE_NAME(GL_POINT_SIZE_MIN), TYPE_8GLdouble, -1, BUGLE_GL_EXT_point_parameters, -1, STATE_GLOBAL },
    { STATE_NAME(GL_POINT_SIZE_MAX), TYPE_8GLdouble, -1, BUGLE_GL_EXT_point_parameters, -1, STATE_GLOBAL },
    { STATE_NAME(GL_POINT_FADE_THRESHOLD_SIZE), TYPE_8GLdouble, -1, BUGLE_GL_EXT_point_parameters, -1, STATE_GLOBAL },
    { STATE_NAME(GL_POINT_DISTANCE_ATTENUATION), TYPE_8GLdouble, 3, BUGLE_GL_EXT_point_parameters, -1, STATE_GLOBAL },
    { STATE_NAME(GL_POINT_SPRITE_COORD_ORIGIN), TYPE_6GLenum, -1, BUGLE_GL_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_LINE_WIDTH), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_LINE_SMOOTH), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_LINE_STIPPLE_PATTERN), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_LINE_STIPPLE_REPEAT), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_LINE_STIPPLE), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_CULL_FACE), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_CULL_FACE_MODE), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_FRONT_FACE), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_POLYGON_SMOOTH), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_POLYGON_MODE), TYPE_6GLenum, 2, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_POLYGON_OFFSET_FACTOR), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_POLYGON_OFFSET_UNITS), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_POLYGON_OFFSET_POINT), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_POLYGON_OFFSET_LINE), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_POLYGON_OFFSET_FILL), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_POLYGON_STIPPLE), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { "PolygonStipple", GL_NONE, TYPE_16GLpolygonstipple, -1, BUGLE_GL_VERSION_1_1, -1, STATE_POLYGON_STIPPLE },
    { STATE_NAME(GL_MULTISAMPLE), TYPE_9GLboolean, -1, BUGLE_GL_ARB_multisample, -1, STATE_ENABLED },
    { STATE_NAME(GL_SAMPLE_ALPHA_TO_COVERAGE), TYPE_9GLboolean, -1, BUGLE_GL_ARB_multisample, -1, STATE_ENABLED },
    { STATE_NAME(GL_SAMPLE_ALPHA_TO_ONE), TYPE_9GLboolean, -1, BUGLE_GL_ARB_multisample, -1, STATE_ENABLED },
    { STATE_NAME(GL_SAMPLE_COVERAGE), TYPE_9GLboolean, -1, BUGLE_GL_ARB_multisample, -1, STATE_ENABLED },
    { STATE_NAME(GL_SAMPLE_COVERAGE_VALUE), TYPE_8GLdouble, -1, BUGLE_GL_ARB_multisample, -1, STATE_GLOBAL },
    { STATE_NAME(GL_SAMPLE_COVERAGE_INVERT), TYPE_9GLboolean, -1, BUGLE_GL_ARB_multisample, -1, STATE_GLOBAL },
    { STATE_NAME(GL_SAMPLE_SHADING), TYPE_9GLboolean, -1, BUGLE_GL_ARB_sample_shading, -1, STATE_ENABLED },
    { STATE_NAME(GL_MIN_SAMPLE_SHADING_VALUE), TYPE_7GLfloat, -1, BUGLE_GL_ARB_sample_shading, -1, STATE_GLOBAL },
    { STATE_NAME(GL_SAMPLE_MASK), TYPE_9GLboolean, -1, BUGLE_GL_ARB_texture_multisample, -1, STATE_ENABLED },
    { STATE_NAME(GL_SAMPLE_MASK_VALUE), TYPE_9GLboolean, -1, BUGLE_GL_ARB_texture_multisample, -1, STATE_GLOBAL, spawn_sample_mask_value },
    { "Texture units", GL_TEXTURE0, NULL_TYPE, -1, BUGLE_GL_VERSION_1_1, -1, 0, spawn_texture_units },
    { "Textures", GL_TEXTURE_2D, NULL_TYPE, -1, BUGLE_GL_VERSION_1_1, -1, 0, spawn_textures },
    { STATE_NAME(GL_ACTIVE_TEXTURE), TYPE_6GLenum, -1, BUGLE_GL_ARB_multitexture, -1, STATE_GLOBAL },
    { STATE_NAME(GL_SCISSOR_TEST), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_SCISSOR_BOX), TYPE_5GLint, 4, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_ALPHA_TEST), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_ALPHA_TEST_FUNC), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_ALPHA_TEST_REF), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_TEST), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_STENCIL_FUNC), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_VALUE_MASK), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_REF), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_FAIL), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_PASS_DEPTH_FAIL), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_PASS_DEPTH_PASS), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_BACK_FUNC), TYPE_6GLenum, -1, BUGLE_GL_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_BACK_VALUE_MASK), TYPE_5GLint, -1, BUGLE_GL_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_BACK_REF), TYPE_5GLint, -1, BUGLE_GL_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_BACK_FAIL), TYPE_6GLenum, -1, BUGLE_GL_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_BACK_PASS_DEPTH_FAIL), TYPE_6GLenum, -1, BUGLE_GL_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_BACK_PASS_DEPTH_PASS), TYPE_6GLenum, -1, BUGLE_GL_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_DEPTH_TEST), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_DEPTH_FUNC), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_BLEND), TYPE_9GLboolean, -1, BUGLE_GL_EXT_draw_buffers2, -1, STATE_ENABLED, spawn_blend },
    /* The fallback in case EXT_draw_buffers2 is missing */
    { STATE_NAME(GL_BLEND), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, BUGLE_GL_EXT_draw_buffers2, STATE_ENABLED },
    { STATE_NAME(GL_BLEND_SRC_RGB), TYPE_11GLblendenum, -1, BUGLE_GL_EXT_blend_func_separate, -1, STATE_GLOBAL },
    { STATE_NAME(GL_BLEND_SRC_ALPHA), TYPE_11GLblendenum, -1, BUGLE_GL_EXT_blend_func_separate, -1, STATE_GLOBAL },
    { STATE_NAME(GL_BLEND_DST_RGB), TYPE_11GLblendenum, -1, BUGLE_GL_EXT_blend_func_separate, -1, STATE_GLOBAL },
    { STATE_NAME(GL_BLEND_DST_ALPHA), TYPE_11GLblendenum, -1, BUGLE_GL_EXT_blend_func_separate, -1, STATE_GLOBAL },
    { STATE_NAME(GL_BLEND_SRC), TYPE_11GLblendenum, -1, BUGLE_GL_VERSION_1_1, BUGLE_GL_EXT_blend_func_separate, STATE_GLOBAL },
    { STATE_NAME(GL_BLEND_DST), TYPE_11GLblendenum, -1, BUGLE_GL_VERSION_1_1, BUGLE_GL_EXT_blend_func_separate, STATE_GLOBAL },
    { STATE_NAME(GL_BLEND_EQUATION_RGB), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_BLEND_EQUATION_ALPHA), TYPE_6GLenum, -1, BUGLE_GL_EXT_blend_equation_separate, -1, STATE_GLOBAL },
    { STATE_NAME(GL_BLEND_COLOR), TYPE_8GLdouble, 4, BUGLE_EXTGROUP_blend_color, -1, STATE_GLOBAL },
    /* EXT_framebuffer_sRGB is not a strict subset of ARB_framebuffer_sRGB, hence
     * the two variants of this state (one suppressing the other).
     */
    { STATE_NAME(GL_FRAMEBUFFER_SRGB_EXT), TYPE_9GLboolean, -1, BUGLE_GL_EXT_framebuffer_sRGB, BUGLE_GL_ARB_framebuffer_sRGB, STATE_ENABLED },
    { STATE_NAME(GL_FRAMEBUFFER_SRGB), TYPE_9GLboolean, -1, BUGLE_GL_ARB_framebuffer_sRGB, -1, STATE_ENABLED },
    { STATE_NAME(GL_DITHER), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_INDEX_LOGIC_OP), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_COLOR_LOGIC_OP), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_LOGIC_OP_MODE), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_DRAW_BUFFER), TYPE_6GLenum, -1, GL_VERSION_1_1, BUGLE_GL_ATI_draw_buffers, STATE_GLOBAL },
    { STATE_NAME(GL_DRAW_BUFFER0), TYPE_6GLenum, -1, BUGLE_GL_ATI_draw_buffers, -1, STATE_GLOBAL, spawn_draw_buffers },
    { STATE_NAME(GL_INDEX_WRITEMASK), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_COLOR_WRITEMASK), TYPE_9GLboolean, 4, BUGLE_GL_EXT_draw_buffers2, -1, STATE_GLOBAL, spawn_color_writemask },
    /* Fallback for if EXT_draw_buffers2 is missing */
    { STATE_NAME(GL_COLOR_WRITEMASK), TYPE_9GLboolean, 4, BUGLE_GL_VERSION_1_1, BUGLE_GL_EXT_draw_buffers2, STATE_GLOBAL },
    { STATE_NAME(GL_DEPTH_WRITEMASK), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_WRITEMASK), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_BACK_WRITEMASK), TYPE_5GLint, -1, BUGLE_GL_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_COLOR_CLEAR_VALUE), TYPE_8GLdouble, 4, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_INDEX_CLEAR_VALUE), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_DEPTH_CLEAR_VALUE), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_CLEAR_VALUE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_ACCUM_CLEAR_VALUE), TYPE_8GLdouble, 4, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_DRAW_FRAMEBUFFER_BINDING), TYPE_5GLint, -1, BUGLE_GL_EXT_framebuffer_blit, -1, STATE_GLOBAL },
    { STATE_NAME(GL_READ_FRAMEBUFFER_BINDING), TYPE_5GLint, -1, BUGLE_GL_EXT_framebuffer_blit, -1, STATE_GLOBAL },
    { STATE_NAME(GL_FRAMEBUFFER_BINDING), TYPE_5GLint, -1, BUGLE_GL_EXT_framebuffer_object, BUGLE_GL_EXT_framebuffer_blit, STATE_GLOBAL },
    { STATE_NAME(GL_RENDERBUFFER_BINDING), TYPE_5GLint, -1, BUGLE_GL_EXT_framebuffer_object, -1, STATE_GLOBAL },
    { STATE_NAME(GL_UNPACK_SWAP_BYTES), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_UNPACK_LSB_FIRST), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_UNPACK_IMAGE_HEIGHT), TYPE_5GLint, -1, BUGLE_GL_EXT_texture3D, -1, STATE_GLOBAL },
    { STATE_NAME(GL_UNPACK_SKIP_IMAGES), TYPE_5GLint, -1, BUGLE_GL_EXT_texture3D, -1, STATE_GLOBAL },
    { STATE_NAME(GL_UNPACK_ROW_LENGTH), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_UNPACK_SKIP_ROWS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_UNPACK_SKIP_PIXELS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_UNPACK_ALIGNMENT), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_UNPACK_COMPRESSED_BLOCK_WIDTH), TYPE_5GLint, -1, BUGLE_GL_ARB_compressed_texture_pixel_storage, -1, STATE_GLOBAL },
    { STATE_NAME(GL_UNPACK_COMPRESSED_BLOCK_HEIGHT), TYPE_5GLint, -1, BUGLE_GL_ARB_compressed_texture_pixel_storage, -1, STATE_GLOBAL },
    { STATE_NAME(GL_UNPACK_COMPRESSED_BLOCK_DEPTH), TYPE_5GLint, -1, BUGLE_GL_ARB_compressed_texture_pixel_storage, -1, STATE_GLOBAL },
    { STATE_NAME(GL_UNPACK_COMPRESSED_BLOCK_SIZE), TYPE_5GLint, -1, BUGLE_GL_ARB_compressed_texture_pixel_storage, -1, STATE_GLOBAL },
    { STATE_NAME(GL_PIXEL_PACK_BUFFER_BINDING), TYPE_5GLint, -1, BUGLE_GL_ARB_pixel_buffer_object, -1, STATE_GLOBAL },
    { STATE_NAME(GL_PACK_SWAP_BYTES), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_PACK_LSB_FIRST), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_PACK_IMAGE_HEIGHT), TYPE_5GLint, -1, BUGLE_GL_EXT_texture3D, -1, STATE_GLOBAL },
    { STATE_NAME(GL_PACK_SKIP_IMAGES), TYPE_5GLint, -1, BUGLE_GL_EXT_texture3D, -1, STATE_GLOBAL },
    { STATE_NAME(GL_PACK_ROW_LENGTH), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_PACK_SKIP_ROWS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_PACK_SKIP_PIXELS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_PACK_ALIGNMENT), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_PACK_COMPRESSED_BLOCK_WIDTH), TYPE_5GLint, -1, BUGLE_GL_ARB_compressed_texture_pixel_storage, -1, STATE_GLOBAL },
    { STATE_NAME(GL_PACK_COMPRESSED_BLOCK_HEIGHT), TYPE_5GLint, -1, BUGLE_GL_ARB_compressed_texture_pixel_storage, -1, STATE_GLOBAL },
    { STATE_NAME(GL_PACK_COMPRESSED_BLOCK_DEPTH), TYPE_5GLint, -1, BUGLE_GL_ARB_compressed_texture_pixel_storage, -1, STATE_GLOBAL },
    { STATE_NAME(GL_PACK_COMPRESSED_BLOCK_SIZE), TYPE_5GLint, -1, BUGLE_GL_ARB_compressed_texture_pixel_storage, -1, STATE_GLOBAL },
    { STATE_NAME(GL_PIXEL_UNPACK_BUFFER_BINDING), TYPE_5GLint, -1, BUGLE_GL_ARB_pixel_buffer_object, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAP_COLOR), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAP_STENCIL), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_INDEX_SHIFT), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_INDEX_OFFSET), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_RED_SCALE), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_GREEN_SCALE), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_BLUE_SCALE), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_ALPHA_SCALE), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_DEPTH_SCALE), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_RED_BIAS), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_GREEN_BIAS), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_BLUE_BIAS), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_ALPHA_BIAS), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_DEPTH_BIAS), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    /* FIXME: glGetColorTable */
    /* FIXME: glGetConvolutionFilter */
    /* FIXME: glGetSeparableFilter (and SEPARABLE_2D in general) */
    { "Color table parameters", 0, NULL_TYPE, -1, BUGLE_GL_ARB_imaging, -1, 0, spawn_color_tables },
    { "Convolution parameters", 0, NULL_TYPE, -1, BUGLE_GL_ARB_imaging, -1, 0, spawn_convolutions },
    { STATE_NAME(GL_POST_CONVOLUTION_RED_SCALE), TYPE_8GLdouble, -1, BUGLE_GL_ARB_imaging, -1, STATE_GLOBAL },
    { STATE_NAME(GL_POST_CONVOLUTION_GREEN_SCALE), TYPE_8GLdouble, -1, BUGLE_GL_ARB_imaging, -1, STATE_GLOBAL },
    { STATE_NAME(GL_POST_CONVOLUTION_BLUE_SCALE), TYPE_8GLdouble, -1, BUGLE_GL_ARB_imaging, -1, STATE_GLOBAL },
    { STATE_NAME(GL_POST_CONVOLUTION_ALPHA_SCALE), TYPE_8GLdouble, -1, BUGLE_GL_ARB_imaging, -1, STATE_GLOBAL },
    { STATE_NAME(GL_POST_CONVOLUTION_RED_BIAS), TYPE_8GLdouble, -1, BUGLE_GL_ARB_imaging, -1, STATE_GLOBAL },
    { STATE_NAME(GL_POST_CONVOLUTION_GREEN_BIAS), TYPE_8GLdouble, -1, BUGLE_GL_ARB_imaging, -1, STATE_GLOBAL },
    { STATE_NAME(GL_POST_CONVOLUTION_BLUE_BIAS), TYPE_8GLdouble, -1, BUGLE_GL_ARB_imaging, -1, STATE_GLOBAL },
    { STATE_NAME(GL_POST_CONVOLUTION_ALPHA_BIAS), TYPE_8GLdouble, -1, BUGLE_GL_ARB_imaging, -1, STATE_GLOBAL },
    { STATE_NAME(GL_POST_COLOR_MATRIX_RED_SCALE), TYPE_8GLdouble, -1, BUGLE_GL_ARB_imaging, -1, STATE_GLOBAL },
    { STATE_NAME(GL_POST_COLOR_MATRIX_GREEN_SCALE), TYPE_8GLdouble, -1, BUGLE_GL_ARB_imaging, -1, STATE_GLOBAL },
    { STATE_NAME(GL_POST_COLOR_MATRIX_BLUE_SCALE), TYPE_8GLdouble, -1, BUGLE_GL_ARB_imaging, -1, STATE_GLOBAL },
    { STATE_NAME(GL_POST_COLOR_MATRIX_ALPHA_SCALE), TYPE_8GLdouble, -1, BUGLE_GL_ARB_imaging, -1, STATE_GLOBAL },
    { STATE_NAME(GL_POST_COLOR_MATRIX_RED_BIAS), TYPE_8GLdouble, -1, BUGLE_GL_ARB_imaging, -1, STATE_GLOBAL },
    { STATE_NAME(GL_POST_COLOR_MATRIX_GREEN_BIAS), TYPE_8GLdouble, -1, BUGLE_GL_ARB_imaging, -1, STATE_GLOBAL },
    { STATE_NAME(GL_POST_COLOR_MATRIX_BLUE_BIAS), TYPE_8GLdouble, -1, BUGLE_GL_ARB_imaging, -1, STATE_GLOBAL },
    { STATE_NAME(GL_POST_COLOR_MATRIX_ALPHA_BIAS), TYPE_8GLdouble, -1, BUGLE_GL_ARB_imaging, -1, STATE_GLOBAL },
    /* FIXME: glGetHistogram */
    /* FIXME: glGetMinmax */
    { "Histogram parameters", 0, NULL_TYPE, -1, BUGLE_GL_ARB_imaging, -1, 0, spawn_histograms },
    { "Minmax parameters", 0, NULL_TYPE, -1, BUGLE_GL_ARB_imaging, -1, 0, spawn_minmax },
    { STATE_NAME(GL_ZOOM_X), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_ZOOM_Y), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    /* FIXME: glGetPixelMap */
    { STATE_NAME(GL_PIXEL_MAP_I_TO_I_SIZE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_PIXEL_MAP_S_TO_S_SIZE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_PIXEL_MAP_I_TO_R_SIZE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_PIXEL_MAP_I_TO_G_SIZE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_PIXEL_MAP_I_TO_B_SIZE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_PIXEL_MAP_I_TO_A_SIZE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_PIXEL_MAP_R_TO_R_SIZE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_PIXEL_MAP_G_TO_G_SIZE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_PIXEL_MAP_B_TO_B_SIZE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_PIXEL_MAP_A_TO_A_SIZE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_READ_BUFFER), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    /* FIXME: glGetMap */
    { STATE_NAME(GL_MAP1_VERTEX_3), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_MAP1_VERTEX_4), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_MAP1_INDEX), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_MAP1_COLOR_4), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_MAP1_NORMAL), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_MAP1_TEXTURE_COORD_1), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_MAP1_TEXTURE_COORD_2), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_MAP1_TEXTURE_COORD_3), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_MAP1_TEXTURE_COORD_4), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_MAP2_VERTEX_3), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_MAP2_VERTEX_4), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_MAP2_INDEX), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_MAP2_COLOR_4), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_MAP2_NORMAL), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_MAP2_TEXTURE_COORD_1), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_MAP2_TEXTURE_COORD_2), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_MAP2_TEXTURE_COORD_3), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_MAP2_TEXTURE_COORD_4), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_MAP1_GRID_DOMAIN), TYPE_8GLdouble, 2, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAP2_GRID_DOMAIN), TYPE_8GLdouble, 4, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAP1_GRID_SEGMENTS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAP2_GRID_SEGMENTS), TYPE_5GLint, 2, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_AUTO_NORMAL), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },

    { "Shader objects", 0, NULL_TYPE, -1, BUGLE_GL_ARB_shader_objects, -1, 0, spawn_shaders },
    { STATE_NAME(GL_CURRENT_PROGRAM), TYPE_5GLint, -1, BUGLE_GL_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_PROGRAM_PIPELINE_BINDING), TYPE_5GLint, -1, BUGLE_GL_ARB_separate_shader_objects, -1, STATE_GLOBAL },
    { "Program objects", 0, NULL_TYPE, -1, BUGLE_GL_ARB_shader_objects, -1, 0, spawn_programs },
    { STATE_NAME(GL_UNIFORM_BUFFER_BINDING), TYPE_5GLint, -1, BUGLE_GL_ARB_uniform_buffer_object, -1, STATE_GLOBAL },
    { STATE_NAME(GL_ATOMIC_COUNTER_BUFFER_BINDING), TYPE_5GLint, -1, BUGLE_GL_ARB_shader_atomic_counters, -1, STATE_GLOBAL },
    /* TODO: per-slot GL_UNIFORM_BUFFER_BINDING, start, size */
    { STATE_NAME(GL_VERTEX_PROGRAM_TWO_SIDE), TYPE_9GLboolean, -1, BUGLE_EXTGROUP_vp_options, -1, STATE_ENABLED },
    { STATE_NAME(GL_PROGRAM_POINT_SIZE), TYPE_9GLboolean, -1, BUGLE_EXTGROUP_vp_options, -1, STATE_ENABLED },
    { STATE_NAME(GL_PERSPECTIVE_CORRECTION_HINT), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_POINT_SMOOTH_HINT), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_LINE_SMOOTH_HINT), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_POLYGON_SMOOTH_HINT), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_FOG_HINT), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_GENERATE_MIPMAP_HINT), TYPE_6GLenum, -1, BUGLE_GL_SGIS_generate_mipmap, -1, STATE_GLOBAL },
    { STATE_NAME(GL_TEXTURE_COMPRESSION_HINT), TYPE_6GLenum, -1, BUGLE_GL_ARB_texture_compression, -1, STATE_GLOBAL },
    { STATE_NAME(GL_FRAGMENT_SHADER_DERIVATIVE_HINT), TYPE_6GLenum, -1, BUGLE_GL_ARB_fragment_shader, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_LIGHTS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_CLIP_DISTANCES), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_COLOR_MATRIX_STACK_DEPTH), TYPE_5GLint, -1, BUGLE_GL_ARB_imaging, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_MODELVIEW_STACK_DEPTH), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_PROJECTION_STACK_DEPTH), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_TEXTURE_STACK_DEPTH), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_SUBPIXEL_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_3D_TEXTURE_SIZE), TYPE_5GLint, -1, BUGLE_GL_EXT_texture3D, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_TEXTURE_SIZE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_ARRAY_TEXTURE_LAYERS), TYPE_5GLint, -1, BUGLE_GL_EXT_texture_array, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_TEXTURE_BUFFER_SIZE), TYPE_5GLint, -1, BUGLE_GL_ARB_texture_buffer_object, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_RECTANGLE_TEXTURE_SIZE), TYPE_5GLint, -1, BUGLE_GL_NV_texture_rectangle, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_TEXTURE_LOD_BIAS), TYPE_8GLdouble, -1, BUGLE_GL_EXT_texture_lod_bias, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_CUBE_MAP_TEXTURE_SIZE), TYPE_5GLint, -1, BUGLE_GL_ARB_texture_cube_map, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_RENDERBUFFER_SIZE), TYPE_5GLint, -1, BUGLE_GL_EXT_framebuffer_object, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_PIXEL_MAP_TABLE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_NAME_STACK_DEPTH), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_LIST_NESTING), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_EVAL_ORDER), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_VIEWPORT_DIMS), TYPE_5GLint, 2, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_VIEWPORTS), TYPE_5GLint, -1, BUGLE_GL_ARB_viewport_array, -1, STATE_GLOBAL },
    { STATE_NAME(GL_VIEWPORT_SUBPIXEL_BITS), TYPE_5GLint, -1, BUGLE_GL_ARB_viewport_array, -1, STATE_GLOBAL },
    { STATE_NAME(GL_VIEWPORT_BOUNDS_RANGE), TYPE_7GLfloat, 2, BUGLE_GL_ARB_viewport_array, -1, STATE_GLOBAL },
    { STATE_NAME(GL_LAYER_PROVOKING_VERTEX), TYPE_6GLenum, -1, BUGLE_GL_ARB_viewport_array, -1, STATE_GLOBAL },
    { STATE_NAME(GL_VIEWPORT_INDEX_PROVOKING_VERTEX), TYPE_6GLenum, -1, BUGLE_GL_ARB_viewport_array, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_ATTRIB_STACK_DEPTH), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_CLIENT_ATTRIB_STACK_DEPTH), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_ALIASED_POINT_SIZE_RANGE), TYPE_8GLdouble, 2, BUGLE_GL_VERSION_1_2, -1, STATE_GLOBAL },
    { STATE_NAME(GL_SMOOTH_POINT_SIZE_RANGE), TYPE_8GLdouble, 2, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_SMOOTH_POINT_SIZE_GRANULARITY), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_ALIASED_LINE_WIDTH_RANGE), TYPE_8GLdouble, 2, BUGLE_GL_VERSION_1_2, -1, STATE_GLOBAL },
    { STATE_NAME(GL_SMOOTH_LINE_WIDTH_RANGE), TYPE_8GLdouble, 2, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_SMOOTH_LINE_WIDTH_GRANULARITY), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_ELEMENTS_INDICES), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_ELEMENTS_VERTICES), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_COMPRESSED_TEXTURE_FORMATS), TYPE_6GLenum, -1, BUGLE_GL_ARB_texture_compression, -1, STATE_COMPRESSED_TEXTURE_FORMATS },
    { STATE_NAME(GL_NUM_COMPRESSED_TEXTURE_FORMATS), TYPE_5GLint, -1, BUGLE_GL_ARB_texture_compression, -1, STATE_GLOBAL },
    /* TODO: GL_PROGRAM_BINARY_FORMATS */
    { STATE_NAME(GL_NUM_PROGRAM_BINARY_FORMATS), TYPE_5GLint, -1, BUGLE_GL_ARB_get_program_binary, -1, STATE_GLOBAL },
    /* TODO: GL_SHADER_BINARY_FORMATS */
    { STATE_NAME(GL_NUM_SHADER_BINARY_FORMATS), TYPE_5GLint, -1, BUGLE_GL_ARB_ES2_compatibility, -1, STATE_GLOBAL },
    { STATE_NAME(GL_SHADER_COMPILER), TYPE_9GLboolean, -1, BUGLE_GL_ARB_ES2_compatibility, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MIN_MAP_BUFFER_ALIGNMENT), TYPE_5GLint, -1, BUGLE_GL_ARB_map_buffer_alignment, -1, STATE_GLOBAL },
    { STATE_NAME(GL_NUM_EXTENSIONS), TYPE_5GLint, -1, GL_VERSION_3_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAJOR_VERSION), TYPE_5GLint, -1, GL_VERSION_3_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MINOR_VERSION), TYPE_5GLint, -1, GL_VERSION_3_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_CONTEXT_FLAGS), TYPE_12contextflags, -1, GL_VERSION_3_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_EXTENSIONS), TYPE_PKc, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL, spawn_extensions },
    { STATE_NAME(GL_RENDERER), TYPE_PKc, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_SHADING_LANGUAGE_VERSION), TYPE_PKc, -1, BUGLE_GL_ARB_shading_language_100, -1, STATE_GLOBAL },
    { STATE_NAME(GL_VENDOR), TYPE_PKc, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_VERSION), TYPE_PKc, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_VERTEX_ATTRIBS), TYPE_5GLint, -1, BUGLE_EXTGROUP_vertex_attrib, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_VERTEX_UNIFORM_COMPONENTS), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_shader, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_VERTEX_UNIFORM_VECTORS), TYPE_5GLint, -1, BUGLE_GL_ARB_ES2_compatibility, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_VERTEX_UNIFORM_BLOCKS), TYPE_5GLint, -1, BUGLE_GL_ARB_uniform_buffer_object, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_VERTEX_OUTPUT_COMPONENTS), TYPE_5GLint, -1, BUGLE_GL_VERSION_3_2, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_shader, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_VERTEX_ATOMIC_COUNTER_BUFFERS), TYPE_5GLint, -1, BUGLE_GL_ARB_shader_atomic_counters, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_VERTEX_ATOMIC_COUNTERS), TYPE_5GLint, -1, BUGLE_GL_ARB_shader_atomic_counters, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_TESS_GEN_LEVEL), TYPE_5GLint, -1, BUGLE_GL_ARB_tessellation_shader, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_PATCH_VERTICES), TYPE_5GLint, -1, BUGLE_GL_ARB_tessellation_shader, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_TESS_CONTROL_UNIFORM_COMPONENTS), TYPE_5GLint, -1, BUGLE_GL_ARB_tessellation_shader, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_TESS_EVALUATION_UNIFORM_COMPONENTS), TYPE_5GLint, -1, BUGLE_GL_ARB_tessellation_shader, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_TESS_CONTROL_TEXTURE_IMAGE_UNITS), TYPE_5GLint, -1, BUGLE_GL_ARB_tessellation_shader, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_TESS_EVALUATION_TEXTURE_IMAGE_UNITS), TYPE_5GLint, -1, BUGLE_GL_ARB_tessellation_shader, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_TESS_CONTROL_OUTPUT_COMPONENTS), TYPE_5GLint, -1, BUGLE_GL_ARB_tessellation_shader, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_TESS_PATCH_COMPONENTS), TYPE_5GLint, -1, BUGLE_GL_ARB_tessellation_shader, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_TESS_CONTROL_TOTAL_OUTPUT_COMPONENTS), TYPE_5GLint, -1, BUGLE_GL_ARB_tessellation_shader, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_TESS_EVALUATION_OUTPUT_COMPONENTS), TYPE_5GLint, -1, BUGLE_GL_ARB_tessellation_shader, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_TESS_CONTROL_UNIFORM_BLOCKS), TYPE_5GLint, -1, BUGLE_GL_ARB_tessellation_shader, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_TESS_EVALUATION_UNIFORM_BLOCKS), TYPE_5GLint, -1, BUGLE_GL_ARB_tessellation_shader, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_TESS_CONTROL_ATOMIC_COUNTER_BUFFERS), TYPE_5GLint, -1, BUGLE_GL_ARB_shader_atomic_counters, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_TESS_EVALUATION_ATOMIC_COUNTER_BUFFERS), TYPE_5GLint, -1, BUGLE_GL_ARB_shader_atomic_counters, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_TESS_CONTROL_ATOMIC_COUNTERS), TYPE_5GLint, -1, BUGLE_GL_ARB_shader_atomic_counters, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_TESS_EVALUATION_ATOMIC_COUNTERS), TYPE_5GLint, -1, BUGLE_GL_ARB_shader_atomic_counters, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_GEOMETRY_UNIFORM_COMPONENTS), TYPE_5GLint, -1, BUGLE_GL_ARB_geometry_shader4, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_GEOMETRY_UNIFORM_BLOCKS), TYPE_5GLint, -1, BUGLE_GL_ARB_geometry_shader4, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_GEOMETRY_INPUT_COMPONENTS), TYPE_5GLint, -1, BUGLE_GL_ARB_geometry_shader4, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_GEOMETRY_OUTPUT_COMPONENTS), TYPE_5GLint, -1, BUGLE_GL_ARB_geometry_shader4, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_GEOMETRY_OUTPUT_VERTICES), TYPE_5GLint, -1, BUGLE_GL_ARB_geometry_shader4, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS), TYPE_5GLint, -1, BUGLE_GL_ARB_geometry_shader4, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS), TYPE_5GLint, -1, BUGLE_GL_ARB_geometry_shader4, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_GEOMETRY_SHADER_INVOCATIONS), TYPE_5GLint, -1, BUGLE_GL_ARB_gpu_shader5, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_GEOMETRY_ATOMIC_COUNTER_BUFFERS), TYPE_5GLint, -1, BUGLE_GL_ARB_shader_atomic_counters, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_GEOMETRY_ATOMIC_COUNTERS), TYPE_5GLint, -1, BUGLE_GL_ARB_shader_atomic_counters, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_VERTEX_STREAMS), TYPE_5GLint, -1, BUGLE_GL_ARB_transform_feedback3, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS), TYPE_5GLint, -1, BUGLE_GL_ARB_fragment_shader, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_FRAGMENT_UNIFORM_VECTORS), TYPE_5GLint, -1, BUGLE_GL_ARB_ES2_compatibility, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_FRAGMENT_UNIFORM_BLOCKS), TYPE_5GLint, -1, BUGLE_GL_ARB_uniform_buffer_object, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_FRAGMENT_INPUT_COMPONENTS), TYPE_5GLint, -1, BUGLE_GL_VERSION_3_2, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_FRAGMENT_ATOMIC_COUNTER_BUFFERS), TYPE_5GLint, -1, BUGLE_GL_ARB_shader_atomic_counters, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_FRAGMENT_ATOMIC_COUNTERS), TYPE_5GLint, -1, BUGLE_GL_ARB_shader_atomic_counters, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_TEXTURE_IMAGE_UNITS), TYPE_5GLint, -1, BUGLE_EXTGROUP_texunits, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MIN_PROGRAM_TEXTURE_GATHER_OFFSET), TYPE_5GLint, -1, BUGLE_GL_ARB_texture_gather, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_PROGRAM_TEXTURE_GATHER_OFFSET), TYPE_5GLint, -1, BUGLE_GL_ARB_texture_gather, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_TEXTURE_UNITS), TYPE_5GLint, -1, BUGLE_GL_ARB_multitexture, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_shader, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_TEXTURE_COORDS), TYPE_5GLint, -1, BUGLE_EXTGROUP_texunits, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MIN_PROGRAM_TEXEL_OFFSET), TYPE_5GLint, -1, BUGLE_GL_VERSION_3_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_PROGRAM_TEXEL_OFFSET), TYPE_5GLint, -1, BUGLE_GL_VERSION_3_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_UNIFORM_BUFFER_BINDINGS), TYPE_5GLint, -1, BUGLE_GL_ARB_uniform_buffer_object, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_UNIFORM_BLOCK_SIZE), TYPE_5GLint, -1, BUGLE_GL_ARB_uniform_buffer_object, -1, STATE_GLOBAL },
    { STATE_NAME(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT), TYPE_5GLint, -1, BUGLE_GL_ARB_uniform_buffer_object, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_COMBINED_UNIFORM_BLOCKS), TYPE_5GLint, -1, BUGLE_GL_ARB_uniform_buffer_object, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_VARYING_COMPONENTS), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_shader, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_VARYING_VECTORS), TYPE_5GLint, -1, BUGLE_GL_ARB_ES2_compatibility, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_SUBROUTINES), TYPE_5GLint, -1, BUGLE_GL_ARB_shader_subroutine, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_SUBROUTINE_UNIFORM_LOCATIONS), TYPE_5GLint, -1, BUGLE_GL_ARB_shader_subroutine, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS), TYPE_5GLint, -1, BUGLE_GL_ARB_shader_atomic_counters, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_ATOMIC_COUNTER_BUFFER_SIZE), TYPE_5GLint, -1, BUGLE_GL_ARB_shader_atomic_counters, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_COMBINED_ATOMIC_COUNTER_BUFFERS), TYPE_5GLint, -1, BUGLE_GL_ARB_shader_atomic_counters, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_COMBINED_ATOMIC_COUNTERS), TYPE_5GLint, -1, BUGLE_GL_ARB_shader_atomic_counters, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_IMAGE_UNITS), TYPE_5GLint, -1, BUGLE_GL_ARB_shader_image_load_store, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_COMBINED_IMAGE_UNITS_AND_FRAGMENT_OUTPUTS), TYPE_5GLint, -1, BUGLE_GL_ARB_shader_image_load_store, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_IMAGE_SAMPLES), TYPE_5GLint, -1, BUGLE_GL_ARB_shader_image_load_store, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_VERTEX_IMAGE_UNIFORMS), TYPE_5GLint, -1, BUGLE_GL_ARB_shader_image_load_store, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_TESS_CONTROL_IMAGE_UNIFORMS), TYPE_5GLint, -1, BUGLE_GL_ARB_shader_image_load_store, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_TESS_EVALUATION_IMAGE_UNIFORMS), TYPE_5GLint, -1, BUGLE_GL_ARB_shader_image_load_store, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_GEOMETRY_IMAGE_UNIFORMS), TYPE_5GLint, -1, BUGLE_GL_ARB_shader_image_load_store, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_FRAGMENT_IMAGE_UNIFORMS), TYPE_5GLint, -1, BUGLE_GL_ARB_shader_image_load_store, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_COMBINED_IMAGE_UNIFORMS), TYPE_5GLint, -1, BUGLE_GL_ARB_shader_image_load_store, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS), TYPE_5GLint, -1, BUGLE_GL_ARB_uniform_buffer_object, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_COMBINED_GEOMETRY_UNIFORM_COMPONENTS), TYPE_5GLint, -1, BUGLE_GL_ARB_geometry_shader4, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_COMBINED_TESS_CONTROL_UNIFORM_COMPONENTS), TYPE_5GLint, -1, BUGLE_GL_ARB_tessellation_shader, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_COMBINED_TESS_EVALUATION_UNIFORM_COMPONENTS), TYPE_5GLint, -1, BUGLE_GL_ARB_tessellation_shader, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS), TYPE_5GLint, -1, BUGLE_GL_ARB_uniform_buffer_object, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_SAMPLE_MASK_WORDS), TYPE_5GLint, -1, BUGLE_GL_ARB_texture_multisample, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_SAMPLES), TYPE_5GLint, -1, BUGLE_GL_ARB_texture_multisample, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_COLOR_TEXTURE_SAMPLES), TYPE_5GLint, -1, BUGLE_GL_ARB_texture_multisample, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_DEPTH_TEXTURE_SAMPLES), TYPE_5GLint, -1, BUGLE_GL_ARB_texture_multisample, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_INTEGER_SAMPLES), TYPE_5GLint, -1, BUGLE_GL_ARB_texture_multisample, -1, STATE_GLOBAL },
    { STATE_NAME(GL_QUADS_FOLLOW_PROVOKING_VERTEX_CONVENTION), TYPE_9GLboolean, -1, BUGLE_GL_ARB_provoking_vertex, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_SERVER_WAIT_TIMEOUT), TYPE_7GLint64, -1, BUGLE_GL_ARB_sync, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MIN_FRAGMENT_INTERPOLATION_OFFSET), TYPE_7GLfloat, -1, BUGLE_GL_ARB_gpu_shader5, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_FRAGMENT_INTERPOLATION_OFFSET), TYPE_7GLfloat, -1, BUGLE_GL_ARB_gpu_shader5, -1, STATE_GLOBAL },
    { STATE_NAME(GL_FRAGMENT_INTERPOLATION_OFFSET_BITS), TYPE_5GLint, -1, BUGLE_GL_ARB_gpu_shader5, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS), TYPE_5GLint, -1, BUGLE_GL_EXT_transform_feedback, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS), TYPE_5GLint, -1, BUGLE_GL_EXT_transform_feedback, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS), TYPE_5GLint, -1, BUGLE_GL_EXT_transform_feedback, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_TRANSFORM_FEEDBACK_BUFFERS), TYPE_5GLint, -1, BUGLE_GL_ARB_transform_feedback3, -1, STATE_GLOBAL },

    { STATE_NAME(GL_AUX_BUFFERS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_DRAW_BUFFERS), TYPE_5GLint, -1, BUGLE_GL_ATI_draw_buffers, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_DUAL_SOURCE_DRAW_BUFFERS), TYPE_5GLint, -1, BUGLE_GL_ARB_blend_func_extended, -1, STATE_GLOBAL },
    { STATE_NAME(GL_RGBA_MODE), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_INDEX_MODE), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_DOUBLEBUFFER), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STEREO), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_SAMPLE_BUFFERS), TYPE_5GLint, -1, BUGLE_GL_ARB_multisample, -1, STATE_GLOBAL },
    { STATE_NAME(GL_SAMPLES), TYPE_5GLint, -1, BUGLE_GL_ARB_multisample, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_COLOR_ATTACHMENTS), TYPE_5GLint, -1, BUGLE_GL_EXT_framebuffer_object, -1, STATE_GLOBAL },
    { STATE_NAME(GL_RED_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_GREEN_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_BLUE_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_ALPHA_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_INDEX_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_DEPTH_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_ACCUM_RED_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_ACCUM_GREEN_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_ACCUM_BLUE_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_ACCUM_ALPHA_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_IMPLEMENTATION_COLOR_READ_TYPE), TYPE_5GLint, -1, BUGLE_GL_ARB_ES2_compatibility, -1, STATE_GLOBAL },
    { STATE_NAME(GL_IMPLEMENTATION_COLOR_READ_FORMAT), TYPE_5GLint, -1, BUGLE_GL_ARB_ES2_compatibility, -1, STATE_GLOBAL },
    { STATE_NAME(GL_LIST_BASE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_LIST_INDEX), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_LIST_MODE), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_ATTRIB_STACK_DEPTH), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_CLIENT_ATTRIB_STACK_DEPTH), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_NAME_STACK_DEPTH), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_RENDER_MODE), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_SELECTION_BUFFER_POINTER), TYPE_P6GLvoid, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_SELECTION_BUFFER_SIZE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_FEEDBACK_BUFFER_POINTER), TYPE_P6GLvoid, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_FEEDBACK_BUFFER_SIZE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_FEEDBACK_BUFFER_TYPE), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_COPY_READ_BUFFER), TYPE_5GLint, -1, BUGLE_GL_ARB_copy_buffer, -1, STATE_GLOBAL },
    { STATE_NAME(GL_COPY_WRITE_BUFFER), TYPE_5GLint, -1, BUGLE_GL_ARB_copy_buffer, -1, STATE_GLOBAL },
    { STATE_NAME(GL_TEXTURE_CUBE_MAP_SEAMLESS), TYPE_9GLboolean, -1, BUGLE_GL_ARB_seamless_cube_map, -1, STATE_ENABLED },
    /* FIXME: glGetError */

    /* The remaining state is pure extensions */
#if defined(GL_ARB_vertex_program) || defined(GL_ARB_fragment_program)
    { STATE_NAME(GL_PROGRAM_ERROR_POSITION_ARB), TYPE_5GLint, -1, BUGLE_EXTGROUP_old_program, -1, STATE_GLOBAL },
    { STATE_NAME(GL_PROGRAM_ERROR_STRING_ARB), TYPE_PKc, -1, BUGLE_EXTGROUP_old_program, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_PROGRAM_MATRICES_ARB), TYPE_5GLint, -1, BUGLE_EXTGROUP_old_program, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_PROGRAM_MATRIX_STACK_DEPTH_ARB), TYPE_5GLint, -1, BUGLE_EXTGROUP_old_program, -1, STATE_GLOBAL },
#endif
#ifdef GL_EXT_texture_filter_anisotropic
    { STATE_NAME(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT), TYPE_8GLdouble, -1, BUGLE_GL_EXT_texture_filter_anisotropic, -1, STATE_GLOBAL },
#endif
#ifdef GL_NV_light_max_exponent
    { STATE_NAME(GL_MAX_SHININESS_NV), TYPE_5GLint, -1, BUGLE_GL_NV_light_max_exponent, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_SPOT_EXPONENT_NV), TYPE_5GLint, -1, BUGLE_GL_NV_light_max_exponent, -1, STATE_GLOBAL },
#endif
#ifdef GL_NV_multisample_filter_hint
    { STATE_NAME(GL_MULTISAMPLE_FILTER_HINT_NV), TYPE_6GLenum, -1, BUGLE_GL_NV_multisample_filter_hint, -1, STATE_GLOBAL },
#endif
#ifdef GL_IBM_rasterpos_clip
    { STATE_NAME(GL_RASTER_POSITION_UNCLIPPED_IBM), TYPE_9GLboolean, -1, BUGLE_GL_IBM_rasterpos_clip, -1, STATE_ENABLED },
#endif
#ifdef GL_EXT_depth_bounds_test
    { STATE_NAME(GL_DEPTH_BOUNDS_TEST_EXT), TYPE_9GLboolean, -1, BUGLE_GL_EXT_depth_bounds_test, -1, STATE_ENABLED },
    { STATE_NAME(GL_DEPTH_BOUNDS_EXT), TYPE_8GLdouble, 2, BUGLE_GL_EXT_depth_bounds_test, -1, STATE_GLOBAL },
#endif
    /* G80 extensions */
#ifdef GL_EXT_bindable_uniform
    { STATE_NAME(GL_MAX_VERTEX_BINDABLE_UNIFORMS_EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_bindable_uniform, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_GEOMETRY_BINDABLE_UNIFORMS_EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_bindable_uniform, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_FRAGMENT_BINDABLE_UNIFORMS_EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_bindable_uniform, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_BINDABLE_UNIFORM_SIZE_EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_bindable_uniform, -1, STATE_GLOBAL },
    { STATE_NAME(GL_UNIFORM_BUFFER_BINDING_EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_bindable_uniform, -1, STATE_GLOBAL },
#endif
#ifdef GL_EXT_packed_float
    { STATE_NAME(GL_RGBA_SIGNED_COMPONENTS_EXT), TYPE_9GLboolean, 4, BUGLE_GL_EXT_packed_float, -1, STATE_GLOBAL },
#endif
#ifdef GL_NV_transform_feedback
    { STATE_NAME(GL_TRANSFORM_FEEDBACK_ATTRIBS_NV), TYPE_5GLint, -1, BUGLE_GL_NV_transform_feedback, -1, STATE_GLOBAL },
    { STATE_NAME(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING_NV), TYPE_5GLint, -1, BUGLE_GL_NV_transform_feedback, -1, STATE_GLOBAL },
#endif /* GL_NV_transform_feedback */
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info old_program_object_state[] =
{
#if defined(GL_ARB_vertex_program) || defined(GL_ARB_fragment_program)
    { STATE_NAME(GL_PROGRAM_LENGTH_ARB), TYPE_5GLint, -1, BUGLE_EXTGROUP_old_program, -1, STATE_OLD_PROGRAM },
    { STATE_NAME(GL_PROGRAM_FORMAT_ARB), TYPE_6GLenum, -1, BUGLE_EXTGROUP_old_program, -1, STATE_OLD_PROGRAM },
    { STATE_NAME(GL_PROGRAM_STRING_ARB), TYPE_PKc, -1, BUGLE_EXTGROUP_old_program, -1, STATE_OLD_PROGRAM },
    { STATE_NAME(GL_PROGRAM_INSTRUCTIONS_ARB), TYPE_5GLint, -1, BUGLE_EXTGROUP_old_program, -1, STATE_OLD_PROGRAM },
#ifdef GL_ARB_fragment_program
    { STATE_NAME(GL_PROGRAM_ALU_INSTRUCTIONS_ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_fragment_program, -1, STATE_OLD_PROGRAM | STATE_SELECT_FRAGMENT },
    { STATE_NAME(GL_PROGRAM_TEX_INSTRUCTIONS_ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_fragment_program, -1, STATE_OLD_PROGRAM | STATE_SELECT_FRAGMENT },
    { STATE_NAME(GL_PROGRAM_TEX_INDIRECTIONS_ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_fragment_program, -1, STATE_OLD_PROGRAM | STATE_SELECT_FRAGMENT },
#endif
    { STATE_NAME(GL_PROGRAM_TEMPORARIES_ARB), TYPE_5GLint, -1, BUGLE_EXTGROUP_old_program, -1, STATE_OLD_PROGRAM },
    { STATE_NAME(GL_PROGRAM_PARAMETERS_ARB), TYPE_5GLint, -1, BUGLE_EXTGROUP_old_program, -1, STATE_OLD_PROGRAM },
    { STATE_NAME(GL_PROGRAM_ATTRIBS_ARB), TYPE_5GLint, -1, BUGLE_EXTGROUP_old_program, -1, STATE_OLD_PROGRAM },
#ifdef GL_ARB_vertex_program
    { STATE_NAME(GL_PROGRAM_ADDRESS_REGISTERS_ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_program, -1, STATE_OLD_PROGRAM | STATE_SELECT_VERTEX },
#endif
    { STATE_NAME(GL_PROGRAM_NATIVE_INSTRUCTIONS_ARB), TYPE_5GLint, -1, BUGLE_EXTGROUP_old_program, -1, STATE_OLD_PROGRAM },
#ifdef GL_ARB_fragment_program
    { STATE_NAME(GL_PROGRAM_NATIVE_ALU_INSTRUCTIONS_ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_fragment_program, -1, STATE_OLD_PROGRAM | STATE_SELECT_FRAGMENT },
    { STATE_NAME(GL_PROGRAM_NATIVE_TEX_INSTRUCTIONS_ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_fragment_program, -1, STATE_OLD_PROGRAM | STATE_SELECT_FRAGMENT },
    { STATE_NAME(GL_PROGRAM_NATIVE_TEX_INDIRECTIONS_ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_fragment_program, -1, STATE_OLD_PROGRAM | STATE_SELECT_FRAGMENT },
#endif
    { STATE_NAME(GL_PROGRAM_NATIVE_TEMPORARIES_ARB), TYPE_5GLint, -1, BUGLE_EXTGROUP_old_program, -1, STATE_OLD_PROGRAM },
    { STATE_NAME(GL_PROGRAM_NATIVE_PARAMETERS_ARB), TYPE_5GLint, -1, BUGLE_EXTGROUP_old_program, -1, STATE_OLD_PROGRAM },
    { STATE_NAME(GL_PROGRAM_NATIVE_ATTRIBS_ARB), TYPE_5GLint, -1, BUGLE_EXTGROUP_old_program, -1, STATE_OLD_PROGRAM },
#ifdef GL_ARB_vertex_program
    { STATE_NAME(GL_PROGRAM_NATIVE_ADDRESS_REGISTERS_ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_program, -1, STATE_OLD_PROGRAM | STATE_SELECT_VERTEX },
#endif
    { STATE_NAME(GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB), TYPE_9GLboolean, -1, BUGLE_EXTGROUP_old_program, -1, STATE_OLD_PROGRAM },
#endif /* GL_ARB_vertex_program || GL_ARB_fragment_program */
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info old_program_state[] =
{
#if defined(GL_ARB_vertex_program) || defined(GL_ARB_fragment_program)
    { STATE_NAME(GL_PROGRAM_BINDING_ARB), TYPE_5GLint, -1, BUGLE_EXTGROUP_old_program, -1, STATE_MODE_OLD_PROGRAM },
    { STATE_NAME(GL_MAX_PROGRAM_ENV_PARAMETERS_ARB), TYPE_5GLint, -1, BUGLE_EXTGROUP_old_program, -1, STATE_OLD_PROGRAM },
    { STATE_NAME(GL_MAX_PROGRAM_LOCAL_PARAMETERS_ARB), TYPE_5GLint, -1, BUGLE_EXTGROUP_old_program, -1, STATE_OLD_PROGRAM },
    { STATE_NAME(GL_MAX_PROGRAM_INSTRUCTIONS_ARB), TYPE_5GLint, -1, BUGLE_EXTGROUP_old_program, -1, STATE_OLD_PROGRAM },
#ifdef GL_ARB_fragment_program
    { STATE_NAME(GL_MAX_PROGRAM_ALU_INSTRUCTIONS_ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_fragment_program, -1, STATE_OLD_PROGRAM | STATE_SELECT_FRAGMENT },
    { STATE_NAME(GL_MAX_PROGRAM_TEX_INSTRUCTIONS_ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_fragment_program, -1, STATE_OLD_PROGRAM | STATE_SELECT_FRAGMENT },
    { STATE_NAME(GL_MAX_PROGRAM_TEX_INDIRECTIONS_ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_fragment_program, -1, STATE_OLD_PROGRAM | STATE_SELECT_FRAGMENT },
#endif
    { STATE_NAME(GL_MAX_PROGRAM_TEMPORARIES_ARB), TYPE_5GLint, -1, BUGLE_EXTGROUP_old_program, -1, STATE_OLD_PROGRAM },
    { STATE_NAME(GL_MAX_PROGRAM_PARAMETERS_ARB), TYPE_5GLint, -1, BUGLE_EXTGROUP_old_program, -1, STATE_OLD_PROGRAM },
    { STATE_NAME(GL_MAX_PROGRAM_ATTRIBS_ARB), TYPE_5GLint, -1, BUGLE_EXTGROUP_old_program, -1, STATE_OLD_PROGRAM },
#ifdef GL_ARB_vertex_program
    { STATE_NAME(GL_MAX_PROGRAM_ADDRESS_REGISTERS_ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_program, -1, STATE_OLD_PROGRAM | STATE_SELECT_VERTEX },
#endif
    { STATE_NAME(GL_MAX_PROGRAM_NATIVE_INSTRUCTIONS_ARB), TYPE_5GLint, -1, BUGLE_EXTGROUP_old_program, -1, STATE_OLD_PROGRAM },
#ifdef GL_ARB_fragment_program
    { STATE_NAME(GL_MAX_PROGRAM_NATIVE_ALU_INSTRUCTIONS_ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_fragment_program, -1, STATE_OLD_PROGRAM | STATE_SELECT_FRAGMENT },
    { STATE_NAME(GL_MAX_PROGRAM_NATIVE_TEX_INSTRUCTIONS_ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_fragment_program, -1, STATE_OLD_PROGRAM | STATE_SELECT_FRAGMENT },
    { STATE_NAME(GL_MAX_PROGRAM_NATIVE_TEX_INDIRECTIONS_ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_fragment_program, -1, STATE_OLD_PROGRAM | STATE_SELECT_FRAGMENT },
#endif
    { STATE_NAME(GL_MAX_PROGRAM_NATIVE_TEMPORARIES_ARB), TYPE_5GLint, -1, BUGLE_EXTGROUP_old_program, -1, STATE_OLD_PROGRAM },
    { STATE_NAME(GL_MAX_PROGRAM_NATIVE_PARAMETERS_ARB), TYPE_5GLint, -1, BUGLE_EXTGROUP_old_program, -1, STATE_OLD_PROGRAM },
    { STATE_NAME(GL_MAX_PROGRAM_NATIVE_ATTRIBS_ARB), TYPE_5GLint, -1, BUGLE_EXTGROUP_old_program, -1, STATE_OLD_PROGRAM },
#ifdef GL_ARB_vertex_program
    { STATE_NAME(GL_MAX_PROGRAM_NATIVE_ADDRESS_REGISTERS_ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_program, -1, STATE_OLD_PROGRAM | STATE_SELECT_VERTEX },
#endif
#endif /* GL_ARB_vertex_program || GL_ARB_fragment_program */
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info query_state[] =
{
    { STATE_NAME(GL_CURRENT_QUERY), TYPE_5GLint, -1, BUGLE_GL_ARB_occlusion_query, -1, STATE_QUERY },
    { STATE_NAME(GL_QUERY_COUNTER_BITS), TYPE_5GLint, -1, BUGLE_GL_ARB_occlusion_query, -1, STATE_QUERY },
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info query_object_state[] =
{
    { STATE_NAME(GL_QUERY_RESULT), TYPE_6GLuint, -1, BUGLE_GL_ARB_occlusion_query, -1, STATE_QUERY_OBJECT },
    { STATE_NAME(GL_QUERY_RESULT_AVAILABLE), TYPE_9GLboolean, -1, BUGLE_GL_ARB_occlusion_query, -1, STATE_QUERY_OBJECT },
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info buffer_parameter_state[] =
{
    /* Note: the access flags, offset and length are new in GL 3.0, and were
     * not backported to ARB_map_buffer_range even though it sets the state.
     */
    { STATE_NAME(GL_BUFFER_SIZE), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_buffer_object, -1, STATE_BUFFER_PARAMETER },
    { STATE_NAME(GL_BUFFER_USAGE), TYPE_6GLenum, -1, BUGLE_GL_ARB_vertex_buffer_object, -1, STATE_BUFFER_PARAMETER },
    { STATE_NAME(GL_BUFFER_ACCESS), TYPE_6GLenum, -1, BUGLE_GL_ARB_vertex_buffer_object, -1, STATE_BUFFER_PARAMETER },
    { STATE_NAME(GL_BUFFER_ACCESS_FLAGS), TYPE_20mapbufferrangeaccess, -1, BUGLE_GL_VERSION_3_0, -1, STATE_BUFFER_PARAMETER },
    { STATE_NAME(GL_BUFFER_MAPPED), TYPE_9GLboolean, -1, BUGLE_GL_ARB_vertex_buffer_object, -1, STATE_BUFFER_PARAMETER },
    { STATE_NAME(GL_BUFFER_MAP_POINTER), TYPE_P6GLvoid, -1, BUGLE_GL_ARB_vertex_buffer_object, -1, STATE_BUFFER_PARAMETER },
    { STATE_NAME(GL_BUFFER_MAP_OFFSET), TYPE_5GLint, -1, BUGLE_GL_VERSION_3_0, -1, STATE_BUFFER_PARAMETER },
    { STATE_NAME(GL_BUFFER_MAP_LENGTH), TYPE_5GLint, -1, BUGLE_GL_VERSION_3_0, -1, STATE_BUFFER_PARAMETER },
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info vertex_array_parameter_state[] =
{
    { STATE_NAME(GL_VERTEX_ARRAY), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_VERTEX_ARRAY_ENABLED },
    { STATE_NAME(GL_VERTEX_ARRAY_SIZE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_VERTEX_ARRAY },
    { STATE_NAME(GL_VERTEX_ARRAY_TYPE), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_VERTEX_ARRAY },
    { STATE_NAME(GL_VERTEX_ARRAY_STRIDE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_VERTEX_ARRAY },
    { STATE_NAME(GL_VERTEX_ARRAY_POINTER), TYPE_P6GLvoid, -1, BUGLE_GL_VERSION_1_1, -1, STATE_VERTEX_ARRAY },
    { STATE_NAME(GL_NORMAL_ARRAY), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_VERTEX_ARRAY_ENABLED },
    { STATE_NAME(GL_NORMAL_ARRAY_TYPE), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_VERTEX_ARRAY },
    { STATE_NAME(GL_NORMAL_ARRAY_STRIDE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_VERTEX_ARRAY },
    { STATE_NAME(GL_NORMAL_ARRAY_POINTER), TYPE_P6GLvoid, -1, BUGLE_GL_VERSION_1_1, -1, STATE_VERTEX_ARRAY },
    { STATE_NAME(GL_FOG_COORD_ARRAY), TYPE_9GLboolean, -1, BUGLE_GL_EXT_fog_coord, -1, STATE_VERTEX_ARRAY_ENABLED },
    { STATE_NAME(GL_FOG_COORD_ARRAY_TYPE), TYPE_6GLenum, -1, BUGLE_GL_EXT_fog_coord, -1, STATE_VERTEX_ARRAY },
    { STATE_NAME(GL_FOG_COORD_ARRAY_STRIDE), TYPE_5GLint, -1, BUGLE_GL_EXT_fog_coord, -1, STATE_VERTEX_ARRAY },
    { STATE_NAME(GL_FOG_COORD_ARRAY_POINTER), TYPE_P6GLvoid, -1, BUGLE_GL_EXT_fog_coord, -1, STATE_VERTEX_ARRAY },
    { STATE_NAME(GL_COLOR_ARRAY), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_VERTEX_ARRAY_ENABLED },
    { STATE_NAME(GL_COLOR_ARRAY_SIZE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_VERTEX_ARRAY },
    { STATE_NAME(GL_COLOR_ARRAY_TYPE), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_VERTEX_ARRAY },
    { STATE_NAME(GL_COLOR_ARRAY_STRIDE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_VERTEX_ARRAY },
    { STATE_NAME(GL_COLOR_ARRAY_POINTER), TYPE_P6GLvoid, -1, BUGLE_GL_VERSION_1_1, -1, STATE_VERTEX_ARRAY },
    { STATE_NAME(GL_SECONDARY_COLOR_ARRAY), TYPE_9GLboolean, -1, BUGLE_GL_EXT_secondary_color, -1, STATE_VERTEX_ARRAY_ENABLED },
    { STATE_NAME(GL_SECONDARY_COLOR_ARRAY_SIZE), TYPE_5GLint, -1, BUGLE_GL_EXT_secondary_color, -1, STATE_VERTEX_ARRAY },
    { STATE_NAME(GL_SECONDARY_COLOR_ARRAY_TYPE), TYPE_6GLenum, -1, BUGLE_GL_EXT_secondary_color, -1, STATE_VERTEX_ARRAY },
    { STATE_NAME(GL_SECONDARY_COLOR_ARRAY_STRIDE), TYPE_5GLint, -1, BUGLE_GL_EXT_secondary_color, -1, STATE_VERTEX_ARRAY },
    { STATE_NAME(GL_SECONDARY_COLOR_ARRAY_POINTER), TYPE_P6GLvoid, -1, BUGLE_GL_EXT_secondary_color, -1, STATE_VERTEX_ARRAY },
    { STATE_NAME(GL_INDEX_ARRAY), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_VERTEX_ARRAY_ENABLED },
    { STATE_NAME(GL_INDEX_ARRAY_TYPE), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_VERTEX_ARRAY },
    { STATE_NAME(GL_INDEX_ARRAY_STRIDE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_VERTEX_ARRAY },
    { STATE_NAME(GL_INDEX_ARRAY_POINTER), TYPE_P6GLvoid, -1, BUGLE_GL_VERSION_1_1, -1, STATE_VERTEX_ARRAY },
    { STATE_NAME(GL_EDGE_FLAG_ARRAY), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_VERTEX_ARRAY_ENABLED },
    { STATE_NAME(GL_EDGE_FLAG_ARRAY_STRIDE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_VERTEX_ARRAY },
    { STATE_NAME(GL_EDGE_FLAG_ARRAY_POINTER), TYPE_P6GLvoid, -1, BUGLE_GL_VERSION_1_1, -1, STATE_VERTEX_ARRAY },
    { "Attribute Arrays", GL_NONE, NULL_TYPE, -1, BUGLE_EXTGROUP_vertex_attrib, -1, 0, spawn_vertex_array_attrib_arrays },
    { STATE_NAME(GL_VERTEX_ARRAY_BUFFER_BINDING), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_buffer_object, -1, STATE_VERTEX_ARRAY },
    { STATE_NAME(GL_NORMAL_ARRAY_BUFFER_BINDING), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_buffer_object, -1, STATE_VERTEX_ARRAY },
    { STATE_NAME(GL_COLOR_ARRAY_BUFFER_BINDING), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_buffer_object, -1, STATE_VERTEX_ARRAY },
    { STATE_NAME(GL_INDEX_ARRAY_BUFFER_BINDING), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_buffer_object, -1, STATE_VERTEX_ARRAY },
    { STATE_NAME(GL_EDGE_FLAG_ARRAY_BUFFER_BINDING), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_buffer_object, -1, STATE_VERTEX_ARRAY },
    { STATE_NAME(GL_SECONDARY_COLOR_ARRAY_BUFFER_BINDING), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_buffer_object, -1, STATE_VERTEX_ARRAY },
    { STATE_NAME(GL_FOG_COORDINATE_ARRAY_BUFFER_BINDING), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_buffer_object, -1, STATE_VERTEX_ARRAY },
    { STATE_NAME(GL_ELEMENT_ARRAY_BUFFER_BINDING), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_buffer_object, -1, STATE_VERTEX_ARRAY },
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info renderbuffer_parameter_state[] =
{
    { STATE_NAME(GL_RENDERBUFFER_WIDTH), TYPE_5GLint, -1, BUGLE_GL_EXT_framebuffer_object, -1, STATE_RENDERBUFFER_PARAMETER },
    { STATE_NAME(GL_RENDERBUFFER_HEIGHT), TYPE_5GLint, -1, BUGLE_GL_EXT_framebuffer_object, -1, STATE_RENDERBUFFER_PARAMETER },
    { STATE_NAME(GL_RENDERBUFFER_INTERNAL_FORMAT), TYPE_6GLenum, -1, BUGLE_GL_EXT_framebuffer_object, -1, STATE_RENDERBUFFER_PARAMETER },
    { STATE_NAME(GL_RENDERBUFFER_RED_SIZE), TYPE_5GLint, -1, BUGLE_GL_EXT_framebuffer_object, -1, STATE_RENDERBUFFER_PARAMETER },
    { STATE_NAME(GL_RENDERBUFFER_GREEN_SIZE), TYPE_5GLint, -1, BUGLE_GL_EXT_framebuffer_object, -1, STATE_RENDERBUFFER_PARAMETER },
    { STATE_NAME(GL_RENDERBUFFER_BLUE_SIZE), TYPE_5GLint, -1, BUGLE_GL_EXT_framebuffer_object, -1, STATE_RENDERBUFFER_PARAMETER },
    { STATE_NAME(GL_RENDERBUFFER_ALPHA_SIZE), TYPE_5GLint, -1, BUGLE_GL_EXT_framebuffer_object, -1, STATE_RENDERBUFFER_PARAMETER },
    { STATE_NAME(GL_RENDERBUFFER_DEPTH_SIZE), TYPE_5GLint, -1, BUGLE_GL_EXT_framebuffer_object, -1, STATE_RENDERBUFFER_PARAMETER },
    { STATE_NAME(GL_RENDERBUFFER_STENCIL_SIZE), TYPE_5GLint, -1, BUGLE_GL_EXT_framebuffer_object, -1, STATE_RENDERBUFFER_PARAMETER },
    { STATE_NAME(GL_RENDERBUFFER_SAMPLES), TYPE_5GLint, -1, BUGLE_GL_EXT_framebuffer_multisample, -1, STATE_RENDERBUFFER_PARAMETER },
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

/* Note: the below is used only if FBO is already available so we don't
 * have to explicitly check for it.
 */
static const state_info framebuffer_parameter_state[] =
{
    { STATE_NAME(GL_DRAW_BUFFER), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, BUGLE_GL_ATI_draw_buffers, STATE_DRAW_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_DRAW_BUFFER0), TYPE_6GLenum, -1, BUGLE_GL_ATI_draw_buffers, -1, STATE_DRAW_FRAMEBUFFER_PARAMETER, spawn_draw_buffers },
    { STATE_NAME(GL_READ_BUFFER), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_READ_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_AUX_BUFFERS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_DRAW_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_MAX_DRAW_BUFFERS), TYPE_5GLint, -1, BUGLE_GL_ATI_draw_buffers, -1, STATE_DRAW_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_RGBA_MODE), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_DRAW_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_INDEX_MODE), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_DRAW_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_DOUBLEBUFFER), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_DRAW_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_STEREO), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_DRAW_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_SAMPLE_BUFFERS), TYPE_5GLint, -1, BUGLE_GL_ARB_multisample, -1, STATE_DRAW_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_SAMPLES), TYPE_5GLint, -1, BUGLE_GL_ARB_multisample, -1, STATE_DRAW_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_SAMPLE_POSITION), TYPE_7GLfloat, 2, BUGLE_GL_ARB_texture_multisample, -1, STATE_MULTISAMPLE, spawn_sample_position },
    { STATE_NAME(GL_RED_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_DRAW_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_GREEN_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_DRAW_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_BLUE_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_DRAW_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_ALPHA_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_DRAW_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_INDEX_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_DRAW_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_DEPTH_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_DRAW_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_STENCIL_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_DRAW_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_ACCUM_RED_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_DRAW_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_ACCUM_GREEN_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_DRAW_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_ACCUM_BLUE_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_DRAW_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_ACCUM_ALPHA_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_DRAW_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_MAX_COLOR_ATTACHMENTS), TYPE_5GLint, -1, BUGLE_GL_EXT_framebuffer_object, -1, STATE_DRAW_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_MAX_SAMPLES), TYPE_5GLint, -1, BUGLE_GL_EXT_framebuffer_multisample, -1, STATE_DRAW_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_FRAMEBUFFER_SRGB_CAPABLE_EXT), TYPE_9GLboolean, -1, BUGLE_GL_EXT_framebuffer_sRGB, -1, STATE_DRAW_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_RGBA_SIGNED_COMPONENTS_EXT), TYPE_9GLboolean, 4, BUGLE_GL_EXT_packed_float, -1, STATE_DRAW_FRAMEBUFFER_PARAMETER },
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info framebuffer_attachment_parameter_state[] =
{
    { STATE_NAME(GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE), TYPE_6GLenum, -1, BUGLE_GL_EXT_framebuffer_object, -1, STATE_FRAMEBUFFER_ATTACHMENT_PARAMETER },
    { STATE_NAME(GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME), TYPE_5GLint, -1, BUGLE_GL_EXT_framebuffer_object, -1, STATE_FRAMEBUFFER_ATTACHMENT_PARAMETER },
    { STATE_NAME(GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL), TYPE_5GLint, -1, BUGLE_GL_EXT_framebuffer_object, -1, STATE_FRAMEBUFFER_ATTACHMENT_PARAMETER },
    { STATE_NAME(GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE), TYPE_6GLenum, -1, BUGLE_GL_EXT_framebuffer_object, -1, STATE_FRAMEBUFFER_ATTACHMENT_PARAMETER },
    { STATE_NAME(GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER), TYPE_5GLint, -1, BUGLE_GL_EXT_framebuffer_object, -1, STATE_FRAMEBUFFER_ATTACHMENT_PARAMETER },
    { STATE_NAME(GL_FRAMEBUFFER_ATTACHMENT_LAYERED), TYPE_9GLboolean, -1, BUGLE_GL_ARB_geometry_shader4, -1, STATE_FRAMEBUFFER_ATTACHMENT_PARAMETER },
    { STATE_NAME(GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING), TYPE_6GLenum, -1, BUGLE_GL_ARB_framebuffer_object, -1, STATE_FRAMEBUFFER_ATTACHMENT_PARAMETER },
    { STATE_NAME(GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE), TYPE_6GLenum, -1, BUGLE_GL_ARB_framebuffer_object, -1, STATE_FRAMEBUFFER_ATTACHMENT_PARAMETER },
    { STATE_NAME(GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE), TYPE_5GLint, -1, BUGLE_GL_ARB_framebuffer_object, -1, STATE_FRAMEBUFFER_ATTACHMENT_PARAMETER },
    { STATE_NAME(GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE), TYPE_5GLint, -1, BUGLE_GL_ARB_framebuffer_object, -1, STATE_FRAMEBUFFER_ATTACHMENT_PARAMETER },
    { STATE_NAME(GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE), TYPE_5GLint, -1, BUGLE_GL_ARB_framebuffer_object, -1, STATE_FRAMEBUFFER_ATTACHMENT_PARAMETER },
    { STATE_NAME(GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE), TYPE_5GLint, -1, BUGLE_GL_ARB_framebuffer_object, -1, STATE_FRAMEBUFFER_ATTACHMENT_PARAMETER },
    { STATE_NAME(GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE), TYPE_5GLint, -1, BUGLE_GL_ARB_framebuffer_object, -1, STATE_FRAMEBUFFER_ATTACHMENT_PARAMETER },
    { STATE_NAME(GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE), TYPE_5GLint, -1, BUGLE_GL_ARB_framebuffer_object, -1, STATE_FRAMEBUFFER_ATTACHMENT_PARAMETER },
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info transform_feedback_record_state[] =
{
    { STATE_NAME(GL_TRANSFORM_FEEDBACK_RECORD_NV), TYPE_11GLxfbattrib, -1, BUGLE_GL_NV_transform_feedback, -1, STATE_INDEXED },
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info transform_feedback_buffer_state[] =
{
    { STATE_NAME(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING_NV), TYPE_5GLint, -1, BUGLE_GL_NV_transform_feedback, -1, STATE_INDEXED },
    { STATE_NAME(GL_TRANSFORM_FEEDBACK_BUFFER_START_NV), TYPE_5GLint, -1, BUGLE_GL_NV_transform_feedback, -1, STATE_INDEXED },
    { STATE_NAME(GL_TRANSFORM_FEEDBACK_BUFFER_SIZE_NV), TYPE_5GLint, -1, BUGLE_GL_NV_transform_feedback, -1, STATE_INDEXED },
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

/* This exists to simplify the work of gldump.c */
const state_info * const all_state[] =
{
    global_state,
    tex_parameter_state,
    tex_buffer_state,
    tex_level_parameter_state,
    tex_unit_state,
    tex_gen_state,
    light_state,
    material_state,
    color_table_parameter_state,
    convolution_parameter_state,
    histogram_parameter_state,
    minmax_parameter_state,
    vertex_attrib_state,
    query_state,
    query_object_state,
    buffer_parameter_state,
    vertex_array_parameter_state,
    shader_state,
    program_state,
    old_program_object_state,
    old_program_state,
    framebuffer_attachment_parameter_state,
    framebuffer_parameter_state,
    renderbuffer_parameter_state,
    transform_feedback_record_state,
    transform_feedback_buffer_state,
    NULL
};

static void get_helper(const glstate *state,
                       GLdouble *d, GLfloat *f, GLint *i,
                       budgie_type *in_type,
                       void (BUDGIEAPIP get_double)(GLenum, GLenum, GLdouble *),
                       void (BUDGIEAPIP get_float)(GLenum, GLenum, GLfloat *),
                       void (BUDGIEAPIP get_int)(GLenum, GLenum, GLint *))
{
    if (state->info->type == TYPE_8GLdouble && get_double != NULL)
    {
        get_double(state->target, state->info->pname, d);
        *in_type = TYPE_8GLdouble;
    }
    else if ((state->info->type == TYPE_8GLdouble || state->info->type == TYPE_7GLfloat)
        && get_float != NULL)
    {
        get_float(state->target, state->info->pname, f);
        *in_type = TYPE_7GLfloat;
    }
    else
    {
        get_int(state->target, state->info->pname, i);
        *in_type = TYPE_5GLint;
    }
}

static void uniform_types(GLenum type,
                          budgie_type *in_type,
                          budgie_type *out_type)
{
    switch (type)
    {
    case GL_FLOAT: *in_type = TYPE_7GLfloat; *out_type = TYPE_7GLfloat; break;
    case GL_FLOAT_VEC2: *in_type = TYPE_7GLfloat; *out_type = TYPE_7GLfloat; break;
    case GL_FLOAT_VEC3: *in_type = TYPE_7GLfloat; *out_type = TYPE_7GLfloat; break;
    case GL_FLOAT_VEC4: *in_type = TYPE_7GLfloat; *out_type = TYPE_7GLfloat; break;
    case GL_INT: *in_type = TYPE_5GLint; *out_type = TYPE_5GLint; break;
    case GL_INT_VEC2: *in_type = TYPE_5GLint; *out_type = TYPE_5GLint; break;
    case GL_INT_VEC3: *in_type = TYPE_5GLint; *out_type = TYPE_5GLint; break;
    case GL_INT_VEC4: *in_type = TYPE_5GLint; *out_type = TYPE_5GLint; break;
    case GL_BOOL: *in_type = TYPE_5GLint; *out_type = TYPE_9GLboolean; break;
    case GL_BOOL_VEC2: *in_type = TYPE_5GLint; *out_type = TYPE_9GLboolean; break;
    case GL_BOOL_VEC3: *in_type = TYPE_5GLint; *out_type = TYPE_9GLboolean; break;
    case GL_BOOL_VEC4: *in_type = TYPE_5GLint; *out_type = TYPE_9GLboolean; break;
    case GL_FLOAT_MAT2: *in_type = TYPE_7GLfloat; *out_type = TYPE_7GLfloat; break;
    case GL_FLOAT_MAT3: *in_type = TYPE_7GLfloat; *out_type = TYPE_7GLfloat; break;
    case GL_FLOAT_MAT4: *in_type = TYPE_7GLfloat; *out_type = TYPE_7GLfloat; break;
    case GL_SAMPLER_1D:
    case GL_SAMPLER_2D:
    case GL_SAMPLER_3D:
    case GL_SAMPLER_CUBE:
    case GL_SAMPLER_1D_SHADOW:
    case GL_SAMPLER_2D_SHADOW:
    case GL_SAMPLER_2D_RECT_ARB:
    case GL_SAMPLER_2D_RECT_SHADOW_ARB:
#ifdef GL_EXT_texture_array
    case GL_SAMPLER_1D_ARRAY_EXT:
    case GL_SAMPLER_2D_ARRAY_EXT:
    case GL_SAMPLER_1D_ARRAY_SHADOW_EXT:
    case GL_SAMPLER_2D_ARRAY_SHADOW_EXT:
#endif
        *in_type = TYPE_5GLint; *out_type = TYPE_5GLint; break;
#ifdef GL_VERSION_2_1
    case GL_FLOAT_MAT2x3: *in_type = TYPE_7GLfloat; *out_type = TYPE_7GLfloat; break;
    case GL_FLOAT_MAT2x4: *in_type = TYPE_7GLfloat; *out_type = TYPE_7GLfloat; break;
    case GL_FLOAT_MAT3x2: *in_type = TYPE_7GLfloat; *out_type = TYPE_7GLfloat; break;
    case GL_FLOAT_MAT3x4: *in_type = TYPE_7GLfloat; *out_type = TYPE_7GLfloat; break;
    case GL_FLOAT_MAT4x2: *in_type = TYPE_7GLfloat; *out_type = TYPE_7GLfloat; break;
    case GL_FLOAT_MAT4x3: *in_type = TYPE_7GLfloat; *out_type = TYPE_7GLfloat; break;
#endif
    default:
        abort();
    }
}

void bugle_state_get_raw(const glstate *state, bugle_state_raw *wrapper)
{
    GLenum error;
    GLdouble d[16];
    GLfloat f[16];
    GLint i[16];
    GLint64 i64[16];
    GLuint ui[16];
    GLboolean b[16];
    GLvoid *p[16];
    GLpolygonstipple stipple;
    GLxfbattrib xfbattrib;
    GLsizei length;
    GLint max_length;
    void *in;
    budgie_type in_type, out_type;
    int in_length;
    char *str = NULL;  /* Set to non-NULL for human-readable string output */

    GLint old_texture, old_buffer, old_vertex_array, old_program;
    GLint old_unit, old_client_unit, old_framebuffer, old_renderbuffer;
    bugle_bool flag_active_texture = BUGLE_FALSE;
    GLenum pname;

    if (!state->info) return;
    if (state->enum_name) pname = state->enum_name;
    else if (state->info->pname) pname = state->info->pname;
    else pname = state->target;
    in_type = state->info->type;
    in_length = state->info->length;
    out_type = state->info->type;
    wrapper->data = NULL; /* Set to non-NULL if manual conversion */

    if ((error = CALL(glGetError)()) != GL_NO_ERROR)
    {
        const char *name;
        name = bugle_api_enum_name(error, BUGLE_API_EXTENSION_BLOCK_GL);
        if (name)
            bugle_log_printf("glstate", "get", BUGLE_LOG_WARNING,
                             "%s preceeding %s", name, state->name ? state->name : "root");
        else
            bugle_log_printf("glstate", "get", BUGLE_LOG_WARNING,
                             "OpenGL error %#08x preceeding %s",
                             (unsigned int) error, state->name ? state->name : "root");
        bugle_log_printf("glstate", "get", BUGLE_LOG_DEBUG,
                         "pname = %s target = %s binding = %s face = %s level = %d object = %u",
                         bugle_api_enum_name(pname, BUGLE_API_EXTENSION_BLOCK_GL),
                         bugle_api_enum_name(state->target, BUGLE_API_EXTENSION_BLOCK_GL),
                         bugle_api_enum_name(state->binding, BUGLE_API_EXTENSION_BLOCK_GL),
                         bugle_api_enum_name(state->face, BUGLE_API_EXTENSION_BLOCK_GL),
                         (int) state->level,
                         (unsigned int) state->object);
    }

    if ((state->info->flags & STATE_MULTIPLEX_ACTIVE_TEXTURE)
        && bugle_gl_has_extension_group(BUGLE_GL_ARB_multitexture))
    {
        CALL(glGetIntegerv)(GL_ACTIVE_TEXTURE, &old_unit);
        CALL(glGetIntegerv)(GL_CLIENT_ACTIVE_TEXTURE, &old_client_unit);
        CALL(glActiveTexture)(state->unit);
        if (state->unit > get_texture_coord_units())
            CALL(glClientActiveTexture)(state->unit);
        flag_active_texture = BUGLE_TRUE;
    }
    if (state->info->flags & STATE_MULTIPLEX_BIND_VERTEX_ARRAY)
    {
        CALL(glGetIntegerv)(GL_VERTEX_ARRAY_BINDING, &old_vertex_array);
        CALL(glBindVertexArray)(state->object);
    }
    if (state->info->flags & STATE_MULTIPLEX_BIND_BUFFER)
    {
        CALL(glGetIntegerv)(GL_ARRAY_BUFFER_BINDING, &old_buffer);
        CALL(glBindBuffer)(GL_ARRAY_BUFFER, state->object);
    }
    if (state->info->flags & STATE_MULTIPLEX_BIND_PROGRAM)
    {
        CALL(glGetProgramivARB)(state->target, GL_PROGRAM_BINDING_ARB, &old_program);
        CALL(glBindProgramARB)(state->target, state->object);
    }
    if ((state->info->flags & STATE_MULTIPLEX_BIND_DRAW_FRAMEBUFFER)
        && bugle_gl_has_framebuffer_object())
    {
        old_framebuffer = bugle_gl_get_draw_framebuffer_binding();
        bugle_gl_bind_draw_framebuffer(state->object);
    }
    else if ((state->info->flags & STATE_MULTIPLEX_BIND_READ_FRAMEBUFFER)
             && bugle_gl_has_framebuffer_object())
    {
        old_framebuffer = bugle_gl_get_read_framebuffer_binding();
        bugle_gl_bind_read_framebuffer(state->object);
    }

    if (state->info->flags & STATE_MULTIPLEX_BIND_RENDERBUFFER)
    {
        CALL(glGetIntegerv)(state->binding, &old_renderbuffer);
        bugle_glBindRenderbuffer(state->target, state->object);
    }
    if ((state->info->flags & STATE_MULTIPLEX_BIND_TEXTURE)
        && state->binding) /* binding of 0 means a proxy texture */
    {
        CALL(glGetIntegerv)(state->binding, &old_texture);
        CALL(glBindTexture)(state->target, state->object);
    }

    switch (state->info->flags & STATE_MODE_MASK)
    {
    case STATE_MODE_GLOBAL:
        if (state->info->type == TYPE_PKc)
        {
            str = (char *) CALL(glGetString)(pname);
            if (str) str = bugle_strdup(str);
            else str = bugle_strdup("(nil)");
        }
        else if (state->info->type == TYPE_9GLboolean)
            CALL(glGetBooleanv)(pname, b);
        else if (state->info->type == TYPE_P6GLvoid)
            CALL(glGetPointerv)(pname, p);
        else if (state->info->type == TYPE_8GLdouble)
            CALL(glGetDoublev)(pname, d);
        else if (state->info->type == TYPE_7GLfloat)
            CALL(glGetFloatv)(pname, f);
        else if (state->info->type == TYPE_7GLint64)
            CALL(glGetInteger64v)(pname, i64);
        else
        {
            CALL(glGetIntegerv)(pname, i);
            in_type = TYPE_5GLint;
        }
        break;
    case STATE_MODE_ENABLED:
        b[0] = CALL(glIsEnabled)(pname);
        in_type = TYPE_9GLboolean;
        break;
    case STATE_MODE_INDEXED:
        if (state->info->type == TYPE_9GLboolean)
            CALL(glGetBooleani_v)(pname, state->level, b);
        else if (state->info->type == TYPE_7GLint64)
            CALL(glGetInteger64i_v)(pname, state->level, i64);
        else if (state->info->type == TYPE_11GLxfbattrib)
            CALL(glGetIntegeri_v)(pname, state->level, (GLint *) &xfbattrib);
        else if (state->info->type == TYPE_PKc)
        {
            str = (char *) CALL(glGetStringi)(pname, state->level);
            if (str) str = bugle_strdup(str);
            else str = bugle_strdup("(nil)");
        }
        else
        {
            CALL(glGetIntegeri_v)(pname, state->level, i);
            in_type = TYPE_5GLint;
        }
        break;
    case STATE_MODE_ENABLED_INDEXED:
        b[0] = CALL(glIsEnabledi)(pname, state->numeric_name);
        in_type = TYPE_9GLboolean;
        break;
    case STATE_MODE_SAMPLE_MASK_VALUE:
        CALL(glGetInteger64i_v)(pname, state->level / 32, i64);
        b[0] = ((i64[0] >> (state->level & 31)) & 1) ? GL_TRUE : GL_FALSE;
        in_type = TYPE_9GLboolean;
        break;
    case STATE_MODE_TEXTURE_ENV:
    case STATE_MODE_TEXTURE_FILTER_CONTROL:
    case STATE_MODE_POINT_SPRITE:
        get_helper(state, d, f, i, &in_type, NULL,
                   CALL(glGetTexEnvfv), CALL(glGetTexEnviv));
        break;
    case STATE_MODE_TEX_PARAMETER:
        get_helper(state, d, f, i, &in_type, NULL,
                   CALL(glGetTexParameterfv), CALL(glGetTexParameteriv));
        break;
    case STATE_MODE_TEX_LEVEL_PARAMETER:
        if (state->info->type == TYPE_8GLdouble || state->info->type == TYPE_7GLfloat)
        {
            CALL(glGetTexLevelParameterfv)(state->face, state->level, pname, f);
            in_type = TYPE_7GLfloat;
        }
        else
        {
            CALL(glGetTexLevelParameteriv)(state->face, state->level, pname, i);
            in_type = TYPE_5GLint;
        }
        break;
    case STATE_MODE_TEX_GEN:
        get_helper(state, d, f, i, &in_type, CALL(glGetTexGendv),
                   CALL(glGetTexGenfv), CALL(glGetTexGeniv));
        break;
    case STATE_MODE_LIGHT:
        get_helper(state, d, f, i, &in_type, NULL,
                   CALL(glGetLightfv), CALL(glGetLightiv));
        break;
    case STATE_MODE_MATERIAL:
        get_helper(state, d, f, i, &in_type, NULL,
                   CALL(glGetMaterialfv), CALL(glGetMaterialiv));
        break;
    case STATE_MODE_CLIP_PLANE:
        CALL(glGetClipPlane)(state->target, d);
        in_type = TYPE_8GLdouble;
        break;
    case STATE_MODE_POLYGON_STIPPLE:
        CALL(glGetPolygonStipple)((GLubyte *) stipple);
        break;
    case STATE_MODE_COLOR_TABLE_PARAMETER:
        get_helper(state, d, f, i, &in_type, NULL,
                   CALL(glGetColorTableParameterfv), CALL(glGetColorTableParameteriv));
        break;
    case STATE_MODE_CONVOLUTION_PARAMETER:
        get_helper(state, d, f, i, &in_type, NULL,
                   CALL(glGetConvolutionParameterfv), CALL(glGetConvolutionParameteriv));
        break;
    case STATE_MODE_HISTOGRAM_PARAMETER:
        get_helper(state, d, f, i, &in_type, NULL,
                   CALL(glGetHistogramParameterfv), CALL(glGetHistogramParameteriv));
        break;
    case STATE_MODE_MINMAX_PARAMETER:
        get_helper(state, d, f, i, &in_type, NULL,
                   CALL(glGetMinmaxParameterfv), CALL(glGetMinmaxParameteriv));
        break;
    case STATE_MODE_VERTEX_ATTRIB:
        if (state->info->type == TYPE_P6GLvoid)
            CALL(glGetVertexAttribPointerv)(state->level, pname, p);
        else if (state->info->type == TYPE_8GLdouble)
            CALL(glGetVertexAttribdv)(state->level, pname, d);
        else if (state->info->type == TYPE_7GLfloat)
            CALL(glGetVertexAttribfv)(state->level, pname, f);
        else
        {
            /* xorg-server 1.2.0 maps the iv and fv forms to the NV
             * variant, which does not support the two boolean queries
             * (_ENABLED and _NORMALIZED). We work around that by using
             * the dv form.
             */
            CALL(glGetVertexAttribdv)(state->level, pname, d);
            in_type = TYPE_8GLdouble;
        }
        break;
    case STATE_MODE_QUERY:
        CALL(glGetQueryiv)(state->target, pname, i);
        in_type = TYPE_5GLint;
        break;
    case STATE_MODE_QUERY_OBJECT:
        if (state->info->type == TYPE_6GLuint)
            CALL(glGetQueryObjectuiv)(state->object, pname, ui);
        else
        {
            CALL(glGetQueryObjectiv)(state->object, pname, i);
            in_type = TYPE_5GLint;
        }
        break;
    case STATE_MODE_BUFFER_PARAMETER:
        if (state->info->type == TYPE_P6GLvoid)
            CALL(glGetBufferPointerv)(GL_ARRAY_BUFFER, pname, p);
        else
        {
            CALL(glGetBufferParameteriv)(GL_ARRAY_BUFFER, pname, i);
            in_type = TYPE_5GLint;
        }
        break;
    case STATE_MODE_SHADER:
        bugle_glGetShaderiv(state->object, pname, i);
        in_type = TYPE_5GLint;
        break;
    case STATE_MODE_PROGRAM:
        bugle_glGetProgramiv(state->object, pname, i);
        in_type = TYPE_5GLint;
        break;
    case STATE_MODE_SHADER_INFO_LOG:
        max_length = 1;
        bugle_glGetShaderiv(state->object, GL_INFO_LOG_LENGTH, &max_length);
        if (max_length < 1) max_length = 1;
        str = BUGLE_NMALLOC(max_length, GLchar);
        bugle_glGetShaderInfoLog(state->object, max_length, &length, (GLchar *) str);
        /* AMD don't always nul-terminate empty strings */
        if (length >= 0 && length < max_length) str[length] = '\0';
        break;
    case STATE_MODE_PROGRAM_INFO_LOG:
        max_length = 1;
        bugle_glGetProgramiv(state->object, GL_INFO_LOG_LENGTH, &max_length);
        if (max_length < 1) max_length = 1;
        str = BUGLE_NMALLOC(max_length, GLchar);
        bugle_glGetProgramInfoLog(state->object, max_length, &length, (GLchar *) str);
        /* AMD don't always nul-terminate empty strings */
        if (length >= 0 && length < max_length) str[length] = '\0';
        break;
    case STATE_MODE_SHADER_SOURCE:
        max_length = 1;
        bugle_glGetShaderiv(state->object, GL_SHADER_SOURCE_LENGTH, &max_length);
        if (max_length < 1) max_length = 1;
        str = BUGLE_NMALLOC(max_length, GLcharARB);
        bugle_glGetShaderSource(state->object, max_length, &length, (GLchar *) str);
        /* AMD don't always nul-terminate empty strings */
        if (length >= 0 && length < max_length) str[length] = '\0';
        break;
    case STATE_MODE_UNIFORM:
        {
            GLsizei size;
            GLenum type;
            GLchar dummy[1];
            GLsizei units;  /* number of the base type in the full type */
            budgie_type composite_type;
            void *buffer;

            bugle_glGetActiveUniform(state->object, state->level, 1, NULL,
                                     &size, &type, dummy);
            uniform_types(type, &in_type, &out_type);
            composite_type = bugle_gl_type_to_type(type);
            units = budgie_type_size(composite_type) / budgie_type_size(in_type);
            buffer = bugle_malloc(size * units * budgie_type_size(in_type));
            wrapper->data = bugle_malloc(size * units * budgie_type_size(out_type));
            wrapper->type = composite_type;
            if (size == 1)
                wrapper->length = -1;
            else
                wrapper->length = size;
            if (in_type == TYPE_7GLfloat)
                bugle_glGetUniformfv(state->object, state->numeric_name, (GLfloat *) buffer);
            else
                bugle_glGetUniformiv(state->object, state->numeric_name, (GLint *) buffer);

            budgie_type_convert(wrapper->data, out_type, buffer, in_type, size * units);
            bugle_free(buffer);
        }
        break;
    case STATE_MODE_ATTRIB_LOCATION:
        i[0] = bugle_glGetAttribLocation(state->object, state->name);
        break;
    case STATE_MODE_ATTACHED_SHADERS:
        {
            GLuint *attached;

            max_length = 1;
            bugle_glGetProgramiv(state->object, GL_ATTACHED_SHADERS, &max_length);
            attached = BUGLE_NMALLOC(max_length, GLuint);
            bugle_glGetAttachedShaders(state->object, max_length, NULL, attached);
            wrapper->data = attached;
            wrapper->length = max_length;
            wrapper->type = TYPE_6GLuint;
        }
        break;
#if defined(GL_ARB_vertex_program) || defined(GL_ARB_fragment_program)
    case STATE_MODE_OLD_PROGRAM:
        if (state->info->type == TYPE_PKc)
        {
            CALL(glGetProgramivARB)(state->target, GL_PROGRAM_LENGTH_ARB, i);
            str = BUGLE_NMALLOC(i[0] + 1, char);
            str[i[0]] = '\0';
            CALL(glGetProgramStringARB)(state->target, pname, str);
        }
        else
        {
            CALL(glGetProgramivARB)(state->target, pname, i);
            in_type = TYPE_5GLint;
        }
        break;
    case STATE_MODE_PROGRAM_ENV_PARAMETER:
        CALL(glGetProgramEnvParameterdvARB)(state->target, state->level, d);
        in_type = TYPE_8GLdouble;
        break;
    case STATE_MODE_PROGRAM_LOCAL_PARAMETER:
        CALL(glGetProgramLocalParameterdvARB)(state->target, state->level, d);
        in_type = TYPE_8GLdouble;
        break;
#endif
    case STATE_MODE_COMPRESSED_TEXTURE_FORMATS:
        {
            GLint count;
            GLint *formats;
            GLenum *out;

            CALL(glGetIntegerv)(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &count);
            formats = BUGLE_NMALLOC(count, GLint);
            out = BUGLE_NMALLOC(count, GLenum);
            CALL(glGetIntegerv)(GL_COMPRESSED_TEXTURE_FORMATS, formats);
            budgie_type_convert(out, TYPE_6GLenum, formats, TYPE_5GLint, count);
            wrapper->data = out;
            wrapper->length = count;
            wrapper->type = TYPE_6GLenum;
            bugle_free(formats);
        }
        break;
    case STATE_MODE_FRAMEBUFFER_ATTACHMENT_PARAMETER:
        bugle_glGetFramebufferAttachmentParameteriv(bugle_gl_draw_framebuffer_target(),
                                                    state->level,
                                                    pname, i);
        in_type = TYPE_5GLint;
        break;
    case STATE_MODE_RENDERBUFFER_PARAMETER:
        bugle_glGetRenderbufferParameteriv(state->target, pname, i);
        in_type = TYPE_5GLint;
        break;
    case STATE_MODE_MULTISAMPLE:
        CALL(glGetMultisamplefv)(pname, state->level, f);
        in_type = TYPE_7GLfloat;
        break;
    default:
        abort();
    }

    if (flag_active_texture)
    {
        CALL(glActiveTexture)(old_unit);
        if (state->unit > get_texture_coord_units())
            CALL(glClientActiveTexture)(old_client_unit);
    }
    if (state->info->flags & STATE_MULTIPLEX_BIND_BUFFER)
    {
        CALL(glBindBuffer)(GL_ARRAY_BUFFER, old_buffer);
    }
    if (state->info->flags & STATE_MULTIPLEX_BIND_VERTEX_ARRAY)
        CALL(glBindVertexArray)(old_vertex_array);
    if (state->info->flags & STATE_MULTIPLEX_BIND_PROGRAM)
        CALL(glBindProgramARB)(state->target, old_program);
    if ((state->info->flags & STATE_MULTIPLEX_BIND_DRAW_FRAMEBUFFER)
        && bugle_gl_has_framebuffer_object())
        bugle_gl_bind_draw_framebuffer(old_framebuffer);
    else if ((state->info->flags & STATE_MULTIPLEX_BIND_READ_FRAMEBUFFER)
        && bugle_gl_has_framebuffer_object())
        bugle_gl_bind_read_framebuffer(old_framebuffer);

    if (state->info->flags & STATE_MULTIPLEX_BIND_RENDERBUFFER)
        bugle_glBindRenderbuffer(state->target, old_renderbuffer);
    if ((state->info->flags & STATE_MULTIPLEX_BIND_TEXTURE)
        && state->binding)
        CALL(glBindTexture)(state->target, old_texture);

    if ((error = CALL(glGetError)()) != GL_NO_ERROR)
    {
        const char *name;
        name = bugle_api_enum_name(error, BUGLE_API_EXTENSION_BLOCK_GL);
        if (name)
            bugle_log_printf("glstate", "get", BUGLE_LOG_WARNING,
                             "%s generated by state %s",
                             name, state->name ? state->name : "root");
        else
            bugle_log_printf("glstate", "get", BUGLE_LOG_WARNING,
                             "OpenGL error %#08x generated by state %s",
                             (unsigned int) error, state->name ? state->name : "root");
        while (CALL(glGetError)() != GL_NO_ERROR)
            ; /* consume any further errors */
        return;
    }

    if (str)
    {
        wrapper->data = str;
        wrapper->length = strlen(str);
        wrapper->type = TYPE_c;
        return;
    }

    if (!wrapper->data)
    {
        switch (in_type)
        {
        case TYPE_8GLdouble: in = d; break;
        case TYPE_7GLfloat: in = f; break;
        case TYPE_5GLint: in = i; break;
        case TYPE_6GLuint: in = ui; break;
        case TYPE_7GLint64: in = i64; break;
        case TYPE_9GLboolean: in = b; break;
        case TYPE_P6GLvoid: in = p; break;
        case TYPE_16GLpolygonstipple: in = stipple; break;
        case TYPE_11GLxfbattrib: in = &xfbattrib; break;
        default: abort();
        }

        wrapper->data = bugle_nmalloc(abs(in_length), budgie_type_size(out_type));
        wrapper->type = out_type;
        wrapper->length = in_length;
        budgie_type_convert(wrapper->data, wrapper->type, in, in_type, abs(wrapper->length));
    }
}

char *bugle_state_get_string(const glstate *state)
{
    bugle_state_raw wrapper;
    char *ans;

    if (!state->info) return NULL; /* root */
    bugle_state_get_raw(state, &wrapper);
    if (!wrapper.data)
        return bugle_strdup("<GL error>");

    if (wrapper.type == TYPE_Pc)
        ans = bugle_strdup((const char *) wrapper.data); /* bugle_string_io(dump_string_wrapper, (char *) wrapper.data); */
    else
    {
        bugle_io_writer *writer;

        writer = bugle_io_writer_mem_new(64);
        budgie_dump_any_type_extended(wrapper.type, wrapper.data, -1, wrapper.length, NULL, writer);
        ans = bugle_io_writer_mem_get(writer);
        bugle_io_writer_close(writer);
    }
    bugle_free(wrapper.data);
    return ans;
}

void bugle_state_get_children(const glstate *state, linked_list *children)
{
    if (state->spawn_children)
    {
        GLenum error;
        (*state->spawn_children)(state, children);
        if ((error = CALL(glGetError)()) != GL_NO_ERROR)
        {
            const char *name;
            name = bugle_api_enum_name(error, BUGLE_API_EXTENSION_BLOCK_GL);
            if (name)
                bugle_log_printf("glstate", "get", BUGLE_LOG_WARNING,
                                 "%s generated by spawn_children for %s", name, state->name ? state->name : "root");
            else
                bugle_log_printf("glstate", "get", BUGLE_LOG_WARNING,
                                 "OpenGL error %#08x generated by spawn_children for %s",
                                 (unsigned int) error, state->name ? state->name : "root");
            bugle_log_printf("glstate", "get", BUGLE_LOG_DEBUG,
                             "pname = %s target = %s binding = %s face = %s level = %d object = %u",
                             bugle_api_enum_name(state->info ? state->info->pname : 0, BUGLE_API_EXTENSION_BLOCK_GL),
                             bugle_api_enum_name(state->target, BUGLE_API_EXTENSION_BLOCK_GL),
                             bugle_api_enum_name(state->binding, BUGLE_API_EXTENSION_BLOCK_GL),
                             bugle_api_enum_name(state->face, BUGLE_API_EXTENSION_BLOCK_GL),
                             (int) state->level,
                             (unsigned int) state->object);
        }
    }
    else
        bugle_list_init(children, bugle_free);
}

void bugle_state_clear(glstate *self)
{
    if (self->name) bugle_free(self->name);
}

static void spawn_children_query(const glstate *self, linked_list *children)
{
    bugle_list_init(children, bugle_free);
    make_leaves(self, query_state, children);
}

static void spawn_children_query_object(const glstate *self, linked_list *children)
{
    bugle_list_init(children, bugle_free);
    make_leaves(self, query_object_state, children);
}

static void spawn_children_buffer_parameter(const glstate *self, linked_list *children)
{
    bugle_list_init(children, bugle_free);
    make_leaves(self, buffer_parameter_state, children);
}

static void spawn_children_vertex_array_parameter(const glstate *self, linked_list *children)
{
    bugle_list_init(children, bugle_free);
    make_leaves(self, vertex_array_parameter_state, children);
}

#if defined(GL_ARB_vertex_program) || defined(GL_ARB_fragment_program)
static void spawn_children_old_program_object(const glstate *self, linked_list *children)
{
    bugle_uint32_t mask = 0;
    GLint max_local, i;
    GLdouble local[4];
    static const state_info program_local_state =
    {
        NULL, GL_NONE, TYPE_8GLdouble, 4, BUGLE_EXTGROUP_old_program, -1, STATE_PROGRAM_LOCAL_PARAMETER
    };

    bugle_list_init(children, bugle_free);
#ifdef GL_ARB_vertex_program
    if (self->target == GL_ARB_vertex_program) mask = STATE_SELECT_FRAGMENT;
#endif
#ifdef GL_ARB_fragment_program
    if (self->target == GL_ARB_fragment_program) mask = STATE_SELECT_VERTEX;
#endif
    make_leaves_conditional(self, old_program_object_state, 0, mask, children);

    CALL(glGetProgramivARB)(self->target, GL_MAX_PROGRAM_LOCAL_PARAMETERS_ARB, &max_local);
    for (i = 0; i < max_local; i++)
    {
        CALL(glGetProgramLocalParameterdvARB)(self->target, i, local);
        if (local[0] || local[1] || local[2] || local[3])
        {
            glstate *child;

            child = BUGLE_MALLOC(glstate);
            *child = *self;
            child->level = i;
            child->info = &program_local_state;
            child->spawn_children = NULL;
            child->name = bugle_asprintf("Local[%lu]", (unsigned long) i);
            child->numeric_name = i;
            child->enum_name = 0;
            bugle_list_append(children, child);
        }
    }
}

static void spawn_children_old_program(const glstate *self, linked_list *children)
{
    bugle_uint32_t mask = 0;
    GLint max_env, i;
    GLdouble env[4];
    static const state_info program_env_state =
    {
        NULL, GL_NONE, TYPE_8GLdouble, 4, BUGLE_EXTGROUP_old_program, -1, STATE_PROGRAM_ENV_PARAMETER
    };

    bugle_list_init(children, bugle_free);
#ifdef GL_ARB_vertex_program
    if (self->target == GL_ARB_vertex_program) mask = STATE_SELECT_FRAGMENT;
#endif
#ifdef GL_ARB_fragment_program
    if (self->target == GL_ARB_fragment_program) mask = STATE_SELECT_VERTEX;
#endif
    make_leaves_conditional(self, old_program_state, 0, mask, children);
    CALL(glGetProgramivARB)(self->target, GL_MAX_PROGRAM_ENV_PARAMETERS_ARB, &max_env);
    for (i = 0; i < max_env; i++)
    {
        CALL(glGetProgramEnvParameterdvARB)(self->target, i, env);
        if (env[0] || env[1] || env[2] || env[3])
        {
            glstate *child;

            child = BUGLE_MALLOC(glstate);
            *child = *self;
            child->level = i;
            child->info = &program_env_state;
            child->spawn_children = NULL;
            child->name = bugle_asprintf("Env[%lu]", (unsigned long) i);
            child->numeric_name = i;
            child->enum_name = 0;
            bugle_list_append(children, child);
        }
    }
    make_objects(self, BUGLE_GLOBJECTS_OLD_PROGRAM, self->target, BUGLE_FALSE,
                 "%lu", spawn_children_old_program_object, NULL, children);
}
#endif /* GL_ARB_vertex_program || GL_ARB_fragment_program */

static void spawn_children_framebuffer_attachment(const glstate *self, linked_list *children)
{
    bugle_list_init(children, bugle_free);
    make_leaves(self, framebuffer_attachment_parameter_state, children);
}

static void make_framebuffer_attachment(const glstate *self,
                                        GLenum attachment,
                                        const char *format,
                                        long int index,
                                        linked_list *children)
{
    GLint type;
    glstate *child;

    bugle_glGetFramebufferAttachmentParameteriv(
        bugle_gl_draw_framebuffer_target(), attachment,
        GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
        &type);
    if (type != GL_NONE)
    {
        child = BUGLE_MALLOC(glstate);
        *child = *self;
        if (index < 0) child->name = bugle_strdup(format);
        else child->name = bugle_asprintf(format, index);
        child->info = NULL;
        child->numeric_name = index;
        child->enum_name = attachment;
        child->level = attachment;
        child->spawn_children = spawn_children_framebuffer_attachment;
        bugle_list_append(children, child);
    }
}

static void spawn_children_framebuffer_object(const glstate *self, linked_list *children)
{
    GLint old;
    GLint attachments;
    int i;

    bugle_list_init(children, bugle_free);
    old = bugle_gl_get_draw_framebuffer_binding();
    bugle_gl_bind_draw_framebuffer(self->object);
    make_leaves(self, framebuffer_parameter_state, children);
    if (self->object != 0)
    {
        CALL(glGetIntegerv)(GL_MAX_COLOR_ATTACHMENTS, &attachments);
        for (i = 0; i < attachments; i++)
            make_framebuffer_attachment(self, GL_COLOR_ATTACHMENT0 + i,
                                        "GL_COLOR_ATTACHMENT%ld",
                                        i, children);
        make_framebuffer_attachment(self, GL_DEPTH_ATTACHMENT,
                                    "GL_DEPTH_ATTACHMENT", -1, children);
        make_framebuffer_attachment(self, GL_STENCIL_ATTACHMENT,
                                    "GL_STENCIL_ATTACHMENT", -1, children);
    }

    bugle_gl_bind_draw_framebuffer(old);
}

static void spawn_children_framebuffer(const glstate *self, linked_list *children)
{
    bugle_list_init(children, bugle_free);
    make_objects(self, BUGLE_GLOBJECTS_FRAMEBUFFER, GL_NONE, BUGLE_TRUE,
                 "%lu", spawn_children_framebuffer_object, NULL, children);
}

static void spawn_children_renderbuffer_object(const glstate *self, linked_list *children)
{
    bugle_list_init(children, bugle_free);
    make_leaves(self, renderbuffer_parameter_state, children);
}

static void spawn_children_renderbuffer(const glstate *self, linked_list *children)
{
    bugle_list_init(children, bugle_free);
    make_objects(self, BUGLE_GLOBJECTS_RENDERBUFFER, self->target, BUGLE_FALSE,
                 "%lu", spawn_children_renderbuffer_object, NULL, children);
}

static void spawn_children_transform_feedback_buffer(const glstate *self, linked_list *children)
{
    GLint records;
    CALL(glGetIntegerv)(GL_TRANSFORM_FEEDBACK_ATTRIBS_NV, &records);
    bugle_list_init(children, bugle_free);
    if (self->level < records)
    {
        make_leaves(self, transform_feedback_record_state, children);
    }
    make_leaves(self, transform_feedback_buffer_state, children);
}

static void spawn_children_transform_feedback(const glstate *self, linked_list *children)
{
    GLint buffers;

    CALL(glGetIntegerv)(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS_NV, &buffers);
    bugle_list_init(children, bugle_free);
    make_counted(self, buffers, "%lu", 0, offsetof(glstate, level),
                 spawn_children_transform_feedback_buffer, NULL, children);
}

static void spawn_children_global(const glstate *self, linked_list *children)
{
    static const state_info enable =
    {
        NULL, GL_NONE, TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED
    };

    bugle_list_init(children, bugle_free);
    make_leaves(self, global_state, children);

    if (bugle_gl_has_extension_group(BUGLE_GL_ARB_occlusion_query))
    {
        make_fixed(self, query_enums, offsetof(glstate, target),
                   spawn_children_query, NULL, children);
        make_objects(self, BUGLE_GLOBJECTS_QUERY, GL_NONE, BUGLE_FALSE,
                     "Query[%lu]", spawn_children_query_object, NULL, children);
    }
    if (bugle_gl_has_extension_group(BUGLE_GL_ARB_vertex_buffer_object))
    {
        make_objects(self, BUGLE_GLOBJECTS_BUFFER, GL_NONE, BUGLE_FALSE,
                     "Buffer[%lu]", spawn_children_buffer_parameter, NULL, children);
    }
    if (bugle_gl_has_extension_group(BUGLE_GL_ARB_vertex_array_object))
    {
        make_objects(self, BUGLE_GLOBJECTS_VERTEX_ARRAY, GL_NONE, BUGLE_TRUE,
                     "VertexArray[%lu]", spawn_children_vertex_array_parameter, NULL, children);
    }
    if (bugle_gl_has_extension_group(BUGLE_GL_ARB_vertex_program))
        make_target(self, "GL_VERTEX_PROGRAM_ARB",
                    GL_VERTEX_PROGRAM_ARB,
                    0, spawn_children_old_program, &enable, children);
    if (bugle_gl_has_extension_group(BUGLE_GL_ARB_fragment_program))
        make_target(self, "GL_FRAGMENT_PROGRAM_ARB",
                    GL_FRAGMENT_PROGRAM_ARB,
                    0, spawn_children_old_program, &enable, children);
    if (bugle_gl_has_framebuffer_object())
    {
        make_target(self, "GL_FRAMEBUFFER",
                    GL_FRAMEBUFFER,
                    GL_DRAW_FRAMEBUFFER_BINDING,
                    spawn_children_framebuffer, NULL, children);
        make_target(self, "GL_RENDERBUFFER",
                    GL_RENDERBUFFER,
                    GL_RENDERBUFFER_BINDING,
                    spawn_children_renderbuffer, NULL, children);
    }
    if (bugle_gl_has_extension_group(BUGLE_GL_NV_transform_feedback))
    {
        make_target(self, "TransformFeedbackBuffers",
                    0,
                    GL_TRANSFORM_FEEDBACK_BUFFER_BINDING_NV,
                    spawn_children_transform_feedback, NULL, children);
    }
}

const glstate *bugle_state_get_root(void)
{
    static const glstate root =
    {
        "", 0, GL_NONE, GL_NONE, GL_NONE, GL_NONE, GL_NONE,
        0, 0, NULL, spawn_children_global
    };

    return &root;
}
