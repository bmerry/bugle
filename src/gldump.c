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
#include <GL/gl.h>
#include <string.h>
#include <assert.h>
#include "gltokens.h"
#include "gldump.h"
#include "glutils.h"
#include "canon.h"
#include "filters.h"
#include "common/safemem.h"
#include "budgielib/budgieutils.h"
#include "src/types.h"
#include "src/utils.h"

const char *gl_enum_to_token(GLenum e)
{
    int l, r, m;

    l = 0;
    r = gl_token_count;
    while (l + 1 < r)
    {
        m = (l + r) / 2;
        if (e < gl_tokens_value[m].value) r = m;
        else l = m;
    }
    if (gl_tokens_value[l].value != e)
        return NULL;
    else
    {
        /* Pick the first one, to avoid using extension suffices */
        while (l > 0 && gl_tokens_value[l - 1].value == e)
            l--;
        return gl_tokens_value[l].name;
    }
}

GLenum gl_token_to_enum(const char *name)
{
    int l, r, m;

    l = 0;
    r = gl_token_count;
    while (l + 1 < r)
    {
        m = (l + r) / 2;
        if (strcmp(name, gl_tokens_name[m].name) < 0) r = m;
        else l = m;
    }
    if (strcmp(gl_tokens_name[l].name, name) != 0)
        return (GLenum) -1;
    else
        return gl_tokens_name[l].value;
}

budgie_type gl_type_to_type(GLenum gl_type)
{
    switch (gl_type)
    {
    case GL_UNSIGNED_BYTE_3_3_2:
    case GL_UNSIGNED_BYTE_2_3_3_REV:
    case GL_UNSIGNED_BYTE:
        return TYPE_7GLubyte;
    case GL_BYTE:
        return TYPE_6GLbyte;
    case GL_UNSIGNED_SHORT_5_6_5:
    case GL_UNSIGNED_SHORT_5_6_5_REV:
    case GL_UNSIGNED_SHORT_4_4_4_4:
    case GL_UNSIGNED_SHORT_4_4_4_4_REV:
    case GL_UNSIGNED_SHORT_5_5_5_1:
    case GL_UNSIGNED_SHORT_1_5_5_5_REV:
    case GL_UNSIGNED_SHORT:
        return TYPE_8GLushort;
    case GL_SHORT:
        return TYPE_7GLshort;
    case GL_UNSIGNED_INT_8_8_8_8:
    case GL_UNSIGNED_INT_8_8_8_8_REV:
    case GL_UNSIGNED_INT_10_10_10_2:
    case GL_UNSIGNED_INT_2_10_10_10_REV:
    case GL_UNSIGNED_INT:
        return TYPE_6GLuint;
    case GL_INT:
        return TYPE_5GLint;
    case GL_FLOAT:
        return TYPE_7GLfloat;
    case GL_DOUBLE:
        return TYPE_8GLdouble;
    default:
        fprintf(stderr, "Do not know the correct type for %s; please email the author\n",
                gl_enum_to_token(gl_type));
        exit(1);
    }
}

budgie_type gl_type_to_type_ptr(GLenum gl_type)
{
    budgie_type ans;

    ans = type_table[gl_type_to_type(gl_type)].pointer;
    assert(ans != NULL_TYPE);
    return ans;
}

size_t gl_type_to_size(GLenum gl_type)
{
    return type_table[gl_type_to_type(gl_type)].size;
}

int gl_format_to_count(GLenum format, GLenum type)
{
    switch (type)
    {
    case GL_UNSIGNED_BYTE:
    case GL_BYTE:
    case GL_UNSIGNED_SHORT:
    case GL_SHORT:
    case GL_UNSIGNED_INT:
    case GL_INT:
    case GL_FLOAT:
        /* Note: GL_DOUBLE is not a legal value */
        switch (format)
        {
        case 1:
        case 2:
        case 3:
        case 4:
            return format;
        case GL_COLOR_INDEX:
        case GL_RED:
        case GL_GREEN:
        case GL_BLUE:
        case GL_ALPHA:
        case GL_LUMINANCE:
        case GL_STENCIL_INDEX:
        case GL_DEPTH_COMPONENT:
            return 1;
        case GL_LUMINANCE_ALPHA:
            return 2;
        case GL_RGB:
        case GL_BGR:
            return 3;
        case GL_RGBA:
        case GL_BGRA:
            return 4;
        default:
            fprintf(stderr, "unknown format %s; assuming 4 components\n",
                    gl_enum_to_token(format));
            return 4; /* conservative */
        }
        break;
    default:
        assert(type != GL_BITMAP); /* cannot return 1/8 */
        return 1; /* all the packed types */
    }
}

bool dump_GLenum(const void *value, int count, FILE *out)
{
    GLenum e = *(const GLenum *) value;
    const char *name = gl_enum_to_token(e);
    if (!name)
        fprintf(out, "<unknown token 0x%.4x>", (unsigned int) e);
    else
        fputs(name, out);
    return true;
}

bool dump_GLerror(const void *value, int count, FILE *out)
{
    GLenum err = *(const GLenum *) value;

    switch (err)
    {
    case GL_NO_ERROR: fputs("GL_NO_ERROR", out); break;
    default: dump_GLenum(value, count, out);
    }
    return true;
}

/* FIXME: redo definition of alternate enums, based on sort order */
bool dump_GLalternateenum(const void *value, int count, FILE *out)
{
    GLenum token = *(const GLenum *) value;

    switch (token)
    {
    case GL_ZERO: fputs("GL_ZERO", out); break;
    case GL_ONE: fputs("GL_ONE", out); break;
    default: dump_GLenum(value, count, out);
    }
    return true;
}

bool dump_GLboolean(const void *value, int count, FILE *out)
{
    fputs((*(const GLboolean *) value) ? "GL_TRUE" : "GL_FALSE", out);
    return true;
}

static budgie_type get_real_type(const function_call *call)
{
    switch (canonical_call(call))
    {
    case FUNC_glTexParameteri:
    case FUNC_glTexParameteriv:
    case FUNC_glTexParameterf:
    case FUNC_glTexParameterfv:
    case FUNC_glGetTexParameteriv:
    case FUNC_glGetTexParameterfv:
        switch (*(const GLenum *) call->args[1])
        {
        case GL_TEXTURE_WRAP_S:
        case GL_TEXTURE_WRAP_T:
        case GL_TEXTURE_WRAP_R:
        case GL_TEXTURE_MIN_FILTER:
        case GL_TEXTURE_MAG_FILTER:
#ifdef GL_ARB_depth_texture
        case GL_DEPTH_TEXTURE_MODE_ARB:
#endif
#ifdef GL_ARB_shadow
        case GL_TEXTURE_COMPARE_MODE_ARB:
        case GL_TEXTURE_COMPARE_FUNC_ARB:
#endif
            return TYPE_6GLenum;
#ifdef GL_SGIS_generate_mipmap
        case GL_GENERATE_MIPMAP_SGIS:
            return TYPE_9GLboolean;
#endif
        default:
            return NULL_TYPE;
        }
        break;
    default:
        return NULL_TYPE;
    }
}

bool dump_convert(const generic_function_call *gcall,
                  int arg,
                  const void *value,
                  FILE *out)
{
    const function_call *call;
    budgie_type in_type, out_type;
    const void *in;
    int length = -1;
    void *out_data;

    call = (const function_call *) gcall;
    out_type = get_real_type(call);
    if (out_type == NULL_TYPE) return false;

    in_type = function_table[call->generic.id].parameters[2].type;
    if (type_table[in_type].code == CODE_POINTER)
    {
        in = *(const void * const *) value;
        in_type = type_table[in_type].type;
    }
    else
        in = value;
    if (function_table[call->generic.id].parameters[2].get_length)
        length = (*function_table[call->generic.id].parameters[2].get_length)(gcall, arg, value);
    if (length < 0) length = 1;

    out_data = xmalloc(type_table[out_type].size * length);
    type_convert(out_data, out_type, in, in_type, length);
    dump_any_type(out_type, out_data, length, out);
    free(out_data);
    return true;
}

int count_gl(budgie_function func, GLenum token)
{
    switch (token)
    {
        /* lights and materials */
    case GL_AMBIENT:
    case GL_DIFFUSE:
    case GL_SPECULAR:
    case GL_EMISSION:
    case GL_AMBIENT_AND_DIFFUSE:
    case GL_POSITION:
        /* glLightModel */
    case GL_LIGHT_MODEL_AMBIENT:
        /* glTexEnv */
    case GL_TEXTURE_ENV_COLOR:
        /* glFog */
    case GL_FOG_COLOR:
        /* glTexParmeter */
    case GL_TEXTURE_BORDER_COLOR:
        /* glTexGen */
    case GL_OBJECT_PLANE:
    case GL_EYE_PLANE:
        return 4;
        /* Other */
    case GL_COLOR_INDEXES:
    case GL_SPOT_DIRECTION:
        return 3;
    default:
        return -1;
    }
}

/* Computes the number of pixel elements (units of byte, int, float etc)
 * used by a client-side encoding of a 1D, 2D or 3D image.
 * Specify -1 for depth for 1D or 2D textures.
 *
 * The trackcontext and trackbeginend filtersets
 * must be loaded for this to work.
 */
size_t image_element_count(GLsizei width,
                           GLsizei height,
                           GLsizei depth,
                           GLenum format,
                           GLenum type,
                           bool unpack)
{
    state_7context_I *ctx;
    /* data from OpenGL state */
    GLint swap_bytes = 0, row_length = 0, image_height = 0;
    GLint skip_pixels = 0, skip_rows = 0, skip_images = 0, alignment = 4;
    /* following the notation of the OpenGL 1.5 spec, section 3.6.4 */
    int l, n, k, s, a;
    int elements; /* number of elements in the last row */

    /* First check that we aren't in begin/end, in which case the call
     * will fail anyway.
     */
    ctx = get_context_state();
    if (in_begin_end()) return 0;
    if (unpack)
    {
        CALL_glGetIntegerv(GL_UNPACK_SWAP_BYTES, &swap_bytes);
        CALL_glGetIntegerv(GL_UNPACK_ROW_LENGTH, &row_length);
        CALL_glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &skip_pixels);
        CALL_glGetIntegerv(GL_UNPACK_SKIP_ROWS, &skip_rows);
        CALL_glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
#ifdef GL_EXT_texture3D
        if (depth >= 0)
        {
            CALL_glGetIntegerv(GL_UNPACK_IMAGE_HEIGHT, &image_height);
            CALL_glGetIntegerv(GL_UNPACK_SKIP_IMAGES, &skip_images);
        }
#endif
    }
    else
    {
        CALL_glGetIntegerv(GL_PACK_SWAP_BYTES, &swap_bytes);
        CALL_glGetIntegerv(GL_PACK_ROW_LENGTH, &row_length);
        CALL_glGetIntegerv(GL_PACK_SKIP_PIXELS, &skip_pixels);
        CALL_glGetIntegerv(GL_PACK_SKIP_ROWS, &skip_rows);
        CALL_glGetIntegerv(GL_PACK_ALIGNMENT, &alignment);
#ifdef GL_EXT_texture3D
        if (depth >= 0)
        {
            CALL_glGetIntegerv(GL_PACK_IMAGE_HEIGHT_EXT, &image_height);
            CALL_glGetIntegerv(GL_PACK_SKIP_IMAGES_EXT, &skip_images);
        }
#endif
    }
    a = alignment;
    skip_images = (depth > 0) ? skip_images : 0;
    depth = abs(depth);
    image_height = (image_height > 0) ? image_height : height;
    l = (row_length > 0) ? row_length : width;
    /* FIXME: divisions can be avoided */
    if (type == GL_BITMAP) /* bitmaps are totally different */
    {
        k = a * ((l + 8 * a - 1) / (8 * a));
        elements = a * (((width + skip_pixels) + 8 * a - 1) / (8 * a));
    }
    else
    {
        n = gl_format_to_count(format, type);
        s = gl_type_to_size(type);
        if ((s == 1 || s == 2 || s == 4 || s == 8)
            && s < a)
            k = a / s * ((s * n * l + a - 1) / a);
        else
            k = n * l;
        elements = (width + skip_pixels) * n;
    }
    return elements
        + k * (height + skip_rows - 1)
        + k * image_height * (depth + skip_images - 1);
}

/* Computes the number of pixel elements required by glGetTexImage
 */
size_t texture_element_count(GLenum target,
                             GLint level,
                             GLenum format,
                             GLenum type)
{
    int width, height, depth = -1;
    /* FIXME: don't query for depth if we don't have 3D textures */
    CALL_glGetTexLevelParameteriv(target, level, GL_TEXTURE_WIDTH, &width);
    CALL_glGetTexLevelParameteriv(target, level, GL_TEXTURE_HEIGHT, &height);
#ifdef GL_EXT_texture3D
    CALL_glGetTexLevelParameteriv(target, level, GL_TEXTURE_DEPTH, &depth);
#endif
    return image_element_count(width, height, depth, format, type, false);
}
