/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2008, 2010-2012  Bruce Merry
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

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <bugle/gl/glheaders.h>
#include <bugle/gl/glutils.h>
#include <bugle/gl/gldump.h>
#include <bugle/gl/gltypes.h>
#include <bugle/gl/glbeginend.h>
#include <bugle/gl/glextensions.h>
#include <bugle/gl/glsl.h>
#include <bugle/filters.h>
#include <bugle/log.h>
#include <bugle/apireflect.h>
#include <bugle/memory.h>
#include <bugle/io.h>
#include <budgie/types.h>
#include <budgie/reflect.h>
#include <budgie/call.h>
#include "budgielib/defines.h"

budgie_type bugle_gl_type_to_type(GLenum gl_type)
{
    switch (gl_type)
    {
    case GL_UNSIGNED_BYTE_3_3_2:
    case GL_UNSIGNED_BYTE_2_3_3_REV:
    case GL_UNSIGNED_BYTE:
        return TYPE_7GLubyte;
    case GL_BYTE:
        return TYPE_6GLbyte;
    case GL_UNSIGNED_SHORT_4_4_4_4:
    case GL_UNSIGNED_SHORT_5_5_5_1:
    case GL_UNSIGNED_SHORT_5_6_5:
    case GL_UNSIGNED_SHORT_5_6_5_REV:
    case GL_UNSIGNED_SHORT_4_4_4_4_REV:
    case GL_UNSIGNED_SHORT_1_5_5_5_REV:
    case GL_UNSIGNED_SHORT_8_8_APPLE:
    case GL_UNSIGNED_SHORT_8_8_REV_APPLE:
    case GL_UNSIGNED_SHORT:
        return TYPE_8GLushort;
    case GL_SHORT:
        return TYPE_7GLshort;
    case GL_UNSIGNED_INT_8_8_8_8:
    case GL_UNSIGNED_INT_10_10_10_2:
    case GL_UNSIGNED_INT_8_8_8_8_REV:
    case GL_UNSIGNED_INT_2_10_10_10_REV:
    case GL_UNSIGNED_INT_24_8:
    case GL_UNSIGNED_INT_S8_S8_8_8_NV:
    case GL_UNSIGNED_INT_8_8_S8_S8_REV_NV:
    case GL_UNSIGNED_INT_10F_11F_11F_REV:
    case GL_UNSIGNED_INT_5_9_9_9_REV:
    case GL_UNSIGNED_INT:
        return TYPE_6GLuint;
    case GL_INT:
        return TYPE_5GLint;
    case GL_FLOAT:
        return TYPE_7GLfloat;
    case GL_HALF_FLOAT:
        return TYPE_9GLhalfARB;
    case GL_DOUBLE:
        return TYPE_8GLdouble;
    case GL_FLOAT_VEC2: return TYPE_6GLvec2; break;
    case GL_FLOAT_VEC3: return TYPE_6GLvec3; break;
    case GL_FLOAT_VEC4: return TYPE_6GLvec4; break;
    case GL_INT_VEC2: return TYPE_7GLivec2; break;
    case GL_INT_VEC3: return TYPE_7GLivec3; break;
    case GL_INT_VEC4: return TYPE_7GLivec4; break;
    case GL_FLOAT_MAT2: return TYPE_6GLmat2; break;
    case GL_FLOAT_MAT3: return TYPE_6GLmat3; break;
    case GL_FLOAT_MAT4: return TYPE_6GLmat4; break;
    case GL_BOOL: return TYPE_9GLboolean; break;
    case GL_BOOL_VEC2: return TYPE_7GLbvec2; break;
    case GL_BOOL_VEC3: return TYPE_7GLbvec3; break;
    case GL_BOOL_VEC4: return TYPE_7GLbvec4; break;
    case GL_SAMPLER_1D:
    case GL_SAMPLER_2D:
    case GL_SAMPLER_3D:
    case GL_SAMPLER_CUBE:
    case GL_SAMPLER_1D_SHADOW:
    case GL_SAMPLER_2D_SHADOW:
    case GL_SAMPLER_2D_RECT_ARB:
    case GL_SAMPLER_2D_RECT_SHADOW_ARB:
        return TYPE_5GLint;
#if defined(GL_VERSION_2_1)
    case GL_FLOAT_MAT2x3: return TYPE_8GLmat2x3; break;
    case GL_FLOAT_MAT3x2: return TYPE_8GLmat3x2; break;
    case GL_FLOAT_MAT2x4: return TYPE_8GLmat2x4; break;
    case GL_FLOAT_MAT4x2: return TYPE_8GLmat4x2; break;
    case GL_FLOAT_MAT3x4: return TYPE_8GLmat3x4; break;
    case GL_FLOAT_MAT4x3: return TYPE_8GLmat4x3; break;
#endif
    default:
        fprintf(stderr,
                "Do not know the correct type for %s. This probably indicates that you\n"
                "passed an illegal enumerant when a type enum (such as GL_FLOAT) was\n"
                "expected. If this is not the case, email the author with details of the\n"
                "function that you called and the arguments that you passed to it. You can\n"
                "find the location of this error by setting a breakpoint on line %d\n"
                "of %s and examining the backtrace.\n",
                bugle_api_enum_name(gl_type, BUGLE_API_EXTENSION_BLOCK_GL), __LINE__, __FILE__);
        return TYPE_7GLubyte;
    }
}

budgie_type bugle_gl_type_to_type_ptr(GLenum gl_type)
{
    budgie_type ans;

    ans = budgie_type_pointer(bugle_gl_type_to_type(gl_type));
    assert(ans != NULL_TYPE);
    return ans;
}

static budgie_type bugle_gl_type_to_type_ptr_vbo(GLenum gl_type, GLenum binding)
{
    if (binding != 0 && bugle_gl_begin_internal_render())
    {
        GLint id;
        CALL(glGetIntegerv)(binding, &id);
        bugle_gl_end_internal_render("bugle_gl_type_to_type_ptr_vbo", BUGLE_FALSE);
        if (id != 0)
        {
            if (sizeof(unsigned long) == sizeof(GLvoid *)) return TYPE_m;
            else return TYPE_P6GLvoid;
        }
    }
    return bugle_gl_type_to_type_ptr(gl_type);
}

budgie_type bugle_gl_type_to_type_ptr_pbo_source(GLenum gl_type)
{
    GLenum binding = 0;
#ifdef GL_EXT_pixel_buffer_object
    if (BUGLE_GL_HAS_EXTENSION_GROUP(GL_EXT_pixel_buffer_object))
    {
        binding = GL_PIXEL_UNPACK_BUFFER_BINDING_EXT;
    }
#endif
    return bugle_gl_type_to_type_ptr_vbo(gl_type, binding);
}

budgie_type bugle_gl_type_to_type_ptr_pbo_sink(GLenum gl_type)
{
    GLenum binding = 0;
#ifdef GL_EXT_pixel_buffer_object
    if (BUGLE_GL_HAS_EXTENSION_GROUP(GL_EXT_pixel_buffer_object))
    {
        binding = GL_PIXEL_PACK_BUFFER_BINDING_EXT;
    }
#endif
    return bugle_gl_type_to_type_ptr_vbo(gl_type, binding);
}

budgie_type bugle_gl_type_to_type_ptr_vbo_element(GLenum gl_type)
{
    GLenum binding = 0;
#if BUGLE_GLTYPE_GL
    if (BUGLE_GL_HAS_EXTENSION_GROUP(GL_ARB_vertex_buffer_object))
#endif
    {
        binding = GL_ELEMENT_ARRAY_BUFFER_BINDING;
    }
    return bugle_gl_type_to_type_ptr_vbo(gl_type, binding);
}

size_t bugle_gl_type_to_size(GLenum gl_type)
{
    return budgie_type_size(bugle_gl_type_to_type(gl_type));
}

int bugle_gl_format_to_count(GLenum format, GLenum type)
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
    case GL_HALF_FLOAT:
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
        case GL_INTENSITY:
        case GL_LUMINANCE:
        case GL_SLUMINANCE:
        case GL_STENCIL_INDEX:
        case GL_DEPTH_COMPONENT:
        case GL_RED_INTEGER:
        case GL_GREEN_INTEGER:
        case GL_BLUE_INTEGER:
        case GL_ALPHA_INTEGER:
        case GL_LUMINANCE_INTEGER_EXT:
            return 1;
        case GL_LUMINANCE_ALPHA:
        case GL_SLUMINANCE_ALPHA_EXT:
        case GL_LUMINANCE_ALPHA_INTEGER_EXT:
        case GL_RG:
        case GL_RG_INTEGER:
        case GL_DEPTH_STENCIL:
            return 2;
        case GL_RGB:
        case GL_BGR:
        case GL_SRGB:
            return 3;
        case GL_RGBA:
        case GL_BGRA:
        case GL_ABGR_EXT:
        case GL_SRGB_ALPHA_EXT:
            return 4;
        default:
            bugle_log_printf("gldump", "format_to_count", BUGLE_LOG_WARNING,
                             "unknown format %s; assuming 4 components",
                             bugle_api_enum_name(format, BUGLE_API_EXTENSION_BLOCK_GL));
            return 4; /* conservative */
        }
        break;
    default:
        assert(type != GL_BITMAP); /* cannot return 1/8 */
        return 1; /* all the packed types */
    }
}

typedef struct
{
    GLenum key;
    budgie_type type;
    int length;
} dump_table_entry;

static int compare_dump_table_entry(const void *a, const void *b)
{
    GLenum ka, kb;

    ka = ((const dump_table_entry *) a)->key;
    kb = ((const dump_table_entry *) b)->key;
    return (ka < kb) ? -1 : (ka > kb) ? 1 : 0;
}

static int find_dump_table_entry(const void *a, const void *b)
{
    GLenum ka, kb;

    ka = *(GLenum *) a;
    kb = ((const dump_table_entry *) b)->key;
    return (ka < kb) ? -1 : (ka > kb) ? 1 : 0;
}

static dump_table_entry *dump_table = NULL;
static size_t dump_table_size = 0;

/* The state tables tell us many things about the number of parameters
 * that both queries and sets take. This routine processes the state
 * specifications to build a lookup table.
 */
void dump_initialise(void)
{
    dump_table_entry *cur;
    const state_info * const *t;
    const state_info *s;

    /* Count */
    dump_table_size = 0;
    for (t = all_state; *t; t++)
        for (s = *t; s->name; s++)
            if (s->type == TYPE_9GLboolean || s->type == TYPE_6GLenum
                || s->length != 1) dump_table_size++;

    dump_table_size += 1; /* Manual extras */

    dump_table = BUGLE_NMALLOC(dump_table_size, dump_table_entry);
    cur = dump_table;
    for (t = all_state; *t; t++)
        for (s = *t; s->name; s++)
            if (s->type == BUDGIE_TYPE_ID(9GLboolean) || s->type == BUDGIE_TYPE_ID(6GLenum)
                || s->length != 1)
            {
                cur->key = s->pname;
                cur->type = NULL_TYPE;
                switch (s->type)
                {
                case TYPE_9GLboolean:
                case TYPE_6GLenum:
                case TYPE_11GLxfbattrib:
                    cur->type = s->type;
                    break;
                }
                cur->length = (s->length == 1) ? -1 : s->length;
                cur++;
            }

    /* NB: if you update this, also update the adjustment to dump_table_size
     * above!!!
     */
    cur->key = GL_AMBIENT_AND_DIFFUSE;
    cur->type = BUDGIE_TYPE_ID(7GLfloat);
    cur->length = 4;
    cur++;

    qsort(dump_table, dump_table_size, sizeof(dump_table_entry), compare_dump_table_entry);
}

static const dump_table_entry *get_dump_table_entry(GLenum e)
{
    /* Default, if no match is found */
    static dump_table_entry def = {GL_ZERO, NULL_TYPE, -1};
    const dump_table_entry *ans;

    assert(dump_table != NULL);
    ans = (const dump_table_entry *)
        bsearch(&e, dump_table, dump_table_size, sizeof(dump_table_entry),
                find_dump_table_entry);
    return ans ? ans : &def;
}

bugle_bool bugle_dump_convert(GLenum pname, const void *value,
                              budgie_type in_type, bugle_io_writer *writer)
{
    const dump_table_entry *entry;
    budgie_type out_type;
    const void *in;
    int length = -1, alength;
    void *out_data;
    const void *ptr = NULL;
    budgie_type base_type;

    entry = get_dump_table_entry(pname);
    if (entry->type == NULL_TYPE) return BUGLE_FALSE;
    out_type = entry->type;

    base_type = budgie_type_pointer_base(in_type);
    if (base_type != NULL_TYPE)
    {
        in = *(const void * const *) value;
        in_type = base_type;
        ptr = in;
    }
    else
        in = value;

    length = entry->length;
    alength = (length == -1) ? 1 : length;
    out_data = bugle_nmalloc(alength, budgie_type_size(out_type));
    if (out_type == TYPE_11GLxfbattrib)
    {
        /* budgie_type_convert doesn't do array-to-struct conversion */
        memcpy(out_data, in, sizeof(GLxfbattrib));
    }
    else
        budgie_type_convert(out_data, out_type, in, in_type, alength);
    if (ptr)
        budgie_dump_any_type_extended(out_type, out_data, -1, length, ptr, writer);
    else
        budgie_dump_any_type(out_type, out_data, -1, writer);
    bugle_free(out_data);
    return BUGLE_TRUE;
}

int bugle_count_gl(budgie_function func, GLenum token)
{
    return get_dump_table_entry(token)->length;
}

bugle_bool bugle_gl_dump_multi_draw_elements(const GLsizei *count, GLenum gl_type, const GLvoid * const *indices, GLsizei primcount, bugle_io_writer *writer)
{
    budgie_type type, base_type;
    GLsizei i;

    type = bugle_gl_type_to_type_ptr_vbo_element(gl_type);
    base_type = budgie_type_pointer_base(type);
    /* Happens if there is an element array buffer bound */
    if (base_type == NULL_TYPE) return BUGLE_FALSE;

    bugle_io_printf(writer, "%p -> { ", indices);
    for (i = 0; i < primcount; i++)
    {
        if (i) bugle_io_puts(", ", writer);
        budgie_dump_any_type_extended(base_type, indices[i], -1, count[i], indices[i], writer);
    }
    bugle_io_puts(" }", writer);
    return BUGLE_TRUE;
}

#ifdef GL_ARB_vertex_program
int bugle_count_program_string(GLenum target, GLenum pname)
{
    GLint length = 0;
    if (bugle_gl_in_begin_end()) return 0;

    switch (pname)
    {
    case GL_PROGRAM_STRING_ARB:
        CALL(glGetProgramivARB)(target, GL_PROGRAM_LENGTH_ARB, &length);
        break;
    default:
        length = 0;
    }
    return length;
}
#endif

/* Counts the number of objects returned by a call to
 * glGetAttachedShaders(program, max, &count, ptr).
 */
int bugle_count_attached_shaders(GLuint program, GLsizei max)
{
    GLsizei real_count = 0;
    if (bugle_gl_begin_internal_render())
    {
        bugle_glGetProgramiv(program, GL_ATTACHED_SHADERS, &real_count);
        /* The above might generate an error, in which case real_count remains 0 */
        bugle_gl_end_internal_render("bugle_count_attached_shaders", BUGLE_FALSE);
    }
    if (real_count <= max)
        return real_count;
    else
        return max;
}

/* Computes the number of pixel elements (units of byte, int, float etc)
 * used by a client-side encoding of a 1D, 2D or 3D image.
 * Specify -1 for depth for 1D or 2D textures.
 *
 * The trackcontext and glbeginend filtersets
 * must be loaded for this to work.
 */
size_t bugle_image_element_count(GLsizei width,
                                 GLsizei height,
                                 GLsizei depth,
                                 GLenum format,
                                 GLenum type,
                                 bugle_bool unpack)
{
    /* data from OpenGL state */
    GLint swap_bytes = 0, row_length = 0, image_height = 0;
    GLint skip_pixels = 0, skip_rows = 0, skip_images = 0, alignment = 4;
    /* following the notation of the OpenGL 1.5 spec, section 3.6.4 */
    int l, n, k, s, a;
    int elements; /* number of elements in the last row */

    /* First check that we aren't in begin/end, in which case the call
     * will fail anyway.
     */
    if (bugle_gl_in_begin_end()) return 0;
    if (unpack)
    {
        CALL(glGetIntegerv)(GL_UNPACK_SWAP_BYTES, &swap_bytes);
        CALL(glGetIntegerv)(GL_UNPACK_ROW_LENGTH, &row_length);
        CALL(glGetIntegerv)(GL_UNPACK_SKIP_PIXELS, &skip_pixels);
        CALL(glGetIntegerv)(GL_UNPACK_SKIP_ROWS, &skip_rows);
        CALL(glGetIntegerv)(GL_UNPACK_ALIGNMENT, &alignment);
        if (depth >= 0)
        {
            CALL(glGetIntegerv)(GL_UNPACK_IMAGE_HEIGHT, &image_height);
            CALL(glGetIntegerv)(GL_UNPACK_SKIP_IMAGES, &skip_images);
        }
    }
    else
    {
        CALL(glGetIntegerv)(GL_PACK_SWAP_BYTES, &swap_bytes);
        CALL(glGetIntegerv)(GL_PACK_ROW_LENGTH, &row_length);
        CALL(glGetIntegerv)(GL_PACK_SKIP_PIXELS, &skip_pixels);
        CALL(glGetIntegerv)(GL_PACK_SKIP_ROWS, &skip_rows);
        CALL(glGetIntegerv)(GL_PACK_ALIGNMENT, &alignment);
        if (depth >= 0)
        {
            CALL(glGetIntegerv)(GL_PACK_IMAGE_HEIGHT_EXT, &image_height);
            CALL(glGetIntegerv)(GL_PACK_SKIP_IMAGES_EXT, &skip_images);
        }
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
        n = bugle_gl_format_to_count(format, type);
        s = bugle_gl_type_to_size(type);
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

/* Computes the number of pixel elements required by glGetTexImage */
size_t bugle_texture_element_count(GLenum target,
                                   GLint level,
                                   GLenum format,
                                   GLenum type)
{
    int width, height, depth = -1;
    CALL(glGetTexLevelParameteriv)(target, level, GL_TEXTURE_WIDTH, &width);
    CALL(glGetTexLevelParameteriv)(target, level, GL_TEXTURE_HEIGHT, &height);
    if (BUGLE_GL_HAS_EXTENSION_GROUP(GL_EXT_texture3D))
        CALL(glGetTexLevelParameteriv)(target, level, GL_TEXTURE_DEPTH, &depth);
    else
        depth = 1;
    return bugle_image_element_count(width, height, depth, format, type, BUGLE_FALSE);
}
