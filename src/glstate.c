/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004  Bruce Merry
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "src/utils.h"
#include "src/glstate.h"
#include "src/glutils.h"
#include "src/glexts.h"
#include "src/tracker.h"
#include "common/safemem.h"
#include "budgielib/budgieutils.h"
#include <GL/gl.h>
#include <GL/glext.h>
#include <assert.h>

#define STATE_NAME(x) #x, x
#define STATE_NAME_EXT(x, ext) #x, x ## ext

// bottom 8 bits are fetch function, rest are true flags
#define STATE_MODE_GLOBAL             0x0001    // glGet*
#define STATE_MODE_ENABLE             0x0002    // glIsEnabled
#define STATE_MODE_TEXENV             0x0003    // glGetTexEnv
#define STATE_MODE_TEXPARAMETER       0x0004    // glGetTexParameter
#define STATE_MODE_TEXLEVELPARAMETER  0x0005    // glGetTexLevelParameter
#define STATE_MODE_TEXGEN             0x0006    // glGetTexGen
#define STATE_MODE_LIGHT              0x0007    // glGetLight
#define STATE_MODE_MATERIAL           0x0008    // glGetMaterial

#define STATE_MODE_MASK               0x00ff

#define STATE_FLAG_TEXUNIT            0x0100    // Set active texture
#define STATE_FLAG_TEXTARGET          0x0200    // Set current texture object
#define STATE_FLAG_FILTERCONTROL      0x0400    // Pass GL_TEXTURE_FILTER_CONTROL

// conveniences
#define STATE_GLOBAL STATE_MODE_GLOBAL
#define STATE_ENABLE STATE_MODE_ENABLE
#define STATE_TEXENV (STATE_MODE_TEXENV | STATE_FLAG_TEXUNIT)
#define STATE_TEXUNIT (STATE_MODE_GLOBAL | STATE_FLAG_TEXUNIT)
#define STATE_TEXUNIT_ENABLE (STATE_MODE_ENABLE | STATE_FLAG_TEXUNIT)
#define STATE_TEXPARAMETER (STATE_MODE_TEXPARAMETER | STATE_FLAG_TEXTARGET)
#define STATE_TEXLEVELPARAMETER (STATE_MODE_TEXLEVELPARAMETER | STATE_FLAG_TEXTARGET)
#define STATE_TEXGEN (STATE_MODE_TEXGEN | STATE_FLAG_TEXUNIT)
#define STATE_LIGHT STATE_MODE_LIGHT
#define STATE_MATERIAL STATE_MODE_MATERIAL

static const state_info global_state[] =
{
    { STATE_NAME(GL_NORMALIZE), TYPE_9GLboolean, 1, -1, "1.1", STATE_ENABLE },
#ifdef GL_EXT_rescale_normal
    { STATE_NAME_EXT(GL_RESCALE_NORMAL, _EXT), TYPE_9GLboolean, 1, BUGLE_GL_EXT_rescale_normal, "1.2", STATE_ENABLE },
#endif
    { STATE_NAME(GL_FOG), TYPE_9GLboolean, 1, -1, "1.1", STATE_ENABLE },
#ifdef GL_EXT_secondary_color
    { STATE_NAME_EXT(GL_COLOR_SUM, _EXT), TYPE_9GLboolean, 1, BUGLE_GL_EXT_secondary_color, "1.4", STATE_ENABLE },
#endif
    { STATE_NAME(GL_LIGHTING), TYPE_9GLboolean, 1, -1, "1.1", STATE_ENABLE },
    { STATE_NAME(GL_COLOR_MATERIAL), TYPE_9GLboolean, 1, -1, "1.1", STATE_ENABLE },
    { STATE_NAME(GL_POINT_SMOOTH), TYPE_9GLboolean, 1, -1, "1.1", STATE_ENABLE },
    { STATE_NAME(GL_LINE_SMOOTH), TYPE_9GLboolean, 1, -1, "1.1", STATE_ENABLE },
    { STATE_NAME(GL_LINE_STIPPLE), TYPE_9GLboolean, 1, -1, "1.1", STATE_ENABLE },
    { STATE_NAME(GL_CULL_FACE), TYPE_9GLboolean, 1, -1, "1.1", STATE_ENABLE },
    { STATE_NAME(GL_POLYGON_SMOOTH), TYPE_9GLboolean, 1, -1, "1.1", STATE_ENABLE },
    { STATE_NAME(GL_POLYGON_OFFSET_POINT), TYPE_9GLboolean, 1, -1, "1.1", STATE_ENABLE },
    { STATE_NAME(GL_POLYGON_OFFSET_LINE), TYPE_9GLboolean, 1, -1, "1.1", STATE_ENABLE },
    { STATE_NAME(GL_POLYGON_OFFSET_FILL), TYPE_9GLboolean, 1, -1, "1.1", STATE_ENABLE },
    { STATE_NAME(GL_POLYGON_STIPPLE), TYPE_9GLboolean, 1, -1, "1.1", STATE_ENABLE },
#ifdef GL_ARB_multisample
    { STATE_NAME_EXT(GL_MULTISAMPLE, _ARB), TYPE_9GLboolean, 1, BUGLE_GL_ARB_multisample, "1.3", STATE_ENABLE },
    { STATE_NAME_EXT(GL_SAMPLE_ALPHA_TO_COVERAGE, _ARB), TYPE_9GLboolean, 1, BUGLE_GL_ARB_multisample, "1.3", STATE_ENABLE },
    { STATE_NAME_EXT(GL_SAMPLE_ALPHA_TO_ONE, _ARB), TYPE_9GLboolean, 1, BUGLE_GL_ARB_multisample, "1.3", STATE_ENABLE },
    { STATE_NAME_EXT(GL_SAMPLE_COVERAGE, _ARB), TYPE_9GLboolean, 1, BUGLE_GL_ARB_multisample, "1.3", STATE_ENABLE },
#endif
    { STATE_NAME(GL_SCISSOR_TEST), TYPE_9GLboolean, 1, -1, "1.1", STATE_ENABLE },
    { STATE_NAME(GL_ALPHA_TEST), TYPE_9GLboolean, 1, -1, "1.1", STATE_ENABLE },
    { STATE_NAME(GL_STENCIL_TEST), TYPE_9GLboolean, 1, -1, "1.1", STATE_ENABLE },
    { STATE_NAME(GL_DEPTH_TEST), TYPE_9GLboolean, 1, -1, "1.1", STATE_ENABLE },
    { STATE_NAME(GL_BLEND), TYPE_9GLboolean, 1, -1, "1.1", STATE_ENABLE },
    { STATE_NAME(GL_DITHER), TYPE_9GLboolean, 1, -1, "1.1", STATE_ENABLE },
    { STATE_NAME(GL_INDEX_LOGIC_OP), TYPE_9GLboolean, 1, -1, "1.1", STATE_ENABLE },
    { STATE_NAME(GL_COLOR_LOGIC_OP), TYPE_9GLboolean, 1, -1, "1.1", STATE_ENABLE },
    { STATE_NAME(GL_AUTO_NORMAL), TYPE_9GLboolean, 1, -1, "1.1", STATE_ENABLE },

    { STATE_NAME(GL_MAX_LIGHTS), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_MAX_CLIP_PLANES), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_MAX_COLOR_MATRIX_STACK_DEPTH), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_MAX_MODELVIEW_STACK_DEPTH), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_MAX_PROJECTION_STACK_DEPTH), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_MAX_TEXTURE_STACK_DEPTH), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_SUBPIXEL_BITS), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
#ifdef GL_EXT_texture3D
    { STATE_NAME_EXT(GL_MAX_3D_TEXTURE_SIZE, _EXT), TYPE_5GLint, 1, BUGLE_GL_EXT_texture3D, "1.2", STATE_GLOBAL },
#endif
    { STATE_NAME(GL_MAX_TEXTURE_SIZE), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
#ifdef GL_EXT_texture_lod_bias
    { STATE_NAME_EXT(GL_MAX_TEXTURE_LOD_BIAS, _EXT), TYPE_8GLdouble, 1, BUGLE_GL_EXT_texture_lod_bias, "1.4", STATE_GLOBAL },
#endif
#ifdef GL_ARB_texture_cube_map
    { STATE_NAME_EXT(GL_MAX_CUBE_MAP_TEXTURE_SIZE, _ARB), TYPE_5GLint, 1, BUGLE_GL_ARB_texture_cube_map, "1.3", STATE_GLOBAL },
#endif
    { STATE_NAME(GL_MAX_PIXEL_MAP_TABLE), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_MAX_NAME_STACK_DEPTH), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_MAX_LIST_NESTING), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_MAX_EVAL_ORDER), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_MAX_VIEWPORT_DIMS), TYPE_5GLint, 2, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_MAX_ATTRIB_STACK_DEPTH), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_MAX_CLIENT_ATTRIB_STACK_DEPTH), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_AUX_BUFFERS), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_RGBA_MODE), TYPE_9GLboolean, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_INDEX_MODE), TYPE_9GLboolean, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_DOUBLEBUFFER), TYPE_9GLboolean, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_STEREO), TYPE_9GLboolean, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_ALIASED_POINT_SIZE_RANGE), TYPE_8GLdouble, 2, -1, "1.2", STATE_GLOBAL },
    { STATE_NAME(GL_SMOOTH_POINT_SIZE_RANGE), TYPE_8GLdouble, 2, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_SMOOTH_POINT_SIZE_GRANULARITY), TYPE_8GLdouble, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_ALIASED_LINE_WIDTH_RANGE), TYPE_8GLdouble, 2, -1, "1.2", STATE_GLOBAL },
    { STATE_NAME(GL_SMOOTH_LINE_WIDTH_RANGE), TYPE_8GLdouble, 2, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_SMOOTH_LINE_WIDTH_GRANULARITY), TYPE_8GLdouble, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_MAX_ELEMENTS_INDICES), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_MAX_ELEMENTS_VERTICES), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
#ifdef GL_ARB_multitexture
    { STATE_NAME_EXT(GL_MAX_TEXTURE_UNITS, _ARB), TYPE_5GLint, 1, BUGLE_GL_ARB_multitexture, "1.3", STATE_GLOBAL },
#endif
#ifdef GL_ARB_multisample
    { STATE_NAME_EXT(GL_SAMPLE_BUFFERS, _ARB), TYPE_5GLint, 1, BUGLE_GL_ARB_multisample, "1.3", STATE_GLOBAL },
    { STATE_NAME_EXT(GL_SAMPLES, _ARB), TYPE_5GLint, 1, BUGLE_GL_ARB_multisample, "1.3", STATE_GLOBAL },
#endif
#ifdef GL_ARB_texture_compression
    { STATE_NAME_EXT(GL_NUM_COMPRESSED_TEXTURE_FORMATS, _ARB), TYPE_5GLint, 1, BUGLE_GL_ARB_texture_compression, "1.3", STATE_GLOBAL },
#endif
    { STATE_NAME(GL_RED_BITS), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_GREEN_BITS), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_BLUE_BITS), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_ALPHA_BITS), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_DEPTH_BITS), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_BITS), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_ACCUM_RED_BITS), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_ACCUM_GREEN_BITS), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_ACCUM_BLUE_BITS), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_ACCUM_ALPHA_BITS), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_CURRENT_COLOR), TYPE_8GLdouble, 4, -1, "1.1", STATE_GLOBAL },
#ifdef GL_EXT_secondary_color
    { STATE_NAME_EXT(GL_CURRENT_SECONDARY_COLOR, _EXT), TYPE_8GLdouble, 4, BUGLE_GL_EXT_secondary_color, "1.4", STATE_GLOBAL },
#endif
    { STATE_NAME(GL_CURRENT_INDEX), TYPE_8GLdouble, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_CURRENT_NORMAL), TYPE_8GLdouble, 3, -1, "1.1", STATE_GLOBAL },
#ifdef GL_EXT_fog_coord
    { STATE_NAME_EXT(GL_CURRENT_FOG_COORDINATE, _EXT), TYPE_8GLdouble, 1, BUGLE_GL_EXT_fog_coord, "1.4", STATE_GLOBAL },
#endif
    { STATE_NAME(GL_CURRENT_RASTER_POSITION), TYPE_8GLdouble, 4, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_CURRENT_RASTER_DISTANCE), TYPE_8GLdouble, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_CURRENT_RASTER_COLOR), TYPE_8GLdouble, 4, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_CURRENT_RASTER_INDEX), TYPE_8GLdouble, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_CURRENT_RASTER_POSITION_VALID), TYPE_9GLboolean, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_EDGE_FLAG), TYPE_9GLboolean, 1, -1, "1.1", STATE_GLOBAL },
#ifdef GL_ARB_multitexture
    { STATE_NAME_EXT(GL_CLIENT_ACTIVE_TEXTURE, _ARB), TYPE_6GLenum, 1, BUGLE_GL_ARB_multitexture, "1.3", STATE_GLOBAL },
#endif
#ifdef GL_ARB_vertex_buffer_object
    { STATE_NAME_EXT(GL_ARRAY_BUFFER_BINDING, _ARB), TYPE_5GLint, 1, BUGLE_GL_ARB_vertex_buffer_object, "1.5", STATE_GLOBAL },
    { STATE_NAME_EXT(GL_ELEMENT_ARRAY_BUFFER_BINDING, _ARB), TYPE_5GLint, 1, BUGLE_GL_ARB_vertex_buffer_object, "1.5", STATE_GLOBAL },
#endif
    { STATE_NAME(GL_COLOR_MATRIX), TYPE_8GLdouble, 16, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_MODELVIEW_MATRIX), TYPE_8GLdouble, 16, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_PROJECTION_MATRIX), TYPE_8GLdouble, 16, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_VIEWPORT), TYPE_5GLint, 4, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_DEPTH_RANGE), TYPE_8GLdouble, 2, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_COLOR_MATRIX_STACK_DEPTH), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_MODELVIEW_STACK_DEPTH), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_PROJECTION_STACK_DEPTH), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_MATRIX_MODE), TYPE_6GLenum, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_FOG_COLOR), TYPE_8GLdouble, 4, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_FOG_INDEX), TYPE_8GLdouble, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_FOG_DENSITY), TYPE_8GLdouble, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_FOG_START), TYPE_8GLdouble, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_FOG_END), TYPE_8GLdouble, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_FOG_MODE), TYPE_6GLenum, 1, -1, "1.1", STATE_GLOBAL },
#ifdef GL_EXT_fog_coord
    { STATE_NAME_EXT(GL_FOG_COORDINATE_SOURCE, _EXT), TYPE_6GLenum, 1, BUGLE_GL_EXT_fog_coord, "1.4", STATE_GLOBAL },
#endif
    { STATE_NAME(GL_SHADE_MODEL), TYPE_6GLenum, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_COLOR_MATERIAL_PARAMETER), TYPE_6GLenum, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_COLOR_MATERIAL_FACE), TYPE_6GLenum, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_LIGHT_MODEL_AMBIENT), TYPE_8GLdouble, 4, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_LIGHT_MODEL_LOCAL_VIEWER), TYPE_9GLboolean, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_LIGHT_MODEL_TWO_SIDE), TYPE_9GLboolean, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_LIGHT_MODEL_COLOR_CONTROL), TYPE_6GLenum, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_POINT_SIZE), TYPE_8GLdouble, 1, -1, "1.1", STATE_GLOBAL },
#ifdef GL_ARB_point_parameters
    { STATE_NAME_EXT(GL_POINT_SIZE_MIN, _ARB), TYPE_8GLdouble, 1, BUGLE_GL_ARB_point_parameters, "1.4", STATE_GLOBAL },
    { STATE_NAME_EXT(GL_POINT_SIZE_MAX, _ARB), TYPE_8GLdouble, 1, BUGLE_GL_ARB_point_parameters, "1.4", STATE_GLOBAL },
    { STATE_NAME_EXT(GL_POINT_FADE_THRESHOLD_SIZE, _ARB), TYPE_8GLdouble, 1, BUGLE_GL_ARB_point_parameters, "1.4", STATE_GLOBAL },
    { STATE_NAME_EXT(GL_POINT_DISTANCE_ATTENUATION, _ARB), TYPE_8GLdouble, 3, BUGLE_GL_ARB_point_parameters, "1.4", STATE_GLOBAL },
#endif
    { STATE_NAME(GL_LINE_WIDTH), TYPE_8GLdouble, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_LINE_STIPPLE_PATTERN), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_LINE_STIPPLE_REPEAT), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_CULL_FACE_MODE), TYPE_6GLenum, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_FRONT_FACE), TYPE_6GLenum, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_POLYGON_MODE), TYPE_6GLenum, 2, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_POLYGON_OFFSET_FACTOR), TYPE_8GLdouble, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_POLYGON_OFFSET_UNITS), TYPE_8GLdouble, 1, -1, "1.1", STATE_GLOBAL },
#ifdef GL_ARB_multisample
    { STATE_NAME_EXT(GL_SAMPLE_COVERAGE_VALUE, _ARB), TYPE_8GLdouble, 1, BUGLE_GL_ARB_multisample, "1.3", STATE_GLOBAL },
    { STATE_NAME_EXT(GL_SAMPLE_COVERAGE_INVERT, _ARB), TYPE_9GLboolean, 1, BUGLE_GL_ARB_multisample, "1.3", STATE_GLOBAL },
#endif
#ifdef GL_ARB_multitexture
    { STATE_NAME_EXT(GL_ACTIVE_TEXTURE, _ARB), TYPE_6GLenum, 1, BUGLE_GL_ARB_multitexture, "1.3", STATE_GLOBAL },
#endif
    { STATE_NAME(GL_SCISSOR_BOX), TYPE_5GLint, 4, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_ALPHA_TEST_FUNC), TYPE_6GLenum, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_FUNC), TYPE_6GLenum, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_VALUE_MASK), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_REF), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_FAIL), TYPE_6GLenum, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_PASS_DEPTH_FAIL), TYPE_6GLenum, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_PASS_DEPTH_PASS), TYPE_6GLenum, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_DEPTH_FUNC), TYPE_6GLenum, 1, -1, "1.1", STATE_GLOBAL },
#ifdef GL_EXT_blend_func_separate
    { STATE_NAME_EXT(GL_BLEND_SRC_RGB, _EXT), TYPE_15GLalternateenum, 1, BUGLE_GL_EXT_blend_func_separate, "1.4", STATE_GLOBAL },
    { STATE_NAME_EXT(GL_BLEND_SRC_ALPHA, _EXT), TYPE_15GLalternateenum, 1, BUGLE_GL_EXT_blend_func_separate, "1.4", STATE_GLOBAL },
    { STATE_NAME_EXT(GL_BLEND_DST_RGB, _EXT), TYPE_15GLalternateenum, 1, BUGLE_GL_EXT_blend_func_separate, "1.4", STATE_GLOBAL },
    { STATE_NAME_EXT(GL_BLEND_DST_ALPHA, _EXT), TYPE_15GLalternateenum, 1, BUGLE_GL_EXT_blend_func_separate, "1.4", STATE_GLOBAL },
#endif
    { STATE_NAME(GL_BLEND_EQUATION), TYPE_6GLenum, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_BLEND_COLOR), TYPE_8GLdouble, 4, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_LOGIC_OP_MODE), TYPE_6GLenum, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_DRAW_BUFFER), TYPE_6GLenum, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_INDEX_WRITEMASK), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_COLOR_WRITEMASK), TYPE_9GLboolean, 4, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_DEPTH_WRITEMASK), TYPE_9GLboolean, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_WRITEMASK), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_COLOR_CLEAR_VALUE), TYPE_8GLdouble, 4, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_INDEX_CLEAR_VALUE), TYPE_8GLdouble, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_DEPTH_CLEAR_VALUE), TYPE_8GLdouble, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_STENCIL_CLEAR_VALUE), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_UNPACK_SWAP_BYTES), TYPE_9GLboolean, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_UNPACK_LSB_FIRST), TYPE_9GLboolean, 1, -1, "1.1", STATE_GLOBAL },
#ifdef GL_EXT_texture3D
    { STATE_NAME_EXT(GL_UNPACK_IMAGE_HEIGHT, _EXT), TYPE_5GLint, 1, BUGLE_GL_EXT_texture3D, "1.2", STATE_GLOBAL },
    { STATE_NAME_EXT(GL_UNPACK_SKIP_IMAGES, _EXT), TYPE_5GLint, 1, BUGLE_GL_EXT_texture3D, "1.2", STATE_GLOBAL },
#endif
    { STATE_NAME(GL_UNPACK_ROW_LENGTH), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_UNPACK_SKIP_ROWS), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_UNPACK_SKIP_PIXELS), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_UNPACK_ALIGNMENT), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_PACK_SWAP_BYTES), TYPE_9GLboolean, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_PACK_LSB_FIRST), TYPE_9GLboolean, 1, -1, "1.1", STATE_GLOBAL },
#ifdef GL_EXT_texture3D
    { STATE_NAME_EXT(GL_PACK_IMAGE_HEIGHT, _EXT), TYPE_5GLint, 1, BUGLE_GL_EXT_texture3D, "1.2", STATE_GLOBAL },
    { STATE_NAME_EXT(GL_PACK_SKIP_IMAGES, _EXT), TYPE_5GLint, 1, BUGLE_GL_EXT_texture3D, "1.2", STATE_GLOBAL },
#endif
    { STATE_NAME(GL_PACK_ROW_LENGTH), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_PACK_SKIP_ROWS), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_PACK_SKIP_PIXELS), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_PACK_ALIGNMENT), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_MAP_COLOR), TYPE_9GLboolean, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_MAP_STENCIL), TYPE_9GLboolean, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_INDEX_SHIFT), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_INDEX_OFFSET), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_RED_SCALE), TYPE_8GLdouble, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_GREEN_SCALE), TYPE_8GLdouble, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_BLUE_SCALE), TYPE_8GLdouble, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_ALPHA_SCALE), TYPE_8GLdouble, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_DEPTH_SCALE), TYPE_8GLdouble, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_RED_BIAS), TYPE_8GLdouble, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_GREEN_BIAS), TYPE_8GLdouble, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_BLUE_BIAS), TYPE_8GLdouble, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_ALPHA_BIAS), TYPE_8GLdouble, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_DEPTH_BIAS), TYPE_8GLdouble, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_ZOOM_X), TYPE_8GLdouble, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_ZOOM_Y), TYPE_8GLdouble, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_READ_BUFFER), TYPE_6GLenum, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_MAP1_GRID_DOMAIN), TYPE_8GLdouble, 2, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_MAP2_GRID_DOMAIN), TYPE_8GLdouble, 4, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_MAP1_GRID_SEGMENTS), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_MAP2_GRID_SEGMENTS), TYPE_5GLint, 2, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_PERSPECTIVE_CORRECTION_HINT), TYPE_6GLenum, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_POINT_SMOOTH_HINT), TYPE_6GLenum, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_LINE_SMOOTH_HINT), TYPE_6GLenum, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_POLYGON_SMOOTH_HINT), TYPE_6GLenum, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_FOG_HINT), TYPE_6GLenum, 1, -1, "1.1", STATE_GLOBAL },
#ifdef GL_SGIS_generate_mipmap
    { STATE_NAME_EXT(GL_GENERATE_MIPMAP_HINT, _SGIS), TYPE_6GLenum, 1, BUGLE_GL_SGIS_generate_mipmap, "1.4", STATE_GLOBAL },
#endif
#ifdef GL_ARB_texture_compression
    { STATE_NAME_EXT(GL_TEXTURE_COMPRESSION_HINT, _ARB), TYPE_6GLenum, 1, BUGLE_GL_ARB_texture_compression, "1.3", STATE_GLOBAL },
#endif
    { STATE_NAME(GL_LIST_BASE), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_LIST_INDEX), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_LIST_MODE), TYPE_6GLenum, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_ATTRIB_STACK_DEPTH), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_CLIENT_ATTRIB_STACK_DEPTH), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_NAME_STACK_DEPTH), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_RENDER_MODE), TYPE_6GLenum, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_SELECTION_BUFFER_POINTER), TYPE_P6GLvoid, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_SELECTION_BUFFER_SIZE), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_FEEDBACK_BUFFER_POINTER), TYPE_P6GLvoid, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_FEEDBACK_BUFFER_SIZE), TYPE_5GLint, 1, -1, "1.1", STATE_GLOBAL },
    { STATE_NAME(GL_FEEDBACK_BUFFER_TYPE), TYPE_6GLenum, 1, -1, "1.1", STATE_GLOBAL },
    { NULL, GL_NONE, NULL_TYPE, 0, 0, NULL, 0 }
};

static const state_info texture_object_state[] =
{
    { STATE_NAME(GL_TEXTURE_BORDER_COLOR), TYPE_8GLdouble, 4, -1, "1.1", STATE_TEXPARAMETER },
    { STATE_NAME(GL_TEXTURE_MIN_FILTER), TYPE_6GLenum, 1, -1, "1.1", STATE_TEXPARAMETER },
    { STATE_NAME(GL_TEXTURE_MAG_FILTER), TYPE_6GLenum, 1, -1, "1.1", STATE_TEXPARAMETER },
    { STATE_NAME(GL_TEXTURE_WRAP_S), TYPE_6GLenum, 1, -1, "1.1", STATE_TEXPARAMETER },
    { STATE_NAME(GL_TEXTURE_WRAP_T), TYPE_6GLenum, 1, -1, "1.1", STATE_TEXPARAMETER },
    { STATE_NAME(GL_TEXTURE_WRAP_R), TYPE_6GLenum, 1, -1, "1.1", STATE_TEXPARAMETER },
    { STATE_NAME(GL_TEXTURE_PRIORITY), TYPE_8GLdouble, 1, -1, "1.1", STATE_TEXPARAMETER },
    { STATE_NAME(GL_TEXTURE_RESIDENT), TYPE_9GLboolean, 1, -1, "1.1", STATE_TEXPARAMETER },
    { STATE_NAME(GL_TEXTURE_MIN_LOD), TYPE_8GLdouble, 1, -1, "1.1", STATE_TEXPARAMETER },
    { STATE_NAME(GL_TEXTURE_MAX_LOD), TYPE_8GLdouble, 1, -1, "1.1", STATE_TEXPARAMETER },
    { STATE_NAME(GL_TEXTURE_BASE_LEVEL), TYPE_5GLint, 1, -1, "1.1", STATE_TEXPARAMETER },
    { STATE_NAME(GL_TEXTURE_MAX_LEVEL), TYPE_5GLint, 1, -1, "1.1", STATE_TEXPARAMETER },
#ifdef GL_EXT_texture_lod_bias
    { STATE_NAME_EXT(GL_TEXTURE_LOD_BIAS, _EXT), TYPE_8GLdouble, 1, BUGLE_GL_EXT_texture_lod_bias, "1.4", STATE_TEXPARAMETER },
#endif
#ifdef GL_ARB_depth_texture
    { STATE_NAME_EXT(GL_DEPTH_TEXTURE_MODE, _ARB), TYPE_6GLenum, 1, BUGLE_GL_ARB_depth_texture, "1.4", STATE_TEXPARAMETER },
#endif
#ifdef GL_ARB_shadow
    { STATE_NAME_EXT(GL_TEXTURE_COMPARE_MODE, _ARB), TYPE_6GLenum, 1, BUGLE_GL_ARB_shadow, "1.4", STATE_TEXPARAMETER },
    { STATE_NAME_EXT(GL_TEXTURE_COMPARE_FUNC, _ARB), TYPE_6GLenum, 1, BUGLE_GL_ARB_shadow, "1.4", STATE_TEXPARAMETER },
#endif
#ifdef GL_SGIS_generate_mipmap
    { STATE_NAME_EXT(GL_GENERATE_MIPMAP, _SGIS), TYPE_9GLboolean, 1, BUGLE_GL_SGIS_generate_mipmap, "1.4", STATE_TEXPARAMETER },
#endif
    { NULL, GL_NONE, NULL_TYPE, 0, 0, NULL, 0 }
};

static const state_info texture_level_state[] =
{
    { STATE_NAME(GL_TEXTURE_WIDTH), TYPE_5GLint, 1, -1, "1.1", STATE_TEXLEVELPARAMETER },
    { STATE_NAME(GL_TEXTURE_HEIGHT), TYPE_5GLint, 1, -1, "1.1", STATE_TEXLEVELPARAMETER },
#ifdef GL_EXT_texture3D
    { STATE_NAME_EXT(GL_TEXTURE_DEPTH, _EXT), TYPE_5GLint, 1, BUGLE_GL_EXT_texture3D, "1.2", STATE_TEXLEVELPARAMETER },
#endif
    { STATE_NAME(GL_TEXTURE_BORDER), TYPE_5GLint, 1, -1, "1.1", STATE_TEXLEVELPARAMETER },
    { STATE_NAME(GL_TEXTURE_INTERNAL_FORMAT), TYPE_16GLcomponentsenum, 1, -1, "1.1", STATE_TEXLEVELPARAMETER },
    { STATE_NAME(GL_TEXTURE_RED_SIZE), TYPE_5GLint, 1, -1, "1.1", STATE_TEXLEVELPARAMETER },
    { STATE_NAME(GL_TEXTURE_GREEN_SIZE), TYPE_5GLint, 1, -1, "1.1", STATE_TEXLEVELPARAMETER },
    { STATE_NAME(GL_TEXTURE_BLUE_SIZE), TYPE_5GLint, 1, -1, "1.1", STATE_TEXLEVELPARAMETER },
    { STATE_NAME(GL_TEXTURE_ALPHA_SIZE), TYPE_5GLint, 1, -1, "1.1", STATE_TEXLEVELPARAMETER },
    { STATE_NAME(GL_TEXTURE_LUMINANCE_SIZE), TYPE_5GLint, 1, -1, "1.1", STATE_TEXLEVELPARAMETER },
    { STATE_NAME(GL_TEXTURE_INTENSITY_SIZE), TYPE_5GLint, 1, -1, "1.1", STATE_TEXLEVELPARAMETER },
#ifdef GL_ARB_depth_texture
    { STATE_NAME_EXT(GL_TEXTURE_DEPTH_SIZE, _ARB), TYPE_5GLint, 1, BUGLE_GL_ARB_depth_texture, "1.4", STATE_TEXLEVELPARAMETER },
#endif
#ifdef GL_ARB_texture_compression
    { STATE_NAME_EXT(GL_TEXTURE_COMPRESSED, _ARB), TYPE_9GLboolean, 1, BUGLE_GL_ARB_texture_compression, "1.3", STATE_TEXLEVELPARAMETER },
#endif
    { NULL, GL_NONE, NULL_TYPE, 0, 0, NULL, 0 }
};

static const state_info texture_unit_state[] =
{
    { STATE_NAME(GL_TEXTURE_ENV_MODE), TYPE_6GLenum, 1, -1, "1.1", STATE_TEXENV },
    { STATE_NAME(GL_TEXTURE_ENV_COLOR), TYPE_8GLdouble, 4, -1, "1.1", STATE_TEXENV },
#ifdef GL_EXT_texture_lod_bias
    { STATE_NAME_EXT(GL_TEXTURE_LOD_BIAS, _EXT), TYPE_8GLdouble, 1, BUGLE_GL_EXT_texture_lod_bias, "1.4", STATE_TEXENV | STATE_FLAG_FILTERCONTROL },
#endif
#ifdef GL_EXT_texture_env_combine
    { STATE_NAME_EXT(GL_COMBINE_RGB, _ARB), TYPE_6GLenum, 1, BUGLE_GL_ARB_texture_env_combine, "1.3", STATE_TEXENV },
    { STATE_NAME_EXT(GL_COMBINE_ALPHA, _ARB), TYPE_6GLenum, 1, BUGLE_GL_ARB_texture_env_combine, "1.3", STATE_TEXENV },
    { STATE_NAME_EXT(GL_SOURCE0_RGB, _ARB), TYPE_6GLenum, 1, BUGLE_GL_ARB_texture_env_combine, "1.3", STATE_TEXENV },
    { STATE_NAME_EXT(GL_SOURCE1_RGB, _ARB), TYPE_6GLenum, 1, BUGLE_GL_ARB_texture_env_combine, "1.3", STATE_TEXENV },
    { STATE_NAME_EXT(GL_SOURCE2_RGB, _ARB), TYPE_6GLenum, 1, BUGLE_GL_ARB_texture_env_combine, "1.3", STATE_TEXENV },
    { STATE_NAME_EXT(GL_SOURCE0_ALPHA, _ARB), TYPE_6GLenum, 1, BUGLE_GL_ARB_texture_env_combine, "1.3", STATE_TEXENV },
    { STATE_NAME_EXT(GL_SOURCE1_ALPHA, _ARB), TYPE_6GLenum, 1, BUGLE_GL_ARB_texture_env_combine, "1.3", STATE_TEXENV },
    { STATE_NAME_EXT(GL_SOURCE2_ALPHA, _ARB), TYPE_6GLenum, 1, BUGLE_GL_ARB_texture_env_combine, "1.3", STATE_TEXENV },
    { STATE_NAME_EXT(GL_OPERAND0_RGB, _ARB), TYPE_6GLenum, 1, BUGLE_GL_ARB_texture_env_combine, "1.3", STATE_TEXENV },
    { STATE_NAME_EXT(GL_OPERAND1_RGB, _ARB), TYPE_6GLenum, 1, BUGLE_GL_ARB_texture_env_combine, "1.3", STATE_TEXENV },
    { STATE_NAME_EXT(GL_OPERAND2_RGB, _ARB), TYPE_6GLenum, 1, BUGLE_GL_ARB_texture_env_combine, "1.3", STATE_TEXENV },
    { STATE_NAME_EXT(GL_OPERAND0_ALPHA, _ARB), TYPE_6GLenum, 1, BUGLE_GL_ARB_texture_env_combine, "1.3", STATE_TEXENV },
    { STATE_NAME_EXT(GL_OPERAND1_ALPHA, _ARB), TYPE_6GLenum, 1, BUGLE_GL_ARB_texture_env_combine, "1.3", STATE_TEXENV },
    { STATE_NAME_EXT(GL_OPERAND2_ALPHA, _ARB), TYPE_6GLenum, 1, BUGLE_GL_ARB_texture_env_combine, "1.3", STATE_TEXENV },
    { STATE_NAME_EXT(GL_RGB_SCALE, _ARB), TYPE_8GLdouble, 1, BUGLE_GL_ARB_texture_env_combine, "1.3", STATE_TEXENV },
    { STATE_NAME(GL_ALPHA_SCALE), TYPE_8GLdouble, 1, BUGLE_GL_ARB_texture_env_combine, "1.3", STATE_TEXENV },
#endif
    { STATE_NAME(GL_CURRENT_TEXTURE_COORDS), TYPE_8GLdouble, 4, -1, "1.1", STATE_TEXUNIT },
    { STATE_NAME(GL_CURRENT_RASTER_TEXTURE_COORDS), TYPE_8GLdouble, 4, -1, "1.1", STATE_TEXUNIT },
    { STATE_NAME(GL_TEXTURE_MATRIX), TYPE_8GLdouble, 16, -1, "1.1", STATE_TEXUNIT },
    { STATE_NAME(GL_TEXTURE_STACK_DEPTH), TYPE_5GLint, 1, -1, "1.1", STATE_TEXUNIT },
    { STATE_NAME(GL_TEXTURE_1D), TYPE_9GLboolean, 1, -1, "1.1", STATE_TEXUNIT_ENABLE },
    { STATE_NAME(GL_TEXTURE_2D), TYPE_9GLboolean, 1, -1, "1.1", STATE_TEXUNIT_ENABLE },
#ifdef GL_EXT_texture3D
    { STATE_NAME_EXT(GL_TEXTURE_3D, _EXT), TYPE_9GLboolean, 1, BUGLE_GL_EXT_texture3D, "1.2", STATE_TEXUNIT_ENABLE },
#endif
#ifdef GL_ARB_texture_cube_map
    { STATE_NAME_EXT(GL_TEXTURE_CUBE_MAP, _ARB), TYPE_9GLboolean, 1, BUGLE_GL_ARB_texture_cube_map, "1.3", STATE_TEXUNIT_ENABLE },
#endif
    { STATE_NAME(GL_TEXTURE_BINDING_1D), TYPE_5GLint, 1, -1, "1.1", STATE_TEXUNIT },
    { STATE_NAME(GL_TEXTURE_BINDING_2D), TYPE_5GLint, 1, -1, "1.1", STATE_TEXUNIT },
#ifdef GL_VERSION_1_2
    { STATE_NAME(GL_TEXTURE_BINDING_3D), TYPE_5GLint, 1, -1, "1.2", STATE_TEXUNIT }, /* Note: GL_EXT_texture3D doesn't define this! */
#endif
#ifdef GL_ARB_texture_cube_map
    { STATE_NAME_EXT(GL_TEXTURE_BINDING_CUBE_MAP, _ARB), TYPE_5GLint, 1, BUGLE_GL_ARB_texture_cube_map, "1.3", STATE_TEXUNIT },
#endif
    { STATE_NAME(GL_TEXTURE_GEN_S), TYPE_9GLboolean, 1, -1, "1.1", STATE_TEXUNIT_ENABLE },
    { STATE_NAME(GL_TEXTURE_GEN_T), TYPE_9GLboolean, 1, -1, "1.1", STATE_TEXUNIT_ENABLE },
    { STATE_NAME(GL_TEXTURE_GEN_R), TYPE_9GLboolean, 1, -1, "1.1", STATE_TEXUNIT_ENABLE },
    { STATE_NAME(GL_TEXTURE_GEN_Q), TYPE_9GLboolean, 1, -1, "1.1", STATE_TEXUNIT_ENABLE },
    { NULL, GL_NONE, NULL_TYPE, 0, 0, NULL, 0 }
    /* VALUE GL_TEXTURE_COORD_ARRAY GLarray 1 glstate_get_texunit */
};

static const state_info texture_gen_state[] =
{
    { STATE_NAME(GL_EYE_PLANE), TYPE_8GLdouble, 4, -1, "1.1", STATE_TEXGEN },
    { STATE_NAME(GL_OBJECT_PLANE), TYPE_8GLdouble, 4, -1, "1.1", STATE_TEXGEN },
    { STATE_NAME(GL_TEXTURE_GEN_MODE), TYPE_6GLenum, 1, -1, "1.1", STATE_TEXGEN },
    { NULL, GL_NONE, NULL_TYPE, 0, 0, NULL, 0 }
};

static const state_info light_state[] =
{
    { STATE_NAME(GL_AMBIENT), TYPE_8GLdouble, 4, -1, "1.1", STATE_LIGHT },
    { STATE_NAME(GL_DIFFUSE), TYPE_8GLdouble, 4, -1, "1.1", STATE_LIGHT },
    { STATE_NAME(GL_SPECULAR), TYPE_8GLdouble, 4, -1, "1.1", STATE_LIGHT },
    { STATE_NAME(GL_POSITION), TYPE_8GLdouble, 4, -1, "1.1", STATE_LIGHT },
    { STATE_NAME(GL_CONSTANT_ATTENUATION), TYPE_8GLdouble, 1, -1, "1.1", STATE_LIGHT },
    { STATE_NAME(GL_LINEAR_ATTENUATION), TYPE_8GLdouble, 1, -1, "1.1", STATE_LIGHT },
    { STATE_NAME(GL_QUADRATIC_ATTENUATION), TYPE_8GLdouble, 1, -1, "1.1", STATE_LIGHT },
    { STATE_NAME(GL_SPOT_DIRECTION), TYPE_8GLdouble, 3, -1, "1.1", STATE_LIGHT },
    { STATE_NAME(GL_SPOT_EXPONENT), TYPE_8GLdouble, 1, -1, "1.1", STATE_LIGHT },
    { STATE_NAME(GL_SPOT_CUTOFF), TYPE_8GLdouble, 1, -1, "1.1", STATE_LIGHT },
    { NULL, GL_NONE, NULL_TYPE, 0, 0, NULL, 0 }
    /* VALUE enable GLboolean 1 glstate_get_light */
};

static const state_info material_state[] =
{
    { STATE_NAME(GL_AMBIENT), TYPE_8GLdouble, 4, -1, "1.1", STATE_MATERIAL },
    { STATE_NAME(GL_DIFFUSE), TYPE_8GLdouble, 4, -1, "1.1", STATE_MATERIAL },
    { STATE_NAME(GL_SPECULAR), TYPE_8GLdouble, 4, -1, "1.1", STATE_MATERIAL },
    { STATE_NAME(GL_EMISSION), TYPE_8GLdouble, 4, -1, "1.1", STATE_MATERIAL },
    { STATE_NAME(GL_SHININESS), TYPE_8GLdouble, 1, -1, "1.1", STATE_MATERIAL },
    { STATE_NAME(GL_COLOR_INDEXES), TYPE_8GLdouble, 3, -1, "1.1", STATE_MATERIAL },
    { NULL, GL_NONE, NULL_TYPE, 0, 0, NULL, 0 }
};

/* This exists to simplify the work of gldump.c */
const state_info * const all_state[] =
{
    global_state,
    texture_object_state,
    texture_level_state,
    texture_unit_state,
    texture_gen_state,
    light_state,
    material_state,
    NULL
};

typedef struct
{
    void *out;
    budgie_type type;
    int length;
} dump_wrapper_data;

static void dump_wrapper(FILE *f, void *data)
{
    const dump_wrapper_data *w;
    int length;

    w = (const dump_wrapper_data *) data;
    length = w->length;
    if (length == 1) length = -1;
    budgie_dump_any_type_extended(w->type, w->out, -1, length, NULL, f);
}

char *bugle_state_get_string(const glstate *state)
{
    GLdouble d[16];
    GLfloat f[16];
    GLint i[16];
    GLboolean b[16];
    GLvoid *p[16];
    void *in;
    budgie_type in_type;
    dump_wrapper_data wrapper;

    GLint old_texture;
    GLenum old_unit, old_client_unit;
    bool set_texture_unit = false;
    bool set_texture_object = false;

    if (!state->info) return NULL;

#ifdef GL_ARB_multitexture
    if ((state->info->flags & STATE_FLAG_TEXUNIT)
        && bugle_gl_has_extension(BUGLE_GL_ARB_multitexture))
    {
        CALL_glGetIntegerv(GL_ACTIVE_TEXTURE_ARB, &old_unit);
        CALL_glGetIntegerv(GL_CLIENT_ACTIVE_TEXTURE_ARB, &old_client_unit);
        CALL_glActiveTextureARB(state->unit);
        CALL_glClientActiveTextureARB(state->unit);
        set_texture_unit = true;
    }
#endif
    if (state->info->flags & STATE_FLAG_TEXTARGET)
    {
        CALL_glGetIntegerv(state->binding, &old_texture);
        CALL_glBindTexture(state->target, state->object);
        set_texture_object = true;
    }

    switch (state->info->flags & STATE_MODE_MASK)
    {
    case STATE_MODE_GLOBAL:
        if (state->info->type == TYPE_8GLdouble)
        {
            CALL_glGetDoublev(state->info->pname, d);
            in_type = TYPE_8GLdouble;
        }
        else if (state->info->type == TYPE_7GLfloat)
        {
            CALL_glGetFloatv(state->info->pname, f);
            in_type = TYPE_7GLfloat;
        }
        else if (state->info->type == TYPE_9GLboolean)
        {
            CALL_glGetBooleanv(state->info->pname, b);
            in_type = TYPE_9GLboolean;
        }
        else if (state->info->type == TYPE_P6GLvoid)
        {
            CALL_glGetPointerv(state->info->pname, p);
            in_type = TYPE_P6GLvoid;
        }
        else
        {
            CALL_glGetIntegerv(state->info->pname, i);
            in_type = TYPE_5GLint;
        }
        break;
    case STATE_MODE_ENABLE:
        b[0] = CALL_glIsEnabled(state->info->pname);
        in_type = TYPE_9GLboolean;
        break;
    case STATE_MODE_TEXENV:
        if (state->info->type == TYPE_8GLdouble || state->info->type == TYPE_7GLfloat)
        {
            CALL_glGetTexEnvfv(state->target, state->info->pname, f);
            in_type = TYPE_7GLfloat;
        }
        else
        {
            CALL_glGetTexEnviv(state->target, state->info->pname, i);
            in_type = TYPE_5GLint;
        }
        break;
    case STATE_MODE_TEXPARAMETER:
        if (state->info->type == TYPE_8GLdouble || state->info->type == TYPE_7GLfloat)
        {
            CALL_glGetTexParameterfv(state->target, state->info->pname, f);
            in_type = TYPE_7GLfloat;
        }
        else
        {
            CALL_glGetTexParameteriv(state->target, state->info->pname, i);
            in_type = TYPE_5GLint;
        }
        break;
    case STATE_MODE_TEXLEVELPARAMETER:
        if (state->info->type == TYPE_8GLdouble || state->info->type == TYPE_7GLfloat)
        {
            CALL_glGetTexLevelParameterfv(state->target, state->level, state->info->pname, f);
            in_type = TYPE_7GLfloat;
        }
        else
        {
            CALL_glGetTexLevelParameteriv(state->target, state->level, state->info->pname, i);
            in_type = TYPE_5GLint;
        }
        break;
    case STATE_MODE_TEXGEN:
        if (state->info->type == TYPE_8GLdouble)
        {
            CALL_glGetTexGendv(state->target, state->info->pname, d);
            in_type = TYPE_8GLdouble;
        }
        else if (state->info->type == TYPE_7GLfloat)
        {
            CALL_glGetTexGenfv(state->target, state->info->pname, f);
            in_type = TYPE_7GLfloat;
        }
        else
        {
            CALL_glGetTexGeniv(state->target, state->info->pname, i);
            in_type = TYPE_5GLint;
        }
        break;
    case STATE_MODE_LIGHT:
        if (state->info->type == TYPE_8GLdouble || state->info->type == TYPE_7GLfloat)
        {
            CALL_glGetLightfv(state->target, state->info->pname, f);
            in_type = TYPE_7GLfloat;
        }
        else
        {
            CALL_glGetLightiv(state->target, state->info->pname, i);
            in_type = TYPE_5GLint;
        }
        break;
    case STATE_MODE_MATERIAL:
        if (state->info->type == TYPE_8GLdouble || state->info->type == TYPE_7GLfloat)
        {
            CALL_glGetMaterialfv(state->target, state->info->pname, f);
            in_type = TYPE_7GLfloat;
        }
        else
        {
            CALL_glGetMaterialiv(state->target, state->info->pname, i);
            in_type = TYPE_5GLint;
        }
        break;
    default:
        abort();
    }

#ifdef GL_ARB_multitexture
    if (set_texture_unit)
    {
        CALL_glActiveTextureARB(old_unit);
        CALL_glClientActiveTextureARB(old_client_unit);
    }
#endif
    if (set_texture_object)
        CALL_glBindTexture(state->target, old_texture);

    switch (in_type)
    {
    case TYPE_8GLdouble: in = d; break;
    case TYPE_7GLfloat: in = f; break;
    case TYPE_5GLint: in = i; break;
    case TYPE_9GLboolean: in = b; break;
    case TYPE_P6GLvoid: in = p; break;
    default: abort();
    }

    wrapper.out = bugle_malloc(state->info->length * budgie_type_table[state->info->type].size);
    wrapper.type = state->info->type;
    wrapper.length = state->info->length;
    budgie_type_convert(wrapper.out, wrapper.type, in, in_type, wrapper.length);
    return budgie_string_io(dump_wrapper, &wrapper);
}

void bugle_state_get_children(const glstate *self, bugle_linked_list *children)
{
    if (self->spawn_children) (*self->spawn_children)(self, children);
    else bugle_list_init(children, true);
}

void bugle_state_clear(glstate *self)
{
    if (self->name) free(self->name);
}

/* NB: must initialise the list yourself */
static void make_leaves(const glstate *self, const state_info *table,
                        bugle_linked_list *children)
{
    const char *version;
    glstate *child;
    const state_info *info;

    version = CALL_glGetString(GL_VERSION);
    for (info = table; info->name; info++)
    {
        if ((info->version && strcmp(version, info->version) >= 0)
            || (info->extension != -1 && bugle_gl_has_extension(info->extension)))
        {
            child = bugle_malloc(sizeof(glstate));
            *child = *self; // copies contextual info
            child->name = bugle_strdup(info->name);
            child->info = info;
            child->spawn_children = NULL;
            bugle_list_append(children, child);
        }
    }
}

static void spawn_children_texgen(const glstate *self, bugle_linked_list *children)
{
    bugle_list_init(children, true);
    make_leaves(self, texture_gen_state, children);
}

static void spawn_children_texunit(const glstate *self, bugle_linked_list *children)
{
    bugle_list_node *i;
    int j;
    glstate *child;
    GLenum gen_enum[4] = {GL_S, GL_T, GL_R, GL_Q};
    const char *gen_names[4] = {"GL_S", "GL_T", "GL_R", "GL_Q"};

    bugle_list_init(children, true);
    make_leaves(self, texture_unit_state, children);
    for (i = bugle_list_head(children); i; i = bugle_list_next(i))
    {
        child = (glstate *) bugle_list_data(i);
        if (child->info
            && (child->info->flags & STATE_MODE_MASK) == STATE_MODE_TEXENV)
        {
#ifdef GL_EXT_texture_lod_bias
            if (child->info->flags & STATE_FLAG_FILTERCONTROL)
                child->target = GL_TEXTURE_FILTER_CONTROL;
            else
#endif
            {
                child->target = GL_TEXTURE_ENV;
            }
        }
    }

    for (j = 0; j < 4; j++)
    {
        child = bugle_malloc(sizeof(glstate));
        memset(child, 0, sizeof(glstate));
        child->name = bugle_strdup(gen_names[j]);
        child->unit = self->unit;
        child->target = gen_enum[j];
        child->spawn_children = spawn_children_texgen;
        bugle_list_append(children, child);
    }
}

static void spawn_children_light(const glstate *self, bugle_linked_list *children)
{
    bugle_list_init(children, true);
    make_leaves(self, light_state, children);
}

static void spawn_children_material(const glstate *self, bugle_linked_list *children)
{
    bugle_list_init(children, true);
    make_leaves(self, material_state, children);
}

static void spawn_children_global(const glstate *self, bugle_linked_list *children)
{
    GLint count, i;
    glstate *child;
    GLenum material_enum[2] = {GL_FRONT, GL_BACK};
    const char *material_name[2] = {"GL_FRONT", "GL_BACK"};

    bugle_list_init(children, true);
    make_leaves(self, global_state, children);
#ifdef GL_ARB_multitexture
    if (bugle_gl_has_extension(BUGLE_GL_ARB_multitexture))
    {
        CALL_glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &count);
        for (i = 0; i < count; i++)
        {
            child = bugle_malloc(sizeof(glstate));
            memset(child, 0, sizeof(glstate));
            bugle_asprintf(&child->name, "GL_TEXTURE%d", i);
            child->unit = GL_TEXTURE0_ARB + i;
            child->spawn_children = spawn_children_texunit;
            bugle_list_append(children, child);
        }
    }
    else
#endif
    {
        make_leaves(self, texture_unit_state, children);
    }

    CALL_glGetIntegerv(GL_MAX_LIGHTS, &count);
    for (i = 0; i < count; i++)
    {
        child = bugle_malloc(sizeof(glstate));
        memset(child, 0, sizeof(glstate));
        bugle_asprintf(&child->name, "GL_LIGHT%d", i);
        child->target = GL_LIGHT0 + i;
        child->spawn_children = spawn_children_light;
        bugle_list_append(children, child);
    }

    for (i = 0; i < 2; i++)
    {
        child = bugle_malloc(sizeof(glstate));
        memset(child, 0, sizeof(glstate));
        child->name = bugle_strdup(material_name[i]);
        child->target = material_enum[i];
        child->spawn_children = spawn_children_material;
        bugle_list_append(children, child);
    }
}

const glstate *bugle_state_get_root(void)
{
    static const glstate root =
    { "", GL_NONE, GL_NONE, GL_NONE, 0, 0, NULL, spawn_children_global };

    return &root;
}
