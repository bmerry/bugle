/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2008  Bruce Merry
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

/* Still TODO:
 * - multiple modelview matrices (for vertex/fragment program)
 * - paletted textures (need to extend color_table_parameter, which is
 *   complicated by the fact that the palette belongs to the texture not
 *   the target).
 * - EXT_framebuffer_blit and EXT_framebuffer_multisample
 * - all the 4th-gen extensions (DX10 equivalents)
 * - matrix stacks
 * - all the image state (textures, color tables, maps, pixel maps, convolution, stipple etc)
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <inttypes.h>
#include "xalloc.h"
#include "xvasprintf.h"
#include <bugle/log.h>
#include <bugle/misc.h>
#include <bugle/gl/glsl.h>
#include <bugle/gl/glstate.h>
#include <bugle/gl/glutils.h>
#include <bugle/gl/globjects.h>
#include <bugle/gl/glextensions.h>
#include <budgie/types.h>
#include <budgie/reflect.h>
#include <budgie/call.h>
#include "budgielib/defines.h"
#include "src/apitables.h"

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
#define STATE_MODE_MASK                     0x000000ff

#define STATE_MULTIPLEX_ACTIVE_TEXTURE      0x00000100    /* Set active texture */
#define STATE_MULTIPLEX_BIND_TEXTURE        0x00000200    /* Set current texture object */
#define STATE_MULTIPLEX_BIND_BUFFER         0x00000400    /* Set current buffer object (GL_ARRAY_BUFFER) */
#define STATE_MULTIPLEX_BIND_PROGRAM        0x00000800    /* Set current low-level program */
#define STATE_MULTIPLEX_BIND_FRAMEBUFFER    0x00001000    /* Set current framebuffer object */
#define STATE_MULTIPLEX_BIND_RENDERBUFFER   0x00002000    /* Set current renderbuffer object */

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
#define STATE_FRAMEBUFFER_ATTACHMENT_PARAMETER (STATE_MODE_FRAMEBUFFER_ATTACHMENT_PARAMETER | STATE_MULTIPLEX_BIND_FRAMEBUFFER)
#define STATE_FRAMEBUFFER_PARAMETER (STATE_MODE_GLOBAL | STATE_MULTIPLEX_BIND_FRAMEBUFFER)
#define STATE_RENDERBUFFER_PARAMETER (STATE_MODE_RENDERBUFFER_PARAMETER | STATE_MULTIPLEX_BIND_RENDERBUFFER)
#define STATE_INDEXED STATE_MODE_INDEXED
#define STATE_ENABLED_INDEXED STATE_MODE_ENABLED_INDEXED

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
#ifdef GL_ARB_texture_cube_map
    { STATE_NAME_EXT(GL_TEXTURE_CUBE_MAP_POSITIVE_X, _ARB), BUGLE_GL_ARB_texture_cube_map },
    { STATE_NAME_EXT(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, _ARB), BUGLE_GL_ARB_texture_cube_map },
    { STATE_NAME_EXT(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, _ARB), BUGLE_GL_ARB_texture_cube_map },
    { STATE_NAME_EXT(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, _ARB), BUGLE_GL_ARB_texture_cube_map },
    { STATE_NAME_EXT(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, _ARB), BUGLE_GL_ARB_texture_cube_map },
    { STATE_NAME_EXT(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, _ARB), BUGLE_GL_ARB_texture_cube_map },
#endif
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
#ifdef GL_ARB_occlusion_query
    { STATE_NAME_EXT(GL_SAMPLES_PASSED, _ARB), BUGLE_GL_ARB_occlusion_query },
#endif
#ifdef GL_EXT_timer_query
    { STATE_NAME(GL_TIME_ELAPSED_EXT), BUGLE_GL_EXT_timer_query },
#endif
#ifdef GL_NV_transform_feedback
    { STATE_NAME(GL_PRIMITIVES_GENERATED_NV), BUGLE_GL_NV_transform_feedback },
    { STATE_NAME(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN_NV), BUGLE_GL_NV_transform_feedback },
#endif
    { NULL, GL_NONE, 0 }
};

static const state_info vertex_attrib_state[] =
{
#ifdef GL_ARB_vertex_program
    { STATE_NAME_EXT(GL_VERTEX_ATTRIB_ARRAY_ENABLED, _ARB), TYPE_9GLboolean, -1, BUGLE_EXTGROUP_vertex_attrib, -1, STATE_VERTEX_ATTRIB },
    { STATE_NAME_EXT(GL_VERTEX_ATTRIB_ARRAY_SIZE, _ARB), TYPE_5GLint, -1, BUGLE_EXTGROUP_vertex_attrib, -1, STATE_VERTEX_ATTRIB },
    { STATE_NAME_EXT(GL_VERTEX_ATTRIB_ARRAY_STRIDE, _ARB), TYPE_5GLint, -1, BUGLE_EXTGROUP_vertex_attrib, -1, STATE_VERTEX_ATTRIB },
    { STATE_NAME_EXT(GL_VERTEX_ATTRIB_ARRAY_TYPE, _ARB), TYPE_6GLenum, -1, BUGLE_EXTGROUP_vertex_attrib, -1, STATE_VERTEX_ATTRIB },
    { STATE_NAME_EXT(GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, _ARB), TYPE_9GLboolean, -1, BUGLE_EXTGROUP_vertex_attrib, -1, STATE_VERTEX_ATTRIB },
    { STATE_NAME_EXT(GL_VERTEX_ATTRIB_ARRAY_POINTER, _ARB), TYPE_P6GLvoid, -1, BUGLE_EXTGROUP_vertex_attrib, -1, STATE_VERTEX_ATTRIB },
#ifdef GL_ARB_vertex_buffer_object
    { STATE_NAME_EXT(GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, _ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_buffer_object, -1, STATE_VERTEX_ATTRIB },
#endif
    { STATE_NAME_EXT(GL_CURRENT_VERTEX_ATTRIB, _ARB), TYPE_8GLdouble, 4, BUGLE_EXTGROUP_vertex_attrib, -1, STATE_VERTEX_ATTRIB | STATE_SELECT_NON_ZERO },
#endif
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
#ifdef GL_ARB_vertex_buffer_object
    { STATE_NAME_EXT(GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING, _ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_buffer_object, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_COORD },
#endif
#ifdef GL_ARB_point_sprite
    { STATE_NAME_EXT(GL_COORD_REPLACE, _ARB), TYPE_9GLboolean, -1, BUGLE_GL_ARB_point_sprite, -1, STATE_POINT_SPRITE | STATE_SELECT_TEXTURE_COORD },
#endif
    { STATE_NAME(GL_TEXTURE_ENV_MODE), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_TEXTURE_ENV_COLOR), TYPE_8GLdouble, 4, BUGLE_GL_VERSION_1_1, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV  },
#ifdef GL_EXT_texture_lod_bias
    { STATE_NAME_EXT(GL_TEXTURE_LOD_BIAS, _EXT), TYPE_8GLdouble, -1, BUGLE_GL_EXT_texture_lod_bias, -1, STATE_TEXTURE_FILTER_CONTROL | STATE_SELECT_TEXTURE_IMAGE },
#endif
    { STATE_NAME(GL_TEXTURE_GEN_S), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_UNIT_ENABLED | STATE_SELECT_TEXTURE_COORD },
    { STATE_NAME(GL_TEXTURE_GEN_T), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_UNIT_ENABLED | STATE_SELECT_TEXTURE_COORD },
    { STATE_NAME(GL_TEXTURE_GEN_R), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_UNIT_ENABLED | STATE_SELECT_TEXTURE_COORD },
    { STATE_NAME(GL_TEXTURE_GEN_Q), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_UNIT_ENABLED | STATE_SELECT_TEXTURE_COORD },
#ifdef GL_EXT_texture_env_combine
    { STATE_NAME_EXT(GL_COMBINE_RGB, _EXT), TYPE_6GLenum, -1, BUGLE_GL_EXT_texture_env_combine, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME_EXT(GL_COMBINE_ALPHA, _EXT), TYPE_6GLenum, -1, BUGLE_GL_EXT_texture_env_combine, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME_EXT(GL_SOURCE0_RGB, _EXT), TYPE_6GLenum, -1, BUGLE_GL_EXT_texture_env_combine, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME_EXT(GL_SOURCE1_RGB, _EXT), TYPE_6GLenum, -1, BUGLE_GL_EXT_texture_env_combine, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME_EXT(GL_SOURCE2_RGB, _EXT), TYPE_6GLenum, -1, BUGLE_GL_EXT_texture_env_combine, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME_EXT(GL_SOURCE0_ALPHA, _EXT), TYPE_6GLenum, -1, BUGLE_GL_EXT_texture_env_combine, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME_EXT(GL_SOURCE1_ALPHA, _EXT), TYPE_6GLenum, -1, BUGLE_GL_EXT_texture_env_combine, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME_EXT(GL_SOURCE2_ALPHA, _EXT), TYPE_6GLenum, -1, BUGLE_GL_EXT_texture_env_combine, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME_EXT(GL_OPERAND0_RGB, _EXT), TYPE_6GLenum, -1, BUGLE_GL_EXT_texture_env_combine, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME_EXT(GL_OPERAND1_RGB, _EXT), TYPE_6GLenum, -1, BUGLE_GL_EXT_texture_env_combine, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME_EXT(GL_OPERAND2_RGB, _EXT), TYPE_6GLenum, -1, BUGLE_GL_EXT_texture_env_combine, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME_EXT(GL_OPERAND0_ALPHA, _EXT), TYPE_6GLenum, -1, BUGLE_GL_EXT_texture_env_combine, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME_EXT(GL_OPERAND1_ALPHA, _EXT), TYPE_6GLenum, -1, BUGLE_GL_EXT_texture_env_combine, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME_EXT(GL_OPERAND2_ALPHA, _EXT), TYPE_6GLenum, -1, BUGLE_GL_EXT_texture_env_combine, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME_EXT(GL_RGB_SCALE, _EXT), TYPE_8GLdouble, -1, BUGLE_GL_EXT_texture_env_combine, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_ALPHA_SCALE), TYPE_8GLdouble, -1, BUGLE_GL_EXT_texture_env_combine, -1, STATE_TEXTURE_ENV | STATE_SELECT_TEXTURE_ENV },
#endif
    { STATE_NAME(GL_CURRENT_TEXTURE_COORDS), TYPE_8GLdouble, 4, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_COORD },
    { STATE_NAME(GL_CURRENT_RASTER_TEXTURE_COORDS), TYPE_8GLdouble, 4, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_COORD },
    { STATE_NAME(GL_TEXTURE_MATRIX), TYPE_8GLdouble, 16, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_COORD },
    { STATE_NAME(GL_TEXTURE_STACK_DEPTH), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_COORD },
    { STATE_NAME(GL_TEXTURE_1D), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_UNIT_ENABLED | STATE_SELECT_TEXTURE_ENV },
    { STATE_NAME(GL_TEXTURE_2D), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_UNIT_ENABLED | STATE_SELECT_TEXTURE_ENV },
#ifdef GL_EXT_texture3D
    { STATE_NAME_EXT(GL_TEXTURE_3D, _EXT), TYPE_9GLboolean, -1, BUGLE_GL_EXT_texture3D, -1, STATE_TEX_UNIT_ENABLED | STATE_SELECT_TEXTURE_ENV },
#endif
#ifdef GL_ARB_texture_cube_map
    { STATE_NAME_EXT(GL_TEXTURE_CUBE_MAP, _ARB), TYPE_9GLboolean, -1, BUGLE_GL_ARB_texture_cube_map, -1, STATE_TEX_UNIT_ENABLED | STATE_SELECT_TEXTURE_ENV },
#endif
#ifdef GL_NV_texture_rectangle
    { "GL_TEXTURE_RECTANGLE_ARB", GL_TEXTURE_RECTANGLE_NV, TYPE_9GLboolean, -1, BUGLE_GL_NV_texture_rectangle, -1, STATE_TEX_UNIT_ENABLED | STATE_SELECT_TEXTURE_ENV },
#endif
    { STATE_NAME(GL_TEXTURE_BINDING_1D), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_IMAGE },
    { STATE_NAME(GL_TEXTURE_BINDING_2D), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_IMAGE },
#ifdef GL_VERSION_1_2
    { STATE_NAME(GL_TEXTURE_BINDING_3D), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_2, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_IMAGE }, /* Note: GL_EXT_texture3D doesn't define this! */
#endif
#ifdef GL_ARB_texture_cube_map
    { STATE_NAME_EXT(GL_TEXTURE_BINDING_CUBE_MAP, _ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_texture_cube_map, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_IMAGE },
#endif
#ifdef GL_NV_texture_rectangle
    { "GL_TEXTURE_BINDING_RECTANGLE_ARB", GL_TEXTURE_BINDING_RECTANGLE_NV, TYPE_5GLint, -1, BUGLE_GL_NV_texture_rectangle, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_IMAGE },
#endif
#ifdef GL_EXT_texture_array
    { STATE_NAME(GL_TEXTURE_BINDING_1D_ARRAY_EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_texture_array, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_IMAGE },
    { STATE_NAME(GL_TEXTURE_BINDING_2D_ARRAY_EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_texture_array, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_IMAGE },
#endif
#ifdef GL_EXT_texture_buffer_object
    { STATE_NAME(GL_TEXTURE_BINDING_BUFFER_EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_texture_buffer_object, -1, STATE_TEX_UNIT | STATE_SELECT_TEXTURE_IMAGE },
#endif
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
#ifdef GL_EXT_texture3D
    { STATE_NAME_EXT(GL_TEXTURE_DEPTH, _EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_texture3D, -1, STATE_TEX_LEVEL_PARAMETER | STATE_SELECT_NO_1D | STATE_SELECT_NO_2D },
#endif
    { STATE_NAME(GL_TEXTURE_BORDER), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_LEVEL_PARAMETER },
    { STATE_NAME(GL_TEXTURE_INTERNAL_FORMAT), TYPE_16GLcomponentsenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_LEVEL_PARAMETER },
    { STATE_NAME(GL_TEXTURE_RED_SIZE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_LEVEL_PARAMETER },
    { STATE_NAME(GL_TEXTURE_GREEN_SIZE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_LEVEL_PARAMETER },
    { STATE_NAME(GL_TEXTURE_BLUE_SIZE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_LEVEL_PARAMETER },
    { STATE_NAME(GL_TEXTURE_ALPHA_SIZE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_LEVEL_PARAMETER },
    { STATE_NAME(GL_TEXTURE_LUMINANCE_SIZE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_LEVEL_PARAMETER },
    { STATE_NAME(GL_TEXTURE_INTENSITY_SIZE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_LEVEL_PARAMETER },
#ifdef GL_ARB_depth_texture
    { STATE_NAME_EXT(GL_TEXTURE_DEPTH_SIZE, _ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_depth_texture, -1, STATE_TEX_LEVEL_PARAMETER },
#endif
#ifdef GL_EXT_palette_texture
    { STATE_NAME(GL_TEXTURE_INDEX_SIZE_EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_palette_texture, -1, STATE_TEX_LEVEL_PARAMETER },
#endif
#ifdef GL_EXT_texture_shared_exponent
    { STATE_NAME(GL_TEXTURE_SHARED_SIZE_EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_texture_shared_exponent, -1, STATE_TEX_LEVEL_PARAMETER },
#endif
#ifdef GL_ARB_texture_compression
    { STATE_NAME_EXT(GL_TEXTURE_COMPRESSED, _ARB), TYPE_9GLboolean, -1, BUGLE_GL_ARB_texture_compression, -1, STATE_TEX_LEVEL_PARAMETER },
#ifdef GL_TEXTURE_COMPRESSED_IMAGE_SIZE_ARB /* Not defined in glext.h version 21??? */
    { STATE_NAME_EXT(GL_TEXTURE_COMPRESSED_IMAGE_SIZE, _ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_texture_compression, -1, STATE_TEX_LEVEL_PARAMETER | STATE_SELECT_NO_PROXY | STATE_SELECT_COMPRESSED },
#endif
#endif /* GL_ARB_texture_compression */
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info tex_parameter_state[] =
{
    { STATE_NAME(GL_TEXTURE_BORDER_COLOR), TYPE_8GLdouble, 4, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_PARAMETER },
    { STATE_NAME(GL_TEXTURE_MIN_FILTER), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_PARAMETER },
    { STATE_NAME(GL_TEXTURE_MAG_FILTER), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_PARAMETER },
    { STATE_NAME(GL_TEXTURE_WRAP_S), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_PARAMETER },
    { STATE_NAME(GL_TEXTURE_WRAP_T), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_PARAMETER | STATE_SELECT_NO_1D },
#ifdef GL_EXT_texture3D
    { STATE_NAME_EXT(GL_TEXTURE_WRAP_R, _EXT), TYPE_6GLenum, -1, BUGLE_GL_EXT_texture3D, -1, STATE_TEX_PARAMETER | STATE_SELECT_NO_1D | STATE_SELECT_NO_2D },
#endif
    { STATE_NAME(GL_TEXTURE_PRIORITY), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_PARAMETER },
    { STATE_NAME(GL_TEXTURE_RESIDENT), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_TEX_PARAMETER },
#ifdef GL_SGIS_texture_lod
    { STATE_NAME_EXT(GL_TEXTURE_MIN_LOD, _SGIS), TYPE_8GLdouble, -1, BUGLE_GL_SGIS_texture_lod, -1, STATE_TEX_PARAMETER },
    { STATE_NAME_EXT(GL_TEXTURE_MAX_LOD, _SGIS), TYPE_8GLdouble, -1, BUGLE_GL_SGIS_texture_lod, -1, STATE_TEX_PARAMETER },
    { STATE_NAME_EXT(GL_TEXTURE_BASE_LEVEL, _SGIS), TYPE_5GLint, -1, BUGLE_GL_SGIS_texture_lod, -1, STATE_TEX_PARAMETER },
    { STATE_NAME_EXT(GL_TEXTURE_MAX_LEVEL, _SGIS), TYPE_5GLint, -1, BUGLE_GL_SGIS_texture_lod, -1, STATE_TEX_PARAMETER },
#endif
#ifdef GL_EXT_texture_lod_bias
    { STATE_NAME_EXT(GL_TEXTURE_LOD_BIAS, _EXT), TYPE_8GLdouble, -1, BUGLE_GL_EXT_texture_lod_bias, -1, STATE_TEX_PARAMETER },
#endif
#ifdef GL_ARB_depth_texture
    { STATE_NAME_EXT(GL_DEPTH_TEXTURE_MODE, _ARB), TYPE_6GLenum, -1, BUGLE_GL_ARB_depth_texture, -1, STATE_TEX_PARAMETER },
#endif
#ifdef GL_ARB_shadow
    { STATE_NAME_EXT(GL_TEXTURE_COMPARE_MODE, _ARB), TYPE_6GLenum, -1, BUGLE_GL_ARB_shadow, -1, STATE_TEX_PARAMETER },
    { STATE_NAME_EXT(GL_TEXTURE_COMPARE_FUNC, _ARB), TYPE_6GLenum, -1, BUGLE_GL_ARB_shadow, -1, STATE_TEX_PARAMETER },
#endif
#ifdef GL_SGIS_generate_mipmap
    { STATE_NAME_EXT(GL_GENERATE_MIPMAP, _SGIS), TYPE_9GLboolean, -1, BUGLE_GL_SGIS_generate_mipmap, -1, STATE_TEX_PARAMETER },
#endif

    /* Non-GL2.1 state */
#ifdef GL_EXT_texture_filter_anisotropic
    { STATE_NAME(GL_TEXTURE_MAX_ANISOTROPY_EXT), TYPE_8GLdouble, -1, BUGLE_GL_EXT_texture_filter_anisotropic, -1, STATE_TEX_PARAMETER },
#endif
#ifdef GL_NV_texture_expand_normal
    /* Disabled for now because it causes errors.
    { STATE_NAME_EXT(GL_TEXTURE_UNSIGNED_REMAP_MODE, _NV), TYPE_6GLenum, -1, BUGLE_GL_NV_texture_expand_normal, -1, STATE_TEX_PARAMETER },
    */
#endif
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

static const state_info program_state[] =
{
#ifdef GL_ARB_shader_objects
    { "GL_DELETE_STATUS", GL_OBJECT_DELETE_STATUS_ARB, TYPE_9GLboolean, -1, BUGLE_GL_ARB_shader_objects, -1, STATE_PROGRAM },
    { "GL_INFO_LOG_LENGTH", GL_OBJECT_INFO_LOG_LENGTH_ARB, TYPE_5GLint, -1, BUGLE_GL_ARB_shader_objects, -1, STATE_PROGRAM },
    { "InfoLog", GL_NONE, TYPE_P9GLcharARB, -1, BUGLE_GL_ARB_shader_objects, -1, STATE_PROGRAM_INFO_LOG },
    { "GL_ATTACHED_SHADERS", GL_OBJECT_ATTACHED_OBJECTS_ARB, TYPE_5GLint, -1, BUGLE_GL_ARB_shader_objects, -1, STATE_PROGRAM },
    { "GL_ACTIVE_UNIFORMS", GL_OBJECT_ACTIVE_UNIFORMS_ARB, TYPE_5GLint, -1, BUGLE_GL_ARB_shader_objects, -1, STATE_PROGRAM },
    { "GL_LINK_STATUS", GL_OBJECT_LINK_STATUS_ARB, TYPE_9GLboolean, -1, BUGLE_GL_ARB_shader_objects, -1, STATE_PROGRAM },
    { "GL_VALIDATE_STATUS", GL_OBJECT_VALIDATE_STATUS_ARB, TYPE_9GLboolean, -1, BUGLE_GL_ARB_shader_objects, -1, STATE_PROGRAM },
    { "GL_ACTIVE_UNIFORM_MAX_LENGTH", GL_OBJECT_ACTIVE_UNIFORM_MAX_LENGTH_ARB, TYPE_5GLint, -1, BUGLE_GL_ARB_shader_objects, -1, STATE_PROGRAM },
    { "Attached", GL_NONE, TYPE_11GLhandleARB, 0, BUGLE_GL_ARB_shader_objects, -1, STATE_ATTACHED_SHADERS },
#endif
#ifdef GL_ARB_vertex_shader
    { "GL_ACTIVE_ATTRIBUTES", GL_OBJECT_ACTIVE_ATTRIBUTES_ARB, TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_shader, -1, STATE_PROGRAM },
    { "GL_ACTIVE_ATTRIBUTE_MAX_LENGTH", GL_OBJECT_ACTIVE_ATTRIBUTE_MAX_LENGTH_ARB, TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_shader, -1, STATE_PROGRAM },
#endif
#ifdef GL_EXT_geometry_shader4
    { STATE_NAME(GL_GEOMETRY_VERTICES_OUT_EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_geometry_shader4, -1, STATE_PROGRAM },
    { STATE_NAME(GL_GEOMETRY_INPUT_TYPE_EXT), TYPE_6GLenum, -1, BUGLE_GL_EXT_geometry_shader4, -1, STATE_PROGRAM },
    { STATE_NAME(GL_GEOMETRY_OUTPUT_TYPE_EXT), TYPE_6GLenum, -1, BUGLE_GL_EXT_geometry_shader4, -1, STATE_PROGRAM },
#endif
#ifdef GL_NV_transform_feedback
    { STATE_NAME(GL_ACTIVE_VARYINGS_NV), TYPE_5GLint, -1, BUGLE_GL_NV_transform_feedback, -1, STATE_PROGRAM },
    { STATE_NAME(GL_ACTIVE_VARYING_MAX_LENGTH_NV), TYPE_5GLint, -1, BUGLE_GL_NV_transform_feedback, -1, STATE_PROGRAM },
    { STATE_NAME(GL_TRANSFORM_FEEDBACK_BUFFER_MODE_NV), TYPE_6GLenum, -1, BUGLE_GL_NV_transform_feedback, -1, STATE_PROGRAM },
    { STATE_NAME(GL_TRANSFORM_FEEDBACK_VARYINGS_NV), TYPE_5GLint, -1, BUGLE_GL_NV_transform_feedback, -1, STATE_PROGRAM },
#endif
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info shader_state[] =
{
#ifdef GL_ARB_shader_objects
    { "GL_DELETE_STATUS", GL_OBJECT_DELETE_STATUS_ARB, TYPE_9GLboolean, -1, BUGLE_GL_ARB_shader_objects, -1, STATE_SHADER },
    { "GL_INFO_LOG_LENGTH", GL_OBJECT_INFO_LOG_LENGTH_ARB, TYPE_5GLint, -1, BUGLE_GL_ARB_shader_objects, -1, STATE_SHADER },
    { "InfoLog", GL_NONE, TYPE_P9GLcharARB, -1, BUGLE_GL_ARB_shader_objects, -1, STATE_SHADER_INFO_LOG },
    { "ShaderSource", GL_NONE, TYPE_P9GLcharARB, -1, BUGLE_GL_ARB_shader_objects, -1, STATE_SHADER_SOURCE },
    { "GL_SHADER_TYPE", GL_OBJECT_SUBTYPE_ARB, TYPE_6GLenum, -1, BUGLE_GL_ARB_shader_objects, -1, STATE_SHADER },
    { "GL_COMPILE_STATUS", GL_OBJECT_COMPILE_STATUS_ARB, TYPE_9GLboolean, -1, BUGLE_GL_ARB_shader_objects, -1, STATE_SHADER },
    { "GL_SHADER_SOURCE_LENGTH", GL_OBJECT_SHADER_SOURCE_LENGTH_ARB, TYPE_5GLint, -1, BUGLE_GL_ARB_shader_objects, -1, STATE_SHADER },
#endif /* GL_ARB_shader_objects */
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info generic_enable = { NULL, GL_NONE, TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED };

/* All the make functions _append_ nodes to children. That means you have
 * to initialise the list yourself.
 */
static void make_leaves_conditional(const glstate *self, const state_info *table,
                                    uint32_t flags, unsigned int mask,
                                    linked_list *children)
{
    const char *version;
    glstate *child;
    const state_info *info;

    version = (const char *) CALL(glGetString)(GL_VERSION);
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
                child = XMALLOC(glstate);
                *child = *self; /* copies contextual info */
                child->name = xstrdup(info->name);
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
            child = XMALLOC(glstate);
            *child = *self;
            child->info = info;
            child->name = xstrdup(enums[i].name);
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
        child = XMALLOC(glstate);
        *child = *self;
        child->info = info;
        child->name = xasprintf(format, (unsigned long) i);
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

    child = XMALLOC(glstate);
    *child = *self;
    child->target = target;
    child->info = info;
    child->name = xasprintf(format, (unsigned long) id);
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
                         bool add_zero,
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

    child = XMALLOC(glstate);
    *child = *self;
    child->name = xstrdup(name);
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

static void spawn_children_vertex_attrib(const glstate *self, linked_list *children)
{
    bugle_list_init(children, free);
    make_leaves_conditional(self, vertex_attrib_state,
                            0,
                            (self->object == 0) ? STATE_SELECT_NON_ZERO : 0,
                            children);
}

static void spawn_vertex_attrib_arrays(const struct glstate *self,
                                       linked_list *children,
                                       const struct state_info *info)
{
#ifdef GL_ARB_vertex_program
    GLint count, i;
    CALL(glGetIntegerv)(GL_MAX_VERTEX_ATTRIBS_ARB, &count);
    for (i = 0; i < count; i++)
        make_object(self, 0, "VertexAttrib[%lu]", i, spawn_children_vertex_attrib, NULL, children);
#endif
}

static void spawn_clip_planes(const struct glstate *self,
                              linked_list *children,
                              const struct state_info *info)
{
    GLint count;
    CALL(glGetIntegerv)(GL_MAX_CLIP_PLANES, &count);
    make_counted2(self, count, "GL_CLIP_PLANE%lu", GL_CLIP_PLANE0,
                  offsetof(glstate, target), offsetof(glstate, enum_name),
                  NULL, info, children);
}

static void spawn_children_material(const glstate *self, linked_list *children)
{
    bugle_list_init(children, free);
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
    bugle_list_init(children, free);
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

#ifdef GL_ARB_multitexture
    if (bugle_gl_has_extension_group(BUGLE_GL_ARB_multitexture))
    {
        CALL(glGetIntegerv)(GL_MAX_TEXTURE_UNITS_ARB, &cur);
        if (cur > max) max = cur;
    }
#endif
#ifdef GL_ARB_fragment_program
    if (bugle_gl_has_extension_group(BUGLE_EXTGROUP_texunits))
    {
        CALL(glGetIntegerv)(GL_MAX_TEXTURE_IMAGE_UNITS_ARB, &cur);
        if (cur > max) max = cur;
        CALL(glGetIntegerv)(GL_MAX_TEXTURE_COORDS_ARB, &cur);
        if (cur > max) max = cur;
    }
#endif
#ifdef GL_ARB_vertex_shader
    if (bugle_gl_has_extension_group(BUGLE_GL_ARB_vertex_shader))
    {
        CALL(glGetIntegerv)(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS_ARB, &cur);
        if (cur > max) max = cur;
    }
#endif
    /* NVIDIA 66.29 on NV20 fails on some of these calls. Clear the error. */
    CALL(glGetError)();
    return max;
}

/* Returns a mask of flags not to select from state tables, based on the
 * dimension of the target.
 */
static uint32_t texture_mask(GLenum target)
{
    uint32_t mask = 0;

    switch (target)
    {
    case GL_PROXY_TEXTURE_1D: mask |= STATE_SELECT_NO_PROXY; /* Fall through */
    case GL_TEXTURE_1D: mask |= STATE_SELECT_NO_1D; break;
    case GL_PROXY_TEXTURE_2D: mask |= STATE_SELECT_NO_PROXY; /* Fall through */
    case GL_TEXTURE_2D: mask |= STATE_SELECT_NO_2D; break;
#ifdef GL_ARB_texture_cube_map
    case GL_PROXY_TEXTURE_CUBE_MAP_ARB: mask |= STATE_SELECT_NO_PROXY; /* Fall through */
    case GL_TEXTURE_CUBE_MAP_ARB: mask |= STATE_SELECT_NO_2D; break;
#endif
#ifdef GL_NV_texture_rectangle
    case GL_PROXY_TEXTURE_RECTANGLE_NV: mask |= STATE_SELECT_NO_PROXY; /* Fall through */
    case GL_TEXTURE_RECTANGLE_NV: mask |= STATE_SELECT_NO_2D; break;
#endif
#ifdef GL_EXT_texture3D
    case GL_PROXY_TEXTURE_3D_EXT: mask |= STATE_SELECT_NO_PROXY; break;
#endif
#ifdef GL_EXT_texture_array
    case GL_PROXY_TEXTURE_1D_ARRAY_EXT: mask |= STATE_SELECT_NO_PROXY; /* Fall through */
    case GL_TEXTURE_1D_ARRAY_EXT: mask |= STATE_SELECT_NO_2D; break;
    case GL_PROXY_TEXTURE_2D_ARRAY_EXT: mask |= STATE_SELECT_NO_PROXY; break;
#endif
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

#ifdef GL_ARB_multitexture
    if (bugle_gl_has_extension_group(BUGLE_GL_ARB_multitexture))
        CALL(glGetIntegerv)(GL_MAX_TEXTURE_UNITS_ARB, &ans);
#endif
    return ans;
}

static unsigned int get_texture_image_units(void)
{
    GLint cur = 0, max = 1;

#ifdef GL_ARB_multitexture
    if (bugle_gl_has_extension_group(BUGLE_GL_ARB_multitexture))
    {
        CALL(glGetIntegerv)(GL_MAX_TEXTURE_UNITS_ARB, &cur);
        if (cur > max) max = cur;
    }
#endif
#ifdef GL_ARB_fragment_program
    if (bugle_gl_has_extension_group(BUGLE_EXTGROUP_texunits))
    {
        CALL(glGetIntegerv)(GL_MAX_TEXTURE_IMAGE_UNITS_ARB, &cur);
        if (cur > max) max = cur;
    }
#endif
#ifdef GL_ARB_vertex_shader
    if (bugle_gl_has_extension_group(BUGLE_GL_ARB_vertex_shader))
    {
        CALL(glGetIntegerv)(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS_ARB, &cur);
        if (cur > max) max = cur;
    }
#endif
    /* NVIDIA 66.29 on NV20 fails on some of these calls. Clear the error. */
    CALL(glGetError)();
    return max;
}

static unsigned int get_texture_coord_units(void)
{
    GLint cur = 0, max = 1;

#ifdef GL_ARB_multitexture
    if (bugle_gl_has_extension_group(BUGLE_GL_ARB_multitexture))
    {
        CALL(glGetIntegerv)(GL_MAX_TEXTURE_UNITS_ARB, &cur);
        if (cur > max) max = cur;
    }
#endif
#ifdef GL_ARB_fragment_program
    if (bugle_gl_has_extension_group(BUGLE_EXTGROUP_texunits))
    {
        CALL(glGetIntegerv)(GL_MAX_TEXTURE_COORDS_ARB, &cur);
        if (cur > max) max = cur;
    }
#endif
    /* NVIDIA 66.29 on NV20 fails on some of these calls. Clear the error. */
    CALL(glGetError)();
    return max;
}

static void spawn_children_tex_level_parameter(const glstate *self, linked_list *children)
{
    uint32_t mask = STATE_SELECT_COMPRESSED;

#ifdef GL_ARB_texture_compression
    if (!(mask & STATE_SELECT_NO_PROXY)
        && bugle_gl_has_extension_group(BUGLE_GL_ARB_texture_compression)
        && bugle_begin_internal_render())
    {
        GLint old, compressed;

        if (self->binding)
        {
            CALL(glGetIntegerv)(self->binding, &old);
            CALL(glBindTexture)(self->target, self->object);
        }
        CALL(glGetTexLevelParameteriv)(self->face, self->level, GL_TEXTURE_COMPRESSED_ARB, &compressed);
        if (compressed) mask &= ~STATE_SELECT_COMPRESSED;
        if (self->binding) CALL(glBindTexture)(self->target, old);
        bugle_end_internal_render("spawn_children_tex_level_parameter", true);
    }
#endif
    bugle_list_init(children, free);
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
#ifdef GL_SGIS_texture_lod
    if (self->binding && bugle_gl_has_extension_group(GL_SGIS_texture_lod)) /* No parameters for proxy textures */
    {
        CALL(glGetTexParameteriv)(self->target, GL_TEXTURE_BASE_LEVEL_SGIS, &base);
        CALL(glGetTexParameteriv)(self->target, GL_TEXTURE_MAX_LEVEL_SGIS, &max);
    }
#endif

    for (i = base; i <= base + max; i++)
    {
        GLint width;
        CALL(glGetTexLevelParameteriv)(self->face, i, GL_TEXTURE_WIDTH, &width);
        if (width <= 0) break;

        child = XMALLOC(glstate);
        *child = *self;
        child->name = xasprintf("level[%lu]", (unsigned long) i);
        child->numeric_name = i;
        child->enum_name = 0;
        child->info = NULL;
        child->level = i;
        child->spawn_children = spawn_children_tex_level_parameter;
        bugle_list_append(children, child);
    }

    if (self->binding) CALL(glBindTexture)(self->target, old);
}

#ifdef GL_ARB_texture_cube_map
static void spawn_children_cube_map_faces(const glstate *self, linked_list *children)
{
    bugle_list_init(children, free);
    make_tex_levels(self, children);
}
#endif

static void spawn_children_tex_parameter(const glstate *self, linked_list *children)
{
    bugle_list_init(children, free);
    if (self->binding) /* Zero indicates a proxy, which have no texture parameter state */
        make_leaves_conditional(self, tex_parameter_state, 0,
                                texture_mask(self->target), children);
#ifdef GL_ARB_texture_cube_map
    if (self->target == GL_TEXTURE_CUBE_MAP_ARB)
    {
        make_fixed(self, cube_map_face_enums, offsetof(glstate, face),
                   spawn_children_cube_map_faces, NULL, children);
        return;
    }
#endif
    make_tex_levels(self, children);
}

static void spawn_children_tex_target(const glstate *self, linked_list *children)
{
    if (self->binding) /* Zero here indicates a proxy target */
    {
        bugle_list_init(children, free);
        make_objects(self, BUGLE_GLOBJECTS_TEXTURE, self->target, true, "%lu",
                     spawn_children_tex_parameter, NULL, children);
    }
    else
        spawn_children_tex_parameter(self, children);
}

static void spawn_children_tex_buffer(const glstate *self, linked_list *children)
{
    bugle_list_init(children, free);
    make_leaves(self, tex_buffer_state, children);
}

static void spawn_children_tex_buffer_target(const glstate *self, linked_list *children)
{
    bugle_list_init(children, free);
    make_objects(self, BUGLE_GLOBJECTS_TEXTURE, self->target, true, "%lu",
                 spawn_children_tex_buffer, NULL, children);
}

static void spawn_children_tex_gen(const glstate *self, linked_list *children)
{
    bugle_list_init(children, free);
    make_leaves(self, tex_gen_state, children);
}

static void spawn_children_tex_unit(const glstate *self, linked_list *children)
{
    linked_list_node *i;
    glstate *child;
    uint32_t mask = 0;

    bugle_list_init(children, free);
#ifdef GL_ARB_multitexture
    if (self->unit >= GL_TEXTURE0_ARB + get_texture_env_units())
        mask |= STATE_SELECT_TEXTURE_ENV;
    if (self->unit >= GL_TEXTURE0_ARB + get_texture_coord_units())
        mask |= STATE_SELECT_TEXTURE_COORD;
    if (self->unit >= GL_TEXTURE0_ARB + get_texture_image_units())
        mask |= STATE_SELECT_TEXTURE_IMAGE;
#endif
    make_leaves_conditional(self, tex_unit_state, 0, mask, children);
    for (i = bugle_list_head(children); i; i = bugle_list_next(i))
    {
        child = (glstate *) bugle_list_data(i);
        switch (child->info->flags & STATE_MODE_MASK)
        {
        case STATE_MODE_TEXTURE_ENV:
            child->target = GL_TEXTURE_ENV;
            break;
#ifdef GL_EXT_texture_lod_bias
        case STATE_MODE_TEXTURE_FILTER_CONTROL:
            child->target = GL_TEXTURE_FILTER_CONTROL;
            break;
#endif
#ifdef GL_ARB_point_sprite
        case STATE_MODE_POINT_SPRITE:
            child->target = GL_POINT_SPRITE_ARB;
            break;
#endif
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
#ifdef GL_ARB_multitexture
    if (bugle_gl_has_extension_group(BUGLE_GL_ARB_multitexture))
    {
        GLint count;
        count = get_total_texture_units();
        make_counted2(self, count, "GL_TEXTURE%lu", GL_TEXTURE0_ARB,
                      offsetof(glstate, unit), offsetof(glstate, enum_name),
                      spawn_children_tex_unit,
                      NULL, children);
    }
    else
#endif
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
#ifdef GL_VERSION_1_2
    if (bugle_gl_has_extension_group(BUGLE_GL_VERSION_1_2))
    {
        make_target(self, "GL_TEXTURE_3D", GL_TEXTURE_3D,
                        GL_TEXTURE_BINDING_3D, spawn_children_tex_target, NULL, children);
        make_target(self, "GL_PROXY_TEXTURE_3D", GL_PROXY_TEXTURE_3D,
                        0, spawn_children_tex_target, NULL, children);
    }
#endif
#ifdef GL_ARB_texture_cube_map
    if (bugle_gl_has_extension_group(BUGLE_GL_ARB_texture_cube_map))
    {
        make_target(self, "GL_TEXTURE_CUBE_MAP",
                    GL_TEXTURE_CUBE_MAP_ARB,
                    GL_TEXTURE_BINDING_CUBE_MAP_ARB,
                    spawn_children_tex_target, NULL, children);
        make_target(self, "GL_PROXY_TEXTURE_CUBE_MAP",
                    GL_PROXY_TEXTURE_CUBE_MAP_ARB,
                    0,
                    spawn_children_tex_target, NULL, children);
    }
#endif
#ifdef GL_NV_texture_rectangle
    if (bugle_gl_has_extension_group(BUGLE_GL_NV_texture_rectangle))
    {
        make_target(self, "GL_TEXTURE_RECTANGLE_ARB",
                    GL_TEXTURE_RECTANGLE_NV,
                    GL_TEXTURE_BINDING_RECTANGLE_NV,
                    spawn_children_tex_target, NULL, children);
        make_target(self, "GL_PROXY_TEXTURE_RECTANGLE_ARB",
                    GL_PROXY_TEXTURE_RECTANGLE_NV,
                    0,
                    spawn_children_tex_target, NULL, children);
    }
#endif
#ifdef GL_EXT_texture_array
    if (bugle_gl_has_extension_group(BUGLE_GL_EXT_texture_array))
    {
        make_target(self, "GL_TEXTURE_1D_ARRAY_EXT",
                    GL_TEXTURE_1D_ARRAY_EXT,
                    GL_TEXTURE_BINDING_1D_ARRAY_EXT,
                    spawn_children_tex_target, NULL, children);
        make_target(self, "GL_PROXY_TEXTURE_1D_ARRAY_EXT",
                    GL_PROXY_TEXTURE_1D_ARRAY_EXT,
                    0,
                    spawn_children_tex_target, NULL, children);
        make_target(self, "GL_TEXTURE_2D_ARRAY_EXT",
                    GL_TEXTURE_2D_ARRAY_EXT,
                    GL_TEXTURE_BINDING_2D_ARRAY_EXT,
                    spawn_children_tex_target, NULL, children);
        make_target(self, "GL_PROXY_TEXTURE_2D_ARRAY_EXT",
                    GL_PROXY_TEXTURE_2D_ARRAY_EXT,
                    0,
                    spawn_children_tex_target, NULL, children);
    }
#endif
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
    bugle_list_init(children, free);
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
#ifdef GL_EXT_draw_buffers2
    static const state_info color_writemask = { STATE_NAME(GL_COLOR_WRITEMASK), TYPE_9GLboolean, 4, BUGLE_GL_EXT_draw_buffers2, -1, STATE_INDEXED };

    GLint count;
    CALL(glGetIntegerv)(GL_MAX_DRAW_BUFFERS_ATI, &count);
    bugle_list_init(children, free);
    make_counted(self, count, "%lu", 0, offsetof(glstate, numeric_name), NULL,
                 &color_writemask, children);
#endif
}

static void spawn_color_writemask(const struct glstate *self,
                                  linked_list *children,
                                  const struct state_info *info)
{
    make_target(self, "GL_COLOR_WRITEMASK", GL_COLOR_WRITEMASK, GL_COLOR_WRITEMASK,
                spawn_children_color_writemask, NULL, children);
}

static void spawn_draw_buffers(const struct glstate *self,
                               linked_list *children,
                               const struct state_info *info)
{
#ifdef GL_ATI_draw_buffers
    GLint count;
    CALL(glGetIntegerv)(GL_MAX_DRAW_BUFFERS_ATI, &count);
    make_counted(self, count, "GL_DRAW_BUFFER%lu", GL_DRAW_BUFFER0_ATI,
                 offsetof(glstate, enum_name), NULL,
                 info, children);
#endif
}

static void spawn_children_color_table_parameter(const glstate *self, linked_list *children)
{
    bugle_list_init(children, free);
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
    bugle_list_init(children, free);
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
    bugle_list_init(children, free);
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
    bugle_list_init(children, free);
    make_leaves(self, minmax_parameter_state, children);
}

static void spawn_minmax(const struct glstate *self,
                         linked_list *children,
                         const struct state_info *info)
{
    make_fixed(self, minmax_parameter_enums, offsetof(glstate, target),
               spawn_children_minmax_parameter, &generic_enable, children);
}

#ifdef GL_ARB_shader_objects
static void spawn_children_shader(const glstate *self, linked_list *children)
{
    bugle_list_init(children, free);
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

    bugle_list_init(children, free);
    make_leaves(self, program_state, children);

    bugle_glGetProgramiv(self->object, GL_OBJECT_ACTIVE_UNIFORMS_ARB, &count);
    bugle_glGetProgramiv(self->object, GL_OBJECT_ACTIVE_UNIFORM_MAX_LENGTH_ARB, &max);
    /* If the program failed to link, then uniforms cannot be queried */
    bugle_glGetProgramiv(self->object, GL_OBJECT_LINK_STATUS_ARB, &status);
    if (status != GL_FALSE)
        for (i = 0; i < count; i++)
        {
            child = XMALLOC(glstate);
            *child = *self;
            child->spawn_children = NULL;
            child->info = &uniform_state;
            child->name = XNMALLOC(max, GLcharARB);
            child->name[0] = '\0'; /* sanity, in case the query borks */
            child->numeric_name = i;
            child->enum_name = 0;
            child->level = i;
            bugle_glGetActiveUniform(self->object, i, max, &length, &size,
                                     &type, child->name);
            if (length)
            {
                /* Check for built-in state, which is returned by glGetActiveUniformARB
                 * but cannot be queried.
                 */
                child->level = bugle_glGetUniformLocation(self->object, child->name);
                if (child->level == -1) length = 0;
            }
            if (length) bugle_list_append(children, child);
            else free(child->name); /* failed query */
        }

#ifdef GL_ARB_vertex_shader
    if (status != GL_FALSE && bugle_gl_has_extension_group(BUGLE_GL_ARB_vertex_shader))
    {
        bugle_glGetProgramiv(self->object, GL_OBJECT_ACTIVE_ATTRIBUTES_ARB,
                             &count);
        bugle_glGetProgramiv(self->object, GL_OBJECT_ACTIVE_ATTRIBUTE_MAX_LENGTH_ARB,
                             &max);

        for (i = 0; i < count; i++)
        {
            child = XMALLOC(glstate);
            *child = *self;
            child->spawn_children = NULL;
            child->info = &attrib_state;
            child->name = XNMALLOC(max + 1, GLcharARB);
            child->name[0] = '\0';
            child->numeric_name = i;
            child->enum_name = 0;
            child->level = i;
            bugle_glGetActiveAttrib(self->object, i, max, &length, &size,
                                    &type, child->name);
            if (length) bugle_list_append(children, child);
            else free(child->name);
        }
    }
#endif
}
#endif /* GL_ARB_shader_objects */

static void spawn_shaders(const struct glstate *self,
                          linked_list *children,
                          const struct state_info *info)
{
#ifdef GL_ARB_shader_objects
    make_objects(self, BUGLE_GLOBJECTS_SHADER, GL_NONE, false,
                 "Shader[%lu]", spawn_children_shader, NULL, children);
#endif
}

static void spawn_programs(const struct glstate *self,
                           linked_list *children,
                           const struct state_info *info)
{
#ifdef GL_ARB_shader_objects
    make_objects(self, BUGLE_GLOBJECTS_PROGRAM, GL_NONE, false,
                 "Program[%lu]", spawn_children_program, NULL, children);
#endif
}

/*** Main state table ***/

static const state_info global_state[] =
{
    { STATE_NAME(GL_CURRENT_COLOR), TYPE_8GLdouble, 4, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
#ifdef GL_EXT_secondary_color
    { STATE_NAME_EXT(GL_CURRENT_SECONDARY_COLOR, _EXT), TYPE_8GLdouble, 4, BUGLE_GL_EXT_secondary_color, -1, STATE_GLOBAL },
#endif
    { STATE_NAME(GL_CURRENT_INDEX), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_CURRENT_NORMAL), TYPE_8GLdouble, 3, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
#ifdef GL_EXT_fog_coord
    { "GL_CURRENT_FOG_COORD", GL_CURRENT_FOG_COORDINATE_EXT, TYPE_8GLdouble, -1, BUGLE_GL_EXT_fog_coord, -1, STATE_GLOBAL },
#endif
    { STATE_NAME(GL_CURRENT_RASTER_POSITION), TYPE_8GLdouble, 4, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_CURRENT_RASTER_DISTANCE), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_CURRENT_RASTER_COLOR), TYPE_8GLdouble, 4, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
#ifdef GL_VERSION_2_1
    { STATE_NAME(GL_CURRENT_RASTER_SECONDARY_COLOR), TYPE_8GLdouble, 4, BUGLE_GL_VERSION_2_1, -1, STATE_GLOBAL },
#endif
    { STATE_NAME(GL_CURRENT_RASTER_INDEX), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_CURRENT_RASTER_POSITION_VALID), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_EDGE_FLAG), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
#ifdef GL_ARB_multitexture
    { STATE_NAME_EXT(GL_CLIENT_ACTIVE_TEXTURE, _ARB), TYPE_6GLenum, -1, BUGLE_GL_ARB_multitexture, -1, STATE_GLOBAL },
#endif
    { STATE_NAME(GL_VERTEX_ARRAY), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_VERTEX_ARRAY_SIZE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_VERTEX_ARRAY_TYPE), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_VERTEX_ARRAY_STRIDE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_VERTEX_ARRAY_POINTER), TYPE_P6GLvoid, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_NORMAL_ARRAY), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_NORMAL_ARRAY_TYPE), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_NORMAL_ARRAY_STRIDE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_NORMAL_ARRAY_POINTER), TYPE_P6GLvoid, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
#ifdef GL_EXT_fog_coord
    { "GL_FOG_COORD_ARRAY", GL_FOG_COORDINATE_ARRAY_EXT, TYPE_9GLboolean, -1, BUGLE_GL_EXT_fog_coord, -1, STATE_ENABLED },
    { "GL_FOG_COORD_ARRAY_TYPE", GL_FOG_COORDINATE_ARRAY_TYPE_EXT, TYPE_6GLenum, -1, BUGLE_GL_EXT_fog_coord, -1, STATE_GLOBAL },
    { "GL_FOG_COORD_ARRAY_STRIDE", GL_FOG_COORDINATE_ARRAY_STRIDE_EXT, TYPE_5GLint, -1, BUGLE_GL_EXT_fog_coord, -1, STATE_GLOBAL },
    { "GL_FOG_COORD_ARRAY_POINTER", GL_FOG_COORDINATE_ARRAY_POINTER_EXT, TYPE_P6GLvoid, -1, BUGLE_GL_EXT_fog_coord, -1, STATE_GLOBAL },
#endif
    { STATE_NAME(GL_COLOR_ARRAY), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_COLOR_ARRAY_SIZE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_COLOR_ARRAY_TYPE), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_COLOR_ARRAY_STRIDE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_COLOR_ARRAY_POINTER), TYPE_P6GLvoid, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
#ifdef GL_EXT_secondary_color
    { STATE_NAME_EXT(GL_SECONDARY_COLOR_ARRAY, _EXT), TYPE_9GLboolean, -1, BUGLE_GL_EXT_secondary_color, -1, STATE_ENABLED },
    { STATE_NAME_EXT(GL_SECONDARY_COLOR_ARRAY_SIZE, _EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_secondary_color, -1, STATE_GLOBAL },
    { STATE_NAME_EXT(GL_SECONDARY_COLOR_ARRAY_TYPE, _EXT), TYPE_6GLenum, -1, BUGLE_GL_EXT_secondary_color, -1, STATE_GLOBAL },
    { STATE_NAME_EXT(GL_SECONDARY_COLOR_ARRAY_STRIDE, _EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_secondary_color, -1, STATE_GLOBAL },
    { STATE_NAME_EXT(GL_SECONDARY_COLOR_ARRAY_POINTER, _EXT), TYPE_P6GLvoid, -1, BUGLE_GL_EXT_secondary_color, -1, STATE_GLOBAL },
#endif
    { STATE_NAME(GL_INDEX_ARRAY), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_INDEX_ARRAY_TYPE), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_INDEX_ARRAY_STRIDE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_INDEX_ARRAY_POINTER), TYPE_P6GLvoid, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_EDGE_FLAG_ARRAY), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_EDGE_FLAG_ARRAY_STRIDE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_EDGE_FLAG_ARRAY_POINTER), TYPE_P6GLvoid, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
#ifdef GL_ARB_vertex_program
    { "Attribute Arrays", GL_NONE, NULL_TYPE, -1, BUGLE_EXTGROUP_vertex_attrib, -1, 0, spawn_vertex_attrib_arrays },
#endif
#ifdef GL_ARB_vertex_buffer_object
    { STATE_NAME_EXT(GL_ARRAY_BUFFER_BINDING, _ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_buffer_object, -1, STATE_GLOBAL },
    { STATE_NAME_EXT(GL_VERTEX_ARRAY_BUFFER_BINDING, _ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_buffer_object, -1, STATE_GLOBAL },
    { STATE_NAME_EXT(GL_NORMAL_ARRAY_BUFFER_BINDING, _ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_buffer_object, -1, STATE_GLOBAL },
    { STATE_NAME_EXT(GL_COLOR_ARRAY_BUFFER_BINDING, _ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_buffer_object, -1, STATE_GLOBAL },
    { STATE_NAME_EXT(GL_INDEX_ARRAY_BUFFER_BINDING, _ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_buffer_object, -1, STATE_GLOBAL },
    { STATE_NAME_EXT(GL_EDGE_FLAG_ARRAY_BUFFER_BINDING, _ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_buffer_object, -1, STATE_GLOBAL },
    { STATE_NAME_EXT(GL_SECONDARY_COLOR_ARRAY_BUFFER_BINDING, _ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_buffer_object, -1, STATE_GLOBAL },
    { STATE_NAME_EXT(GL_FOG_COORDINATE_ARRAY_BUFFER_BINDING, _ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_buffer_object, -1, STATE_GLOBAL },
    { STATE_NAME_EXT(GL_ELEMENT_ARRAY_BUFFER_BINDING, _ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_buffer_object, -1, STATE_GLOBAL },
#endif
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
#ifdef GL_EXT_rescale_normal
    { STATE_NAME_EXT(GL_RESCALE_NORMAL, _EXT), TYPE_9GLboolean, -1, BUGLE_GL_EXT_rescale_normal, -1, STATE_ENABLED },
#endif
    /* FIXME: clip plane enables */
    { STATE_NAME(GL_CLIP_PLANE0), TYPE_8GLdouble, 4, BUGLE_GL_VERSION_1_1, -1, STATE_CLIP_PLANE, spawn_clip_planes },
    { STATE_NAME(GL_FOG_COLOR), TYPE_8GLdouble, 4, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_FOG_INDEX), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_FOG_DENSITY), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_FOG_START), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_FOG_END), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_FOG_MODE), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_FOG), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
#ifdef GL_EXT_fog_coord
    { "GL_FOG_COORD_SRC", GL_FOG_COORDINATE_SOURCE_EXT, TYPE_6GLenum, -1, BUGLE_GL_EXT_fog_coord, -1, STATE_GLOBAL },
#endif
#ifdef GL_EXT_secondary_color
    { STATE_NAME_EXT(GL_COLOR_SUM, _EXT), TYPE_9GLboolean, -1, BUGLE_GL_EXT_secondary_color, -1, STATE_ENABLED },
#endif
    { STATE_NAME(GL_SHADE_MODEL), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_LIGHTING), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_COLOR_MATERIAL), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_COLOR_MATERIAL_PARAMETER), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_COLOR_MATERIAL_FACE), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { "Materials", 0, NULL_TYPE, -1, BUGLE_GL_VERSION_1_1, -1, 0, spawn_materials },
    { STATE_NAME(GL_LIGHT_MODEL_AMBIENT), TYPE_8GLdouble, 4, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_LIGHT_MODEL_LOCAL_VIEWER), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_LIGHT_MODEL_TWO_SIDE), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
#ifdef GL_EXT_separate_specular_color
    { STATE_NAME(GL_LIGHT_MODEL_COLOR_CONTROL), TYPE_6GLenum, -1, BUGLE_GL_EXT_separate_specular_color, -1, STATE_GLOBAL },
#endif
    { "Lights", 0, NULL_TYPE, -1, BUGLE_GL_VERSION_1_1, -1, 0, spawn_lights },
    { STATE_NAME(GL_POINT_SIZE), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_POINT_SMOOTH), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
#ifdef GL_ARB_point_sprite
    { STATE_NAME_EXT(GL_POINT_SPRITE, _ARB), TYPE_9GLboolean, -1, BUGLE_GL_ARB_point_sprite, -1, STATE_ENABLED },
#endif
#ifdef GL_EXT_point_parameters
    { STATE_NAME_EXT(GL_POINT_SIZE_MIN, _EXT), TYPE_8GLdouble, -1, BUGLE_GL_EXT_point_parameters, -1, STATE_GLOBAL },
    { STATE_NAME_EXT(GL_POINT_SIZE_MAX, _EXT), TYPE_8GLdouble, -1, BUGLE_GL_EXT_point_parameters, -1, STATE_GLOBAL },
    { STATE_NAME_EXT(GL_POINT_FADE_THRESHOLD_SIZE, _EXT), TYPE_8GLdouble, -1, BUGLE_GL_EXT_point_parameters, -1, STATE_GLOBAL },
    { "GL_POINT_DISTANCE_ATTENUATION", GL_DISTANCE_ATTENUATION_EXT, TYPE_8GLdouble, 3, BUGLE_GL_EXT_point_parameters, -1, STATE_GLOBAL },
#endif
#ifdef GL_VERSION_2_0
    { STATE_NAME(GL_POINT_SPRITE_COORD_ORIGIN), TYPE_6GLenum, -1, BUGLE_GL_VERSION_2_0, -1, STATE_GLOBAL },
#endif
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
#ifdef GL_ARB_multisample
    { STATE_NAME_EXT(GL_MULTISAMPLE, _ARB), TYPE_9GLboolean, -1, BUGLE_GL_ARB_multisample, -1, STATE_ENABLED },
    { STATE_NAME_EXT(GL_SAMPLE_ALPHA_TO_COVERAGE, _ARB), TYPE_9GLboolean, -1, BUGLE_GL_ARB_multisample, -1, STATE_ENABLED },
    { STATE_NAME_EXT(GL_SAMPLE_ALPHA_TO_ONE, _ARB), TYPE_9GLboolean, -1, BUGLE_GL_ARB_multisample, -1, STATE_ENABLED },
    { STATE_NAME_EXT(GL_SAMPLE_COVERAGE, _ARB), TYPE_9GLboolean, -1, BUGLE_GL_ARB_multisample, -1, STATE_ENABLED },
    { STATE_NAME_EXT(GL_SAMPLE_COVERAGE_VALUE, _ARB), TYPE_8GLdouble, -1, BUGLE_GL_ARB_multisample, -1, STATE_GLOBAL },
    { STATE_NAME_EXT(GL_SAMPLE_COVERAGE_INVERT, _ARB), TYPE_9GLboolean, -1, BUGLE_GL_ARB_multisample, -1, STATE_GLOBAL },
#endif
    { "Texture units", GL_TEXTURE0, NULL_TYPE, -1, BUGLE_GL_VERSION_1_1, -1, 0, spawn_texture_units },
    { "Textures", GL_TEXTURE_2D, NULL_TYPE, -1, BUGLE_GL_VERSION_1_1, -1, 0, spawn_textures },
#ifdef GL_ARB_multitexture
    { STATE_NAME_EXT(GL_ACTIVE_TEXTURE, _ARB), TYPE_6GLenum, -1, BUGLE_GL_ARB_multitexture, -1, STATE_GLOBAL },
#endif
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
#ifdef GL_VERSION_2_0
    { STATE_NAME(GL_STENCIL_BACK_FUNC), TYPE_6GLenum, -1, BUGLE_GL_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_BACK_VALUE_MASK), TYPE_5GLint, -1, BUGLE_GL_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_BACK_REF), TYPE_5GLint, -1, BUGLE_GL_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_BACK_FAIL), TYPE_6GLenum, -1, BUGLE_GL_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_BACK_PASS_DEPTH_FAIL), TYPE_6GLenum, -1, BUGLE_GL_VERSION_2_0, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_BACK_PASS_DEPTH_PASS), TYPE_6GLenum, -1, BUGLE_GL_VERSION_2_0, -1, STATE_GLOBAL },
#endif
    { STATE_NAME(GL_DEPTH_TEST), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_DEPTH_FUNC), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
#ifdef GL_EXT_draw_buffers2
    { STATE_NAME(GL_BLEND), TYPE_9GLboolean, -1, BUGLE_GL_EXT_draw_buffers2, -1, STATE_ENABLED, spawn_blend },
    { STATE_NAME(GL_BLEND), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, BUGLE_GL_EXT_draw_buffers2, STATE_ENABLED },
#else
    { STATE_NAME(GL_BLEND), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
#endif /* !GL_EXT_draw_buffers2 */
#ifdef GL_EXT_blend_func_separate
    { STATE_NAME_EXT(GL_BLEND_SRC_RGB, _EXT), TYPE_11GLblendenum, -1, BUGLE_GL_EXT_blend_func_separate, -1, STATE_GLOBAL },
    { STATE_NAME_EXT(GL_BLEND_SRC_ALPHA, _EXT), TYPE_11GLblendenum, -1, BUGLE_GL_EXT_blend_func_separate, -1, STATE_GLOBAL },
    { STATE_NAME_EXT(GL_BLEND_DST_RGB, _EXT), TYPE_11GLblendenum, -1, BUGLE_GL_EXT_blend_func_separate, -1, STATE_GLOBAL },
    { STATE_NAME_EXT(GL_BLEND_DST_ALPHA, _EXT), TYPE_11GLblendenum, -1, BUGLE_GL_EXT_blend_func_separate, -1, STATE_GLOBAL },
    { STATE_NAME(GL_BLEND_SRC), TYPE_11GLblendenum, -1, BUGLE_GL_VERSION_1_1, BUGLE_GL_EXT_blend_func_separate, STATE_GLOBAL },
    { STATE_NAME(GL_BLEND_DST), TYPE_11GLblendenum, -1, BUGLE_GL_VERSION_1_1, BUGLE_GL_EXT_blend_func_separate, STATE_GLOBAL },
#else
    { "GL_BLEND_SRC_RGB", GL_BLEND_SRC, TYPE_11GLblendenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { "GL_BLEND_DST_RGB", GL_BLEND_DST, TYPE_11GLblendenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
#endif
    { "GL_BLEND_EQUATION_RGB", GL_BLEND_EQUATION, TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
#ifdef GL_EXT_blend_equation_separate
    { STATE_NAME_EXT(GL_BLEND_EQUATION_ALPHA, _EXT), TYPE_6GLenum, -1, BUGLE_GL_EXT_blend_equation_separate, -1, STATE_GLOBAL },
#endif
#ifdef GL_EXT_blend_color
    { STATE_NAME_EXT(GL_BLEND_COLOR, _EXT), TYPE_8GLdouble, 4, BUGLE_EXTGROUP_blend_color, -1, STATE_GLOBAL },
#endif
    { STATE_NAME(GL_DITHER), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_INDEX_LOGIC_OP), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_COLOR_LOGIC_OP), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED },
    { STATE_NAME(GL_LOGIC_OP_MODE), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
#ifdef GL_ATI_draw_buffers
    { STATE_NAME(GL_DRAW_BUFFER), TYPE_6GLenum, -1, GL_VERSION_1_1, BUGLE_GL_ATI_draw_buffers, STATE_GLOBAL },
    { STATE_NAME_EXT(GL_DRAW_BUFFER0, _ATI), TYPE_6GLenum, -1, BUGLE_GL_ATI_draw_buffers, -1, STATE_GLOBAL, spawn_draw_buffers },
#else
    { STATE_NAME(GL_DRAW_BUFFER), TYPE_6GLenum, -1, GL_VERSION_1_1, -1, STATE_GLOBAL },
#endif
    { STATE_NAME(GL_INDEX_WRITEMASK), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
#ifdef GL_EXT_draw_buffers2
    { STATE_NAME(GL_COLOR_WRITEMASK), TYPE_9GLboolean, 4, BUGLE_GL_EXT_draw_buffers2, -1, STATE_GLOBAL, spawn_color_writemask },
    { STATE_NAME(GL_COLOR_WRITEMASK), TYPE_9GLboolean, 4, BUGLE_GL_VERSION_1_1, BUGLE_GL_EXT_draw_buffers2, STATE_GLOBAL },
#else
    { STATE_NAME(GL_COLOR_WRITEMASK), TYPE_9GLboolean, 4, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
#endif
    { STATE_NAME(GL_DEPTH_WRITEMASK), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_WRITEMASK), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
#ifdef GL_VERSION_2_0
    { STATE_NAME(GL_STENCIL_BACK_WRITEMASK), TYPE_5GLint, -1, BUGLE_GL_VERSION_2_0, -1, STATE_GLOBAL },
#endif
    { STATE_NAME(GL_COLOR_CLEAR_VALUE), TYPE_8GLdouble, 4, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_INDEX_CLEAR_VALUE), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_DEPTH_CLEAR_VALUE), TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_CLEAR_VALUE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_ACCUM_CLEAR_VALUE), TYPE_8GLdouble, 4, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_UNPACK_SWAP_BYTES), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_UNPACK_LSB_FIRST), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
#ifdef GL_EXT_texture3D
    { STATE_NAME_EXT(GL_UNPACK_IMAGE_HEIGHT, _EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_texture3D, -1, STATE_GLOBAL },
    { STATE_NAME_EXT(GL_UNPACK_SKIP_IMAGES, _EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_texture3D, -1, STATE_GLOBAL },
#endif
    { STATE_NAME(GL_UNPACK_ROW_LENGTH), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_UNPACK_SKIP_ROWS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_UNPACK_SKIP_PIXELS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_UNPACK_ALIGNMENT), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_PACK_SWAP_BYTES), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_PACK_LSB_FIRST), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
#ifdef GL_EXT_texture3D
    { STATE_NAME_EXT(GL_PACK_IMAGE_HEIGHT, _EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_texture3D, -1, STATE_GLOBAL },
    { STATE_NAME_EXT(GL_PACK_SKIP_IMAGES, _EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_texture3D, -1, STATE_GLOBAL },
#endif
    { STATE_NAME(GL_PACK_ROW_LENGTH), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_PACK_SKIP_ROWS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_PACK_SKIP_PIXELS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_PACK_ALIGNMENT), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
#ifdef GL_EXT_pixel_buffer_object
    { STATE_NAME_EXT(GL_PIXEL_PACK_BUFFER_BINDING, _EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_pixel_buffer_object, -1, STATE_GLOBAL },
    { STATE_NAME_EXT(GL_PIXEL_UNPACK_BUFFER_BINDING, _EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_pixel_buffer_object, -1, STATE_GLOBAL },
#endif
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

#ifdef GL_ARB_shader_objects
    { "Shader objects", 0, NULL_TYPE, -1, BUGLE_GL_ARB_shader_objects, -1, 0, spawn_shaders },
#endif
#ifdef GL_VERSION_2_0
    { STATE_NAME(GL_CURRENT_PROGRAM), TYPE_5GLint, -1, BUGLE_GL_VERSION_2_0, -1, STATE_GLOBAL },
#endif
#ifdef GL_ARB_shader_objects
    { "Program objects", 0, NULL_TYPE, -1, BUGLE_GL_ARB_shader_objects, -1, 0, spawn_programs },
#endif
#ifdef GL_ARB_vertex_program
    { STATE_NAME_EXT(GL_VERTEX_PROGRAM_TWO_SIDE, _ARB), TYPE_9GLboolean, -1, BUGLE_EXTGROUP_vp_options, -1, STATE_ENABLED },
    { "GL_PROGRAM_POINT_SIZE", GL_VERTEX_PROGRAM_POINT_SIZE_ARB, TYPE_9GLboolean, -1, BUGLE_EXTGROUP_vp_options, -1, STATE_ENABLED },
#endif
    { STATE_NAME(GL_PERSPECTIVE_CORRECTION_HINT), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_POINT_SMOOTH_HINT), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_LINE_SMOOTH_HINT), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_POLYGON_SMOOTH_HINT), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_FOG_HINT), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
#ifdef GL_SGIS_generate_mipmap
    { STATE_NAME_EXT(GL_GENERATE_MIPMAP_HINT, _SGIS), TYPE_6GLenum, -1, BUGLE_GL_SGIS_generate_mipmap, -1, STATE_GLOBAL },
#endif
#ifdef GL_ARB_texture_compression
    { STATE_NAME_EXT(GL_TEXTURE_COMPRESSION_HINT, _ARB), TYPE_6GLenum, -1, BUGLE_GL_ARB_texture_compression, -1, STATE_GLOBAL },
#endif
    /* glext.h v21 doesn't define GL_FRAGMENT_SHADER_DERIVATIVE_HINT_ARB (based on older version of spec) */
#if defined(GL_ARB_fragment_shader) && defined(GL_FRAGMENT_SHADER_DERIVATIVE_HINT_ARB)
    { STATE_NAME_EXT(GL_FRAGMENT_SHADER_DERIVATIVE_HINT, _ARB), TYPE_6GLenum, -1, BUGLE_GL_ARB_fragment_shader, -1, STATE_GLOBAL },
#endif
    { STATE_NAME(GL_MAX_LIGHTS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_CLIP_PLANES), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_COLOR_MATRIX_STACK_DEPTH), TYPE_5GLint, -1, BUGLE_GL_ARB_imaging, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_MODELVIEW_STACK_DEPTH), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_PROJECTION_STACK_DEPTH), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_TEXTURE_STACK_DEPTH), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_SUBPIXEL_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
#ifdef GL_EXT_texture3D
    { STATE_NAME_EXT(GL_MAX_3D_TEXTURE_SIZE, _EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_texture3D, -1, STATE_GLOBAL },
#endif
    { STATE_NAME(GL_MAX_TEXTURE_SIZE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
#ifdef GL_EXT_texture_lod_bias
    { STATE_NAME_EXT(GL_MAX_TEXTURE_LOD_BIAS, _EXT), TYPE_8GLdouble, -1, BUGLE_GL_EXT_texture_lod_bias, -1, STATE_GLOBAL },
#endif
#ifdef GL_ARB_texture_cube_map
    { STATE_NAME_EXT(GL_MAX_CUBE_MAP_TEXTURE_SIZE, _ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_texture_cube_map, -1, STATE_GLOBAL },
#endif
    { STATE_NAME(GL_MAX_PIXEL_MAP_TABLE), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_NAME_STACK_DEPTH), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_LIST_NESTING), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_EVAL_ORDER), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_VIEWPORT_DIMS), TYPE_5GLint, 2, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_ATTRIB_STACK_DEPTH), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_CLIENT_ATTRIB_STACK_DEPTH), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_AUX_BUFFERS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_RGBA_MODE), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_INDEX_MODE), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_DOUBLEBUFFER), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_STEREO), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_ALIASED_POINT_SIZE_RANGE), TYPE_8GLdouble, 2, BUGLE_GL_VERSION_1_2, -1, STATE_GLOBAL },
    { "GL_SMOOTH_POINT_SIZE_RANGE", GL_POINT_SIZE_RANGE, TYPE_8GLdouble, 2, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { "GL_SMOOTH_POINT_SIZE_GRANULARITY", GL_POINT_SIZE_GRANULARITY, TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_ALIASED_LINE_WIDTH_RANGE), TYPE_8GLdouble, 2, BUGLE_GL_VERSION_1_2, -1, STATE_GLOBAL },
    { "GL_SMOOTH_LINE_WIDTH_RANGE", GL_LINE_WIDTH_RANGE, TYPE_8GLdouble, 2, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { "GL_SMOOTH_LINE_WIDTH_GRANULARITY", GL_LINE_WIDTH_GRANULARITY, TYPE_8GLdouble, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_ELEMENTS_INDICES), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_ELEMENTS_VERTICES), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
#ifdef GL_ARB_multisample
    { STATE_NAME_EXT(GL_SAMPLE_BUFFERS, _ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_multisample, -1, STATE_GLOBAL },
    { STATE_NAME_EXT(GL_SAMPLES, _ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_multisample, -1, STATE_GLOBAL },
#endif
#ifdef GL_ARB_texture_compression
    { STATE_NAME_EXT(GL_COMPRESSED_TEXTURE_FORMATS, _ARB), TYPE_6GLenum, -1, BUGLE_GL_ARB_texture_compression, -1, STATE_COMPRESSED_TEXTURE_FORMATS },
    { STATE_NAME_EXT(GL_NUM_COMPRESSED_TEXTURE_FORMATS, _ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_texture_compression, -1, STATE_GLOBAL },
#endif
    { STATE_NAME(GL_EXTENSIONS), TYPE_PKc, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_RENDERER), TYPE_PKc, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    /* SHADING_LANGUAGE_VERSION was only added in a later version of the spec */
#if defined(GL_ARB_shading_language_100) && defined(GL_SHADING_LANGUAGE_VERSION_ARB)
    { STATE_NAME_EXT(GL_SHADING_LANGUAGE_VERSION, _ARB), TYPE_PKc, -1, BUGLE_GL_ARB_shading_language_100, -1, STATE_GLOBAL },
#endif
    { STATE_NAME(GL_VENDOR), TYPE_PKc, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
    { STATE_NAME(GL_VERSION), TYPE_PKc, -1, BUGLE_GL_VERSION_1_1, -1, STATE_GLOBAL },
#ifdef GL_ARB_multitexture
    { STATE_NAME_EXT(GL_MAX_TEXTURE_UNITS, _ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_multitexture, -1, STATE_GLOBAL },
#endif
#ifdef GL_ARB_vertex_program
    { STATE_NAME_EXT(GL_MAX_VERTEX_ATTRIBS, _ARB), TYPE_5GLint, -1, BUGLE_EXTGROUP_vertex_attrib, -1, STATE_GLOBAL },
#endif
#ifdef GL_ARB_vertex_shader
    { STATE_NAME_EXT(GL_MAX_VERTEX_UNIFORM_COMPONENTS, _ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_shader, -1, STATE_GLOBAL },
    { "GL_MAX_VARYING_COMPONENTS", GL_MAX_VARYING_FLOATS_ARB, TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_shader, -1, STATE_GLOBAL },
    { STATE_NAME_EXT(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, _ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_shader, -1, STATE_GLOBAL },
    { STATE_NAME_EXT(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, _ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_shader, -1, STATE_GLOBAL },
#endif
#ifdef GL_ARB_fragment_program
    { STATE_NAME_EXT(GL_MAX_TEXTURE_IMAGE_UNITS, _ARB), TYPE_5GLint, -1, BUGLE_EXTGROUP_texunits, -1, STATE_GLOBAL },
    { STATE_NAME_EXT(GL_MAX_TEXTURE_COORDS, _ARB), TYPE_5GLint, -1, BUGLE_EXTGROUP_texunits, -1, STATE_GLOBAL },
#endif
#ifdef GL_ARB_fragment_shader
    { STATE_NAME_EXT(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, _ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_fragment_shader, -1, STATE_GLOBAL },
#endif
#ifdef GL_ATI_draw_buffers
    { STATE_NAME_EXT(GL_MAX_DRAW_BUFFERS, _ATI), TYPE_5GLint, -1, BUGLE_GL_ATI_draw_buffers, -1, STATE_GLOBAL },
#endif
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
    /* FIXME: glGetError */

    /* The remaining state is not part of the GL 2.1 state, but pure
     * extensions.
     */
#if defined(GL_ARB_vertex_program) || defined(GL_ARB_fragment_program)
    { STATE_NAME(GL_PROGRAM_ERROR_POSITION_ARB), TYPE_5GLint, -1, BUGLE_EXTGROUP_old_program, -1, STATE_GLOBAL },
    { STATE_NAME(GL_PROGRAM_ERROR_STRING_ARB), TYPE_PKc, -1, BUGLE_EXTGROUP_old_program, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_PROGRAM_MATRICES_ARB), TYPE_5GLint, -1, BUGLE_EXTGROUP_old_program, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_PROGRAM_MATRIX_STACK_DEPTH_ARB), TYPE_5GLint, -1, BUGLE_EXTGROUP_old_program, -1, STATE_GLOBAL },
#endif
#ifdef GL_EXT_texture_filter_anisotropic
    { STATE_NAME(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT), TYPE_8GLdouble, -1, BUGLE_GL_EXT_texture_filter_anisotropic, -1, STATE_GLOBAL },
#endif
#ifdef GL_NV_texture_rectangle
    { STATE_NAME(GL_MAX_RECTANGLE_TEXTURE_SIZE_NV), TYPE_5GLint, -1, BUGLE_GL_NV_texture_rectangle, -1, STATE_GLOBAL },
#endif
#ifdef GL_NV_depth_clamp
    { STATE_NAME(GL_DEPTH_CLAMP_NV), TYPE_9GLboolean, -1, BUGLE_GL_NV_depth_clamp, -1, STATE_ENABLED },
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
#ifdef GL_EXT_framebuffer_object
    { STATE_NAME(GL_RENDERBUFFER_BINDING_EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_framebuffer_object, -1, STATE_GLOBAL },
    { STATE_NAME(GL_FRAMEBUFFER_BINDING_EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_framebuffer_object, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_RENDERBUFFER_SIZE_EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_framebuffer_object, -1, STATE_GLOBAL },
#endif
#ifdef GL_EXT_depth_bounds_test
    { STATE_NAME(GL_DEPTH_BOUNDS_TEST_EXT), TYPE_9GLboolean, -1, BUGLE_GL_EXT_depth_bounds_test, -1, STATE_ENABLED },
    { STATE_NAME(GL_DEPTH_BOUNDS_EXT), TYPE_8GLdouble, 2, BUGLE_GL_EXT_depth_bounds_test, -1, STATE_GLOBAL },
#endif
    /* G80 extensions */
#ifdef GL_EXT_texture_array
    { STATE_NAME(GL_MAX_ARRAY_TEXTURE_LAYERS_EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_texture_array, -1, STATE_GLOBAL },
#endif
#ifdef GL_EXT_texture_buffer_object
    { STATE_NAME(GL_MAX_TEXTURE_BUFFER_SIZE_EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_texture_buffer_object, -1, STATE_GLOBAL },
#endif
#ifdef GL_EXT_geometry_shader4
    { STATE_NAME(GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS_EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_geometry_shader4, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_GEOMETRY_OUTPUT_VERTICES_EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_geometry_shader4, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS_EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_geometry_shader4, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_GEOMETRY_UNIFORM_COMPONENTS_EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_geometry_shader4, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_GEOMETRY_VARYING_COMPONENTS_EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_geometry_shader4, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_VERTEX_VARYING_COMPONENTS_EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_geometry_shader4, -1, STATE_GLOBAL },
#endif
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
#ifdef GL_EXT_framebuffer_sRGB
    { STATE_NAME(GL_FRAMEBUFFER_SRGB_CAPABLE_EXT), TYPE_9GLboolean, 4, BUGLE_GL_EXT_framebuffer_sRGB, -1, STATE_GLOBAL },
    { STATE_NAME(GL_FRAMEBUFFER_SRGB_EXT), TYPE_9GLboolean, -1, BUGLE_GL_EXT_framebuffer_sRGB, -1, STATE_ENABLED },
#endif
#ifdef GL_NV_transform_feedback
    /* glext.h gets the name wrong */
#ifdef GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS_NV
    { STATE_NAME(GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS_NV), TYPE_5GLint, -1, BUGLE_GL_NV_transform_feedback, -1, STATE_GLOBAL },
#else
    { "GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS_NV", 0x8C8A, TYPE_5GLint, -1, BUGLE_GL_NV_transform_feedback, -1, STATE_GLOBAL },
#endif
    { STATE_NAME(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS_NV), TYPE_5GLint, -1, BUGLE_GL_NV_transform_feedback, -1, STATE_GLOBAL },
    { STATE_NAME(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS_NV), TYPE_5GLint, -1, BUGLE_GL_NV_transform_feedback, -1, STATE_GLOBAL },
    { STATE_NAME(GL_TRANSFORM_FEEDBACK_BUFFER_MODE_NV), TYPE_6GLenum, -1, BUGLE_GL_NV_transform_feedback, -1, STATE_GLOBAL },
    { STATE_NAME(GL_TRANSFORM_FEEDBACK_ATTRIBS_NV), TYPE_5GLint, -1, BUGLE_GL_NV_transform_feedback, -1, STATE_GLOBAL },
    { STATE_NAME(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING_NV), TYPE_5GLint, -1, BUGLE_GL_NV_transform_feedback, -1, STATE_GLOBAL },
    { STATE_NAME(GL_RASTERIZER_DISCARD_NV), TYPE_9GLboolean, -1, BUGLE_GL_NV_transform_feedback, -1, STATE_ENABLED },
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
#ifdef GL_ARB_occlusion_query
    { STATE_NAME_EXT(GL_CURRENT_QUERY, _ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_occlusion_query, -1, STATE_QUERY },
    { STATE_NAME_EXT(GL_QUERY_COUNTER_BITS, _ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_occlusion_query, -1, STATE_QUERY },
#endif
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info query_object_state[] =
{
#ifdef GL_ARB_occlusion_query
    { STATE_NAME_EXT(GL_QUERY_RESULT, _ARB), TYPE_6GLuint, -1, BUGLE_GL_ARB_occlusion_query, -1, STATE_QUERY_OBJECT },
    { STATE_NAME_EXT(GL_QUERY_RESULT_AVAILABLE, _ARB), TYPE_9GLboolean, -1, BUGLE_GL_ARB_occlusion_query, -1, STATE_QUERY_OBJECT },
#endif
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info buffer_parameter_state[] =
{
#ifdef GL_ARB_vertex_buffer_object
    { STATE_NAME_EXT(GL_BUFFER_SIZE, _ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_vertex_buffer_object, -1, STATE_BUFFER_PARAMETER },
    { STATE_NAME_EXT(GL_BUFFER_USAGE, _ARB), TYPE_6GLenum, -1, BUGLE_GL_ARB_vertex_buffer_object, -1, STATE_BUFFER_PARAMETER },
    { STATE_NAME_EXT(GL_BUFFER_ACCESS, _ARB), TYPE_6GLenum, -1, BUGLE_GL_ARB_vertex_buffer_object, -1, STATE_BUFFER_PARAMETER },
    { STATE_NAME_EXT(GL_BUFFER_MAPPED, _ARB), TYPE_9GLboolean, -1, BUGLE_GL_ARB_vertex_buffer_object, -1, STATE_BUFFER_PARAMETER },
    { STATE_NAME_EXT(GL_BUFFER_MAP_POINTER, _ARB), TYPE_P6GLvoid, -1, BUGLE_GL_ARB_vertex_buffer_object, -1, STATE_BUFFER_PARAMETER },
#endif
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info renderbuffer_parameter_state[] =
{
#ifdef GL_EXT_framebuffer_object
    { STATE_NAME(GL_RENDERBUFFER_WIDTH_EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_framebuffer_object, -1, STATE_RENDERBUFFER_PARAMETER },
    { STATE_NAME(GL_RENDERBUFFER_HEIGHT_EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_framebuffer_object, -1, STATE_RENDERBUFFER_PARAMETER },
    { STATE_NAME(GL_RENDERBUFFER_INTERNAL_FORMAT_EXT), TYPE_6GLenum, -1, BUGLE_GL_EXT_framebuffer_object, -1, STATE_RENDERBUFFER_PARAMETER },
#ifdef GL_RENDERBUFFER_RED_SIZE_EXT /* glext.h version 26 is missing these */
    { STATE_NAME(GL_RENDERBUFFER_RED_SIZE_EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_framebuffer_object, -1, STATE_RENDERBUFFER_PARAMETER },
    { STATE_NAME(GL_RENDERBUFFER_GREEN_SIZE_EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_framebuffer_object, -1, STATE_RENDERBUFFER_PARAMETER },
    { STATE_NAME(GL_RENDERBUFFER_BLUE_SIZE_EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_framebuffer_object, -1, STATE_RENDERBUFFER_PARAMETER },
    { STATE_NAME(GL_RENDERBUFFER_ALPHA_SIZE_EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_framebuffer_object, -1, STATE_RENDERBUFFER_PARAMETER },
    { STATE_NAME(GL_RENDERBUFFER_DEPTH_SIZE_EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_framebuffer_object, -1, STATE_RENDERBUFFER_PARAMETER },
    { STATE_NAME(GL_RENDERBUFFER_STENCIL_SIZE_EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_framebuffer_object, -1, STATE_RENDERBUFFER_PARAMETER },
#endif
#endif
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

/* Note: the below is used only if FBO is already available so we don't
 * have to explicitly check for it.
 */
static const state_info framebuffer_parameter_state[] =
{
#ifdef GL_EXT_framebuffer_object
#ifdef GL_ATI_draw_buffers
    { STATE_NAME(GL_DRAW_BUFFER), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, BUGLE_GL_ATI_draw_buffers, STATE_FRAMEBUFFER_PARAMETER },
    { STATE_NAME_EXT(GL_DRAW_BUFFER0, _ATI), TYPE_6GLenum, -1, BUGLE_GL_ATI_draw_buffers, -1, STATE_FRAMEBUFFER_PARAMETER, spawn_draw_buffers },
#else
    { STATE_NAME(GL_DRAW_BUFFER), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_FRAMEBUFFER_PARAMETER },
#endif
    { STATE_NAME(GL_READ_BUFFER), TYPE_6GLenum, -1, BUGLE_GL_VERSION_1_1, -1, STATE_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_AUX_BUFFERS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_FRAMEBUFFER_PARAMETER },
#ifdef GL_ATI_draw_buffers
    { STATE_NAME_EXT(GL_MAX_DRAW_BUFFERS, _ATI), TYPE_5GLint, -1, BUGLE_GL_ATI_draw_buffers, -1, STATE_FRAMEBUFFER_PARAMETER },
#endif
    { STATE_NAME(GL_RGBA_MODE), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_INDEX_MODE), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_DOUBLEBUFFER), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_STEREO), TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_FRAMEBUFFER_PARAMETER },
#ifdef GL_ARB_multisample
    { STATE_NAME_EXT(GL_SAMPLE_BUFFERS, _ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_multisample, -1, STATE_FRAMEBUFFER_PARAMETER },
    { STATE_NAME_EXT(GL_SAMPLES, _ARB), TYPE_5GLint, -1, BUGLE_GL_ARB_multisample, -1, STATE_FRAMEBUFFER_PARAMETER },
#endif
    { STATE_NAME(GL_RED_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_GREEN_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_BLUE_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_ALPHA_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_DEPTH_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_STENCIL_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_ACCUM_RED_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_ACCUM_GREEN_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_ACCUM_BLUE_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_ACCUM_ALPHA_BITS), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_STENCIL_REF), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_FRAMEBUFFER_PARAMETER },
    { STATE_NAME(GL_MAX_COLOR_ATTACHMENTS_EXT), TYPE_5GLint, -1, BUGLE_GL_VERSION_1_1, -1, STATE_FRAMEBUFFER_PARAMETER },
#ifdef GL_EXT_packed_float
    { STATE_NAME(GL_RGBA_SIGNED_COMPONENTS_EXT), TYPE_9GLboolean, 4, BUGLE_GL_EXT_packed_float, -1, STATE_FRAMEBUFFER_PARAMETER },
#endif
#endif /* GL_EXT_framebuffer_object */
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info framebuffer_attachment_parameter_state[] =
{
#ifdef GL_EXT_framebuffer_object
    { STATE_NAME(GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE_EXT), TYPE_6GLenum, -1, BUGLE_GL_EXT_framebuffer_object, -1, STATE_FRAMEBUFFER_ATTACHMENT_PARAMETER },
    { STATE_NAME(GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME_EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_framebuffer_object, -1, STATE_FRAMEBUFFER_ATTACHMENT_PARAMETER },
    { STATE_NAME(GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL_EXT), TYPE_5GLint, -1, BUGLE_GL_EXT_framebuffer_object, -1, STATE_FRAMEBUFFER_ATTACHMENT_PARAMETER },
    { STATE_NAME(GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE_EXT), TYPE_6GLenum, -1, BUGLE_GL_EXT_framebuffer_object, -1, STATE_FRAMEBUFFER_ATTACHMENT_PARAMETER },
    { "GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER_EXT", GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_3D_ZOFFSET_EXT, TYPE_5GLint, -1, BUGLE_GL_EXT_framebuffer_object, -1, STATE_FRAMEBUFFER_ATTACHMENT_PARAMETER },
#ifdef GL_EXT_geometry_shader4
    { STATE_NAME(GL_FRAMEBUFFER_ATTACHMENT_LAYERED_EXT), TYPE_9GLboolean, -1, BUGLE_GL_EXT_geometry_shader4, -1, STATE_FRAMEBUFFER_ATTACHMENT_PARAMETER },
#endif
#endif /* GL_EXT_framebuffer_object */
    { NULL, GL_NONE, NULL_TYPE, 0, -1, -1, 0 }
};

static const state_info transform_feedback_buffer_state[] =
{
#ifdef GL_NV_transform_feedback
    { STATE_NAME(GL_TRANSFORM_FEEDBACK_RECORD_NV), TYPE_11GLxfbattrib, -1, BUGLE_GL_NV_transform_feedback, -1, STATE_INDEXED },
    { STATE_NAME(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING_NV), TYPE_5GLint, -1, BUGLE_GL_NV_transform_feedback, -1, STATE_INDEXED },
    { STATE_NAME(GL_TRANSFORM_FEEDBACK_BUFFER_START_NV), TYPE_5GLint, -1, BUGLE_GL_NV_transform_feedback, -1, STATE_INDEXED },
    { STATE_NAME(GL_TRANSFORM_FEEDBACK_BUFFER_SIZE_NV), TYPE_5GLint, -1, BUGLE_GL_NV_transform_feedback, -1, STATE_INDEXED },
#endif /* GL_NV_transform_feedback */
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
    shader_state,
    program_state,
    old_program_object_state,
    old_program_state,
    framebuffer_attachment_parameter_state,
    framebuffer_parameter_state,
    renderbuffer_parameter_state,
    transform_feedback_buffer_state,
    NULL
};

static void get_helper(const glstate *state,
                       GLdouble *d, GLfloat *f, GLint *i,
                       budgie_type *in_type,
                       void (BUDGIEAPI *get_double)(GLenum, GLenum, GLdouble *),
                       void (BUDGIEAPI *get_float)(GLenum, GLenum, GLfloat *),
                       void (BUDGIEAPI *get_int)(GLenum, GLenum, GLint *))
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

#ifdef GL_ARB_shader_objects
static void uniform_types(GLenum type,
                          budgie_type *in_type,
                          budgie_type *out_type,
                          int *length)
{
    switch (type)
    {
    case GL_FLOAT: *in_type = TYPE_7GLfloat; *out_type = TYPE_7GLfloat; *length = -1; break;
    case GL_FLOAT_VEC2_ARB: *in_type = TYPE_7GLfloat; *out_type = TYPE_7GLfloat; *length = 2; break;
    case GL_FLOAT_VEC3_ARB: *in_type = TYPE_7GLfloat; *out_type = TYPE_7GLfloat; *length = 3; break;
    case GL_FLOAT_VEC4_ARB: *in_type = TYPE_7GLfloat; *out_type = TYPE_7GLfloat; *length = 4; break;
    case GL_INT: *in_type = TYPE_5GLint; *out_type = TYPE_5GLint; *length = -1; break;
    case GL_INT_VEC2_ARB: *in_type = TYPE_5GLint; *out_type = TYPE_5GLint; *length = 2; break;
    case GL_INT_VEC3_ARB: *in_type = TYPE_5GLint; *out_type = TYPE_5GLint; *length = 3; break;
    case GL_INT_VEC4_ARB: *in_type = TYPE_5GLint; *out_type = TYPE_5GLint; *length = 4; break;
    case GL_BOOL_ARB: *in_type = TYPE_5GLint; *out_type = TYPE_9GLboolean; *length = -1; break;
    case GL_BOOL_VEC2_ARB: *in_type = TYPE_5GLint; *out_type = TYPE_9GLboolean; *length = 2; break;
    case GL_BOOL_VEC3_ARB: *in_type = TYPE_5GLint; *out_type = TYPE_9GLboolean; *length = 3; break;
    case GL_BOOL_VEC4_ARB: *in_type = TYPE_5GLint; *out_type = TYPE_9GLboolean; *length = 4; break;
    case GL_FLOAT_MAT2_ARB: *in_type = TYPE_7GLfloat; *out_type = TYPE_7GLfloat; *length = 4; break;
    case GL_FLOAT_MAT3_ARB: *in_type = TYPE_7GLfloat; *out_type = TYPE_7GLfloat; *length = 9; break;
    case GL_FLOAT_MAT4_ARB: *in_type = TYPE_7GLfloat; *out_type = TYPE_7GLfloat; *length = 16; break;
    /* It appears that glext.h version 21 doesn't define these */
#ifdef GL_SAMPLER_1D_ARB
    case GL_SAMPLER_1D_ARB:
    case GL_SAMPLER_2D_ARB:
    case GL_SAMPLER_3D_ARB:
    case GL_SAMPLER_CUBE_ARB:
    case GL_SAMPLER_1D_SHADOW_ARB:
    case GL_SAMPLER_2D_SHADOW_ARB:
    case GL_SAMPLER_2D_RECT_ARB:
    case GL_SAMPLER_2D_RECT_SHADOW_ARB:
#ifdef GL_EXT_texture_array
    case GL_SAMPLER_1D_ARRAY_EXT:
    case GL_SAMPLER_2D_ARRAY_EXT:
    case GL_SAMPLER_1D_ARRAY_SHADOW_EXT:
    case GL_SAMPLER_2D_ARRAY_SHADOW_EXT:
#endif
        *in_type = TYPE_5GLint; *out_type = TYPE_5GLint; *length = -1; break;
#endif /* GL_SAMPLER_1D_ARB */
#ifdef GL_VERSION_2_1
    case GL_FLOAT_MAT2x3: *in_type = TYPE_7GLfloat; *out_type = TYPE_7GLfloat; *length = 6; break;
    case GL_FLOAT_MAT2x4: *in_type = TYPE_7GLfloat; *out_type = TYPE_7GLfloat; *length = 8; break;
    case GL_FLOAT_MAT3x2: *in_type = TYPE_7GLfloat; *out_type = TYPE_7GLfloat; *length = 6; break;
    case GL_FLOAT_MAT3x4: *in_type = TYPE_7GLfloat; *out_type = TYPE_7GLfloat; *length = 12; break;
    case GL_FLOAT_MAT4x2: *in_type = TYPE_7GLfloat; *out_type = TYPE_7GLfloat; *length = 8; break;
    case GL_FLOAT_MAT4x3: *in_type = TYPE_7GLfloat; *out_type = TYPE_7GLfloat; *length = 12; break;
#endif
    default:
        abort();
    }
}
#endif /* GL_ARB_shader_objects */

void bugle_state_get_raw(const glstate *state, bugle_state_raw *wrapper)
{
    GLenum error;
    GLdouble d[16];
    GLfloat f[16];
    GLint i[16];
    GLuint ui[16];
    GLboolean b[16];
    GLvoid *p[16];
    GLpolygonstipple stipple;
    GLxfbattrib xfbattrib;
#ifdef GL_ARB_shader_objects
    GLhandleARB h[16];
    GLsizei length;
    GLint max_length;
#endif
    void *in;
    budgie_type in_type, out_type;
    int in_length;
    char *str = NULL;  /* Set to non-NULL for human-readable string output */

    GLint old_texture, old_buffer, old_program;
    GLint old_unit, old_client_unit, old_framebuffer, old_renderbuffer;
    bool flag_active_texture = false;
    GLenum pname;

    if (!state->info) return;
    if (state->info->pname) pname = state->info->pname;
    else if (state->enum_name) pname = state->enum_name;
    else pname = state->target;
    in_type = state->info->type;
    in_length = state->info->length;
    out_type = state->info->type;
    wrapper->data = NULL; /* Set to non-NULL if manual conversion */

    if ((error = CALL(glGetError)()) != GL_NO_ERROR)
    {
        const char *name;
        name = bugle_api_enum_name(error);
        if (name)
            bugle_log_printf("glstate", "get", BUGLE_LOG_WARNING,
                             "%s preceeding %s", name, state->name ? state->name : "root");
        else
            bugle_log_printf("glstate", "get", BUGLE_LOG_WARNING,
                             "OpenGL error %#08x preceeding %s",
                             (unsigned int) error, state->name ? state->name : "root");
        bugle_log_printf("glstate", "get", BUGLE_LOG_DEBUG,
                         "pname = %s target = %s binding = %s face = %s level = %d object = %u",
                         bugle_api_enum_name(pname),
                         bugle_api_enum_name(state->target),
                         bugle_api_enum_name(state->binding),
                         bugle_api_enum_name(state->face),
                         (int) state->level,
                         (unsigned int) state->object);
    }

#ifdef GL_ARB_multitexture
    if ((state->info->flags & STATE_MULTIPLEX_ACTIVE_TEXTURE)
        && bugle_gl_has_extension_group(BUGLE_GL_ARB_multitexture))
    {
        CALL(glGetIntegerv)(GL_ACTIVE_TEXTURE_ARB, &old_unit);
        CALL(glGetIntegerv)(GL_CLIENT_ACTIVE_TEXTURE_ARB, &old_client_unit);
#ifdef GL_VERSION_1_3
        if (bugle_gl_has_extension(BUGLE_GL_VERSION_1_3))
        {
            CALL(glActiveTexture)(state->unit);
            if (state->unit > get_texture_coord_units())
                CALL(glClientActiveTexture)(state->unit);
        }
        else
#endif
        {
            CALL(glActiveTextureARB)(state->unit);
            if (state->unit > get_texture_coord_units())
                CALL(glClientActiveTextureARB)(state->unit);
        }
        flag_active_texture = true;
    }
#endif /* GL_ARB_multitexture */
#ifdef GL_ARB_vertex_buffer_object
    if (state->info->flags & STATE_MULTIPLEX_BIND_BUFFER)
    {
        CALL(glGetIntegerv)(GL_ARRAY_BUFFER_BINDING_ARB, &old_buffer);
#ifdef GL_VERSION_1_5
        if (bugle_gl_has_extension(BUGLE_GL_VERSION_1_5))
        {
            CALL(glBindBuffer)(GL_ARRAY_BUFFER_ARB, state->object);
        }
        else
#endif
        {
            CALL(glBindBufferARB)(GL_ARRAY_BUFFER_ARB, state->object);
        }
    }
#endif /* GL_ARB_vertex_buffer_object */
#if defined(GL_ARB_vertex_program) || defined(GL_ARB_fragment_program)
    if (state->info->flags & STATE_MULTIPLEX_BIND_PROGRAM)
    {
        CALL(glGetProgramivARB)(state->target, GL_PROGRAM_BINDING_ARB, &old_program);
        CALL(glBindProgramARB)(state->target, state->object);
    }
#endif
#ifdef GL_EXT_framebuffer_object
    if ((state->info->flags & STATE_MULTIPLEX_BIND_FRAMEBUFFER)
        && bugle_gl_has_extension_group(BUGLE_GL_EXT_framebuffer_object))
    {
        CALL(glGetIntegerv)(state->binding, &old_framebuffer);
        CALL(glBindFramebufferEXT)(state->target, state->object);
    }
    if (state->info->flags & STATE_MULTIPLEX_BIND_RENDERBUFFER)
    {
        CALL(glGetIntegerv)(state->binding, &old_renderbuffer);
        CALL(glBindRenderbufferEXT)(state->target, state->object);
    }
#endif
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
            if (str) str = xstrdup(str);
            else str = xstrdup("(nil)");
        }
        else if (state->info->type == TYPE_9GLboolean)
            CALL(glGetBooleanv)(pname, b);
        else if (state->info->type == TYPE_P6GLvoid)
            CALL(glGetPointerv)(pname, p);
        else if (state->info->type == TYPE_8GLdouble)
            CALL(glGetDoublev)(pname, d);
        else if (state->info->type == TYPE_7GLfloat)
            CALL(glGetFloatv)(pname, f);
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
#ifdef GL_EXT_draw_buffers2
    case STATE_MODE_INDEXED:
        if (state->info->type == TYPE_9GLboolean)
            CALL(glGetBooleanIndexedvEXT)(pname, state->level, b);
        else if (state->info->type == TYPE_11GLxfbattrib)
            CALL(glGetIntegerIndexedvEXT)(pname, state->level, (GLint *) &xfbattrib);
        else
        {
            CALL(glGetIntegerIndexedvEXT)(pname, state->level, i);
            in_type = TYPE_5GLint;
        }
        break;
    case STATE_MODE_ENABLED_INDEXED:
        b[0] = CALL(glIsEnabledIndexedEXT)(pname, state->numeric_name);
        in_type = TYPE_9GLboolean;
        break;
#endif
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
#ifdef GL_ARB_vertex_program
    case STATE_MODE_VERTEX_ATTRIB:
        if (state->info->type == TYPE_P6GLvoid)
            CALL(glGetVertexAttribPointervARB)(state->object, pname, p);
        else if (state->info->type == TYPE_8GLdouble)
            CALL(glGetVertexAttribdvARB)(state->object, pname, d);
        else if (state->info->type == TYPE_7GLfloat)
            CALL(glGetVertexAttribfvARB)(state->object, pname, f);
        else
        {
            /* xorg-server 1.2.0 maps the iv and fv forms to the NV
             * variant, which does not support the two boolean queries
             * (_ENABLED and _NORMALIZED). We work around that by using
             * the dv form.
             */
            CALL(glGetVertexAttribdvARB)(state->object, pname, d);
            in_type = TYPE_8GLdouble;
        }
        break;
#endif
#ifdef GL_ARB_occlusion_query
    case STATE_MODE_QUERY:
        CALL(glGetQueryivARB)(state->target, pname, i);
        in_type = TYPE_5GLint;
        break;
    case STATE_MODE_QUERY_OBJECT:
        if (state->info->type == TYPE_6GLuint)
            CALL(glGetQueryObjectuivARB)(state->object, pname, ui);
        else
        {
            CALL(glGetQueryObjectivARB)(state->object, pname, i);
            in_type = TYPE_5GLint;
        }
        break;
#endif
#ifdef GL_ARB_vertex_buffer_object
    case STATE_MODE_BUFFER_PARAMETER:
        if (state->info->type == TYPE_P6GLvoid)
            CALL(glGetBufferPointervARB)(GL_ARRAY_BUFFER_ARB, pname, p);
        else
        {
            CALL(glGetBufferParameterivARB)(GL_ARRAY_BUFFER_ARB, pname, i);
            in_type = TYPE_5GLint;
        }
        break;
#endif
#ifdef GL_ARB_shader_objects
    case STATE_MODE_SHADER:
        if (state->info->type == TYPE_8GLdouble || state->info->type == TYPE_7GLfloat)
        {
            CALL(glGetObjectParameterfvARB)(state->object, pname, f);
            in_type = TYPE_7GLfloat;
        }
        else
        {
            bugle_glGetShaderiv(state->object, pname, i);
            in_type = TYPE_5GLint;
        }
        break;
    case STATE_MODE_PROGRAM:
        if (state->info->type == TYPE_8GLdouble || state->info->type == TYPE_7GLfloat)
        {
            CALL(glGetObjectParameterfvARB)(state->object, pname, f);
            in_type = TYPE_7GLfloat;
        }
        else
        {
            bugle_glGetProgramiv(state->object, pname, i);
            in_type = TYPE_5GLint;
        }
        break;
    case STATE_MODE_SHADER_INFO_LOG:
        max_length = 1;
        bugle_glGetShaderiv(state->object, GL_OBJECT_INFO_LOG_LENGTH_ARB, &max_length);
        str = XNMALLOC(max_length, GLcharARB);
        bugle_glGetShaderInfoLog(state->object, max_length, &length, (GLcharARB *) str);
        break;
    case STATE_MODE_PROGRAM_INFO_LOG:
        max_length = 1;
        bugle_glGetProgramiv(state->object, GL_OBJECT_INFO_LOG_LENGTH_ARB, &max_length);
        str = XNMALLOC(max_length, GLcharARB);
        bugle_glGetProgramInfoLog(state->object, max_length, &length, (GLcharARB *) str);
        break;
    case STATE_MODE_SHADER_SOURCE:
        max_length = 1;
        bugle_glGetShaderiv(state->object, GL_OBJECT_SHADER_SOURCE_LENGTH_ARB, &max_length);
        str = XNMALLOC(max_length, GLcharARB);
        bugle_glGetShaderSource(state->object, max_length, &length, (GLcharARB *) str);
        break;
    case STATE_MODE_UNIFORM:
        {
            GLsizei size;
            GLenum type;
            GLcharARB dummy[1];

            bugle_glGetActiveUniform(state->object, state->level, 1, NULL,
                                     &size, &type, dummy);
            uniform_types(type, &in_type, &out_type, &in_length);
            if (in_type == TYPE_7GLfloat)
                bugle_glGetUniformfv(state->object, state->level, f);
            else
                bugle_glGetUniformiv(state->object, state->level, i);
        }
        break;
    case STATE_MODE_ATTRIB_LOCATION:
        i[0] = bugle_glGetAttribLocation(state->object, state->name);
        break;
    case STATE_MODE_ATTACHED_SHADERS:
        {
            GLuint *attached;

            max_length = 1;
            bugle_glGetProgramiv(state->object, GL_OBJECT_ATTACHED_OBJECTS_ARB, &max_length);
            attached = XNMALLOC(max_length, GLuint);
            bugle_glGetAttachedShaders(state->object, max_length, NULL, attached);
            wrapper->data = attached;
            wrapper->length = max_length;
            wrapper->type = TYPE_6GLuint;
        }
        break;
#endif
#if defined(GL_ARB_vertex_program) || defined(GL_ARB_fragment_program)
    case STATE_MODE_OLD_PROGRAM:
        if (state->info->type == TYPE_PKc)
        {
            CALL(glGetProgramivARB)(state->target, GL_PROGRAM_LENGTH_ARB, i);
            str = XNMALLOC(i[0] + 1, char);
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
#ifdef GL_ARB_texture_compression
    case STATE_MODE_COMPRESSED_TEXTURE_FORMATS:
        {
            GLint count;
            GLint *formats;
            GLenum *out;

            CALL(glGetIntegerv)(GL_NUM_COMPRESSED_TEXTURE_FORMATS_ARB, &count);
            formats = XNMALLOC(count, GLint);
            out = XNMALLOC(count, GLenum);
            CALL(glGetIntegerv)(GL_COMPRESSED_TEXTURE_FORMATS_ARB, formats);
            budgie_type_convert(out, TYPE_6GLenum, formats, TYPE_5GLint, count);
            wrapper->data = out;
            wrapper->length = count;
            wrapper->type = TYPE_6GLenum;
            free(formats);
        }
        break;
#endif
#ifdef GL_EXT_framebuffer_object
    case STATE_MODE_FRAMEBUFFER_ATTACHMENT_PARAMETER:
        CALL(glGetFramebufferAttachmentParameterivEXT)(state->target,
                                                      state->level,
                                                      pname, i);
        in_type = TYPE_5GLint;
        break;
    case STATE_MODE_RENDERBUFFER_PARAMETER:
        CALL(glGetRenderbufferParameterivEXT)(state->target, pname, i);
        in_type = TYPE_5GLint;
        break;
#endif
    default:
        abort();
    }

#ifdef GL_ARB_multitexture
    if (flag_active_texture)
    {
#ifdef GL_VERSION_1_3
        if (bugle_gl_has_extension(BUGLE_GL_VERSION_1_3))
        {
            CALL(glActiveTexture)(old_unit);
            if (state->unit > get_texture_coord_units())
                CALL(glClientActiveTexture)(old_client_unit);
        }
        else
#endif
        {
            CALL(glActiveTextureARB)(old_unit);
            if (state->unit > get_texture_coord_units())
                CALL(glClientActiveTextureARB)(old_client_unit);
        }
    }
#endif /* GL_ARB_multitexture */
#ifdef GL_ARB_vertex_buffer_object
    if (state->info->flags & STATE_MULTIPLEX_BIND_BUFFER)
    {
#ifdef GL_VERSION_1_5
        if (bugle_gl_has_extension(BUGLE_GL_VERSION_1_5))
        {
            CALL(glBindBuffer)(GL_ARRAY_BUFFER, old_buffer);
        }
        else
#endif
        {
            CALL(glBindBufferARB)(GL_ARRAY_BUFFER_ARB, old_buffer);
        }
    }
#endif /* GL_ARB_vertex_buffer_object */
#if defined(GL_ARB_vertex_program) || defined(GL_ARB_fragment_program)
    if (state->info->flags & STATE_MULTIPLEX_BIND_PROGRAM)
        CALL(glBindProgramARB)(state->target, old_program);
#endif
#ifdef GL_EXT_framebuffer_object
    if ((state->info->flags & STATE_MULTIPLEX_BIND_FRAMEBUFFER)
        && bugle_gl_has_extension_group(GL_EXT_framebuffer_object))
        CALL(glBindFramebufferEXT)(state->target, old_framebuffer);
    if (state->info->flags & STATE_MULTIPLEX_BIND_RENDERBUFFER)
        CALL(glBindRenderbufferEXT)(state->target, old_renderbuffer);
#endif
    if ((state->info->flags & STATE_MULTIPLEX_BIND_TEXTURE)
        && state->binding)
        CALL(glBindTexture)(state->target, old_texture);

    if ((error = CALL(glGetError)()) != GL_NO_ERROR)
    {
        const char *name;
        name = bugle_api_enum_name(error);
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
#ifdef GL_ARB_shader_objects
        /* FIXME: we don't actually use this atm. We need some way to
         * call exactly one of glGetIntegerv on GL_CURRENT_PROGRAM_ARB
         * or glGetHandle on GL_PROGRAM_OBJECT_ARB. Currently there is
         * no indication of the current program if 2.0 is not available.
         */
        case TYPE_11GLhandleARB: in = h; break;
#endif
        case TYPE_9GLboolean: in = b; break;
        case TYPE_P6GLvoid: in = p; break;
        case TYPE_16GLpolygonstipple: in = stipple; break;
        case TYPE_11GLxfbattrib: in = &xfbattrib; break;
        default: abort();
        }

        wrapper->data = xnmalloc(abs(in_length), budgie_type_size(out_type));
        wrapper->type = out_type;
        wrapper->length = in_length;
        budgie_type_convert(wrapper->data, wrapper->type, in, in_type, abs(wrapper->length));
    }
}

static void dump_wrapper(char **buffer, size_t *size, void *data)
{
    const bugle_state_raw *w;
    int length;

    w = (const bugle_state_raw *) data;
    length = w->length;
    budgie_dump_any_type_extended(w->type, w->data, -1, length, NULL, buffer, size);
}

char *bugle_state_get_string(const glstate *state)
{
    bugle_state_raw wrapper;
    char *ans;

    if (!state->info) return NULL; /* root */
    bugle_state_get_raw(state, &wrapper);
    if (!wrapper.data)
        return "<GL error>";

    if (wrapper.type == TYPE_Pc)
        ans = xstrdup((const char *) wrapper.data); /* bugle_string_io(dump_string_wrapper, (char *) wrapper.data); */
    else
        ans = bugle_string_io(dump_wrapper, &wrapper);
    free(wrapper.data);
    return ans;
}

void bugle_state_get_children(const glstate *self, linked_list *children)
{
    if (self->spawn_children) (*self->spawn_children)(self, children);
    else bugle_list_init(children, free);
}

void bugle_state_clear(glstate *self)
{
    if (self->name) free(self->name);
}

static void spawn_children_query(const glstate *self, linked_list *children)
{
    bugle_list_init(children, free);
    make_leaves(self, query_state, children);
}

static void spawn_children_query_object(const glstate *self, linked_list *children)
{
    bugle_list_init(children, free);
    make_leaves(self, query_object_state, children);
}

static void spawn_children_buffer_parameter(const glstate *self, linked_list *children)
{
    bugle_list_init(children, free);
    make_leaves(self, buffer_parameter_state, children);
}

#if defined(GL_ARB_vertex_program) || defined(GL_ARB_fragment_program)
static void spawn_children_old_program_object(const glstate *self, linked_list *children)
{
    uint32_t mask = 0;
    GLint max_local, i;
    GLdouble local[4];
    static const state_info program_local_state =
    {
        NULL, GL_NONE, TYPE_8GLdouble, 4, BUGLE_EXTGROUP_old_program, -1, STATE_PROGRAM_LOCAL_PARAMETER
    };

    bugle_list_init(children, free);
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

            child = XMALLOC(glstate);
            *child = *self;
            child->level = i;
            child->info = &program_local_state;
            child->spawn_children = NULL;
            child->name = xasprintf("Local[%lu]", (unsigned long) i);
            child->numeric_name = i;
            child->enum_name = 0;
            bugle_list_append(children, child);
        }
    }
}

static void spawn_children_old_program(const glstate *self, linked_list *children)
{
    uint32_t mask = 0;
    GLint max_env, i;
    GLdouble env[4];
    static const state_info program_env_state =
    {
        NULL, GL_NONE, TYPE_8GLdouble, 4, BUGLE_EXTGROUP_old_program, -1, STATE_PROGRAM_ENV_PARAMETER
    };

    bugle_list_init(children, free);
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

            child = XMALLOC(glstate);
            *child = *self;
            child->level = i;
            child->info = &program_env_state;
            child->spawn_children = NULL;
            child->name = xasprintf("Env[%lu]", (unsigned long) i);
            child->numeric_name = i;
            child->enum_name = 0;
            bugle_list_append(children, child);
        }
    }
    make_objects(self, BUGLE_GLOBJECTS_OLD_PROGRAM, self->target, false,
                 "%lu", spawn_children_old_program_object, NULL, children);
}
#endif /* GL_ARB_vertex_program || GL_ARB_fragment_program */

#ifdef GL_EXT_framebuffer_object
static void spawn_children_framebuffer_attachment(const glstate *self, linked_list *children)
{
    bugle_list_init(children, free);
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

    CALL(glGetFramebufferAttachmentParameterivEXT)(self->target, attachment,
                                                  GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE_EXT,
                                                  &type);
    if (type != GL_NONE)
    {
        child = XMALLOC(glstate);
        *child = *self;
        if (index < 0) child->name = xstrdup(format);
        else child->name = xasprintf(format, index);
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

    bugle_list_init(children, free);
    CALL(glGetIntegerv)(self->binding, &old);
    CALL(glBindFramebufferEXT)(self->target, self->object);
    make_leaves(self, framebuffer_parameter_state, children);
    if (self->object != 0)
    {
        CALL(glGetIntegerv)(GL_MAX_COLOR_ATTACHMENTS_EXT, &attachments);
        for (i = 0; i < attachments; i++)
            make_framebuffer_attachment(self, GL_COLOR_ATTACHMENT0_EXT + i,
                                        "GL_COLOR_ATTACHMENT%ld",
                                        i, children);
        make_framebuffer_attachment(self, GL_DEPTH_ATTACHMENT_EXT,
                                    "GL_DEPTH_ATTACHMENT", -1, children);
        make_framebuffer_attachment(self, GL_STENCIL_ATTACHMENT_EXT,
                                    "GL_STENCIL_ATTACHMENT", -1, children);
    }

    CALL(glBindFramebufferEXT)(self->target, old);
}

static void spawn_children_framebuffer(const glstate *self, linked_list *children)
{
    bugle_list_init(children, free);
    make_objects(self, BUGLE_GLOBJECTS_FRAMEBUFFER, self->target, true,
                 "%lu", spawn_children_framebuffer_object, NULL, children);
}

static void spawn_children_renderbuffer_object(const glstate *self, linked_list *children)
{
    bugle_list_init(children, free);
    make_leaves(self, renderbuffer_parameter_state, children);
}

static void spawn_children_renderbuffer(const glstate *self, linked_list *children)
{
    bugle_list_init(children, free);
    make_objects(self, BUGLE_GLOBJECTS_RENDERBUFFER, self->target, false,
                 "%lu", spawn_children_renderbuffer_object, NULL, children);
}
#endif /* GL_EXT_framebuffer_object */

#ifdef GL_NV_transform_feedback
static void spawn_children_transform_feedback_buffer(const glstate *self, linked_list *children)
{
    bugle_list_init(children, free);
    make_leaves(self, transform_feedback_buffer_state, children);
}

static void spawn_children_transform_feedback(const glstate *self, linked_list *children)
{
    GLint buffers;

    glGetIntegerv(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS_NV, &buffers);
    bugle_list_init(children, free);
    make_counted(self, buffers, "%lu", 0, offsetof(glstate, level),
                 spawn_children_transform_feedback_buffer, NULL, children);
}
#endif /* GL_NV_transform_feedback */

static void spawn_children_global(const glstate *self, linked_list *children)
{
    static const state_info enable =
    {
        NULL, GL_NONE, TYPE_9GLboolean, -1, BUGLE_GL_VERSION_1_1, -1, STATE_ENABLED
    };

    const char *version;

    version = (const char *) CALL(glGetString)(GL_VERSION);
    bugle_list_init(children, free);
    make_leaves(self, global_state, children);

#ifdef GL_ARB_occlusion_query
    if (bugle_gl_has_extension_group(BUGLE_GL_ARB_occlusion_query))
    {
        make_fixed(self, query_enums, offsetof(glstate, target),
                   spawn_children_query, NULL, children);
        make_objects(self, BUGLE_GLOBJECTS_QUERY, GL_NONE, false,
                     "Query[%lu]", spawn_children_query_object, NULL, children);
    }
#endif
#ifdef GL_ARB_vertex_buffer_object
    if (bugle_gl_has_extension_group(BUGLE_GL_ARB_vertex_buffer_object))
    {
        make_objects(self, BUGLE_GLOBJECTS_BUFFER, GL_NONE, false,
                     "Buffer[%lu]", spawn_children_buffer_parameter, NULL, children);
    }
#endif
#ifdef GL_ARB_vertex_program
    if (bugle_gl_has_extension_group(BUGLE_GL_ARB_vertex_program))
        make_target(self, "GL_VERTEX_PROGRAM_ARB",
                    GL_VERTEX_PROGRAM_ARB,
                    0, spawn_children_old_program, &enable, children);
#endif
#ifdef GL_ARB_fragment_program
    if (bugle_gl_has_extension_group(BUGLE_GL_ARB_fragment_program))
        make_target(self, "GL_FRAGMENT_PROGRAM_ARB",
                    GL_FRAGMENT_PROGRAM_ARB,
                    0, spawn_children_old_program, &enable, children);
#endif
#ifdef GL_EXT_framebuffer_object
    if (bugle_gl_has_extension_group(BUGLE_GL_EXT_framebuffer_object))
    {
        make_target(self, "GL_FRAMEBUFFER_EXT",
                    GL_FRAMEBUFFER_EXT,
                    GL_FRAMEBUFFER_BINDING_EXT,
                    spawn_children_framebuffer, NULL, children);
        make_target(self, "GL_RENDERBUFFER_EXT",
                    GL_RENDERBUFFER_EXT,
                    GL_RENDERBUFFER_BINDING_EXT,
                    spawn_children_renderbuffer, NULL, children);
    }
#endif
#ifdef GL_NV_transform_feedback
    if (bugle_gl_has_extension_group(BUGLE_GL_NV_transform_feedback))
    {
        make_target(self, "TransformFeedbackBuffers",
                    0,
                    GL_TRANSFORM_FEEDBACK_BUFFER_BINDING_NV,
                    spawn_children_transform_feedback, NULL, children);
    }
#endif
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
