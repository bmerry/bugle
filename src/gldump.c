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
#include "tracker.h"
#include "common/safemem.h"
#include "budgielib/budgieutils.h"
#include "src/utils.h"

/* FIXME: should this move into budgielib?
 *
 * Calls dump_any_type, BUT:
 * if outer_length != -1, then it considers the type as if it were an
 * array of that type, of length outer_length. If pointer is
 * non-NULL, then it outputs "<pointer> -> " first as if dumping a
 * pointer to the array.
 */
void dump_any_type_extended(budgie_type type,
                            const void *value,
                            int length,
                            int outer_length,
                            const void *pointer,
                            FILE *out)
{
    int i;
    const char *v;

    if (pointer)
        fprintf(out, "%p -> ", pointer);
    if (outer_length == -1)
        dump_any_type(type, value, length, out);
    else
    {
        v = (const char *) value;
        fputs("{ ", out);
        for (i = 0; i < outer_length; i++)
        {
            if (i) fputs(", ", out);
            dump_any_type(type, (const void *) v, length, out);
            v += type_table[type].size;
        }
        fputs(" }", out);
    }
}

const gl_token *gl_enum_to_token_struct(GLenum e)
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
        while (l > 0 && gl_tokens_value[l - 1].value == e) l--;
        return &gl_tokens_value[l];
    }
}

const char *gl_enum_to_token(GLenum e)
{
    const gl_token *t;

    t = gl_enum_to_token_struct(e);
    if (t) return t->name; else return NULL;
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

bool dump_GLenum(GLenum e, FILE *out)
{
    const char *name = gl_enum_to_token(e);
    if (!name)
        fprintf(out, "<unknown token 0x%.4x>", (unsigned int) e);
    else
        fputs(name, out);
    return true;
}

bool dump_GLerror(GLenum err, FILE *out)
{
    switch (err)
    {
    case GL_NO_ERROR: fputs("GL_NO_ERROR", out); break;
    default: dump_GLenum(err, out);
    }
    return true;
}

/* FIXME: redo definition of alternate enums, based on sort order */
bool dump_GLalternateenum(GLenum token, FILE *out)
{
    switch (token)
    {
    case GL_ZERO: fputs("GL_ZERO", out); break;
    case GL_ONE: fputs("GL_ONE", out); break;
    default: dump_GLenum(token, out);
    }
    return true;
}

bool dump_GLcomponentsenum(GLenum token, FILE *out)
{
    if (token >= 1 && token <= 4)
        dump_type_i(&token, -1, out);
    else
        dump_GLenum(token,  out);
    return true;
}

bool dump_GLboolean(GLboolean b, FILE *out)
{
    if (b == 0 || b == 1)
        fputs(b ? "GL_TRUE" : "GL_FALSE", out);
    else
        fprintf(out, "(GLboolean) %u", (unsigned int) b);
    return true;
}

bool dump_GLXDrawable(GLXDrawable d, FILE *out)
{
    fprintf(out, "0x%08x", (unsigned int) d);
    return true;
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
void initialise_dump_tables(void)
{
    /* Initially make room for one entry per state spec. We can scale
     * down later
     */
    size_t specs;
    GLenum e;
    dump_table_entry *cur;
    const state_spec *s;

    specs = sizeof(state_spec_table) / sizeof(state_spec_table[0]);
    dump_table = xmalloc(sizeof(dump_table_entry) * specs);
    cur = dump_table;
    for (s = state_spec_table; s != state_spec_table + specs; s++)
    {
        e = gl_token_to_enum(s->name);
        if (e == (GLenum) -1) continue;
        cur->key = e;
        if (s->data_type == TYPE_9GLboolean
            || s->data_type == TYPE_6GLenum)
            cur->type = s->data_type;
        else
            cur->type = NULL_TYPE;
        if (s->data_length != 1)
            cur->length = s->data_length;
        else
            cur->length = -1;
        if (cur->type != NULL_TYPE || cur->length != -1)
            cur++;
    }

    /* Reduce memory */
    dump_table_size = cur - dump_table;
    dump_table = xrealloc(dump_table, sizeof(dump_table_entry) * dump_table_size);
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

bool dump_convert(GLenum pname, const void *value,
                  budgie_type in_type, FILE *out)
{
    const dump_table_entry *entry;
    budgie_type out_type;
    const void *in;
    int length = -1, alength;
    void *out_data;
    const void *ptr = NULL;

    entry = get_dump_table_entry(pname);
    if (entry->type == NULL_TYPE) return false;
    out_type = entry->type;

    if (type_table[in_type].code == CODE_POINTER)
    {
        in = *(const void * const *) value;
        in_type = type_table[in_type].type;
        ptr = in;
    }
    else
        in = value;

    length = entry->length;
    alength = (length == -1) ? 1 : length;
    out_data = xmalloc(type_table[out_type].size * alength);
    type_convert(out_data, out_type, in, in_type, alength);
    if (ptr)
        dump_any_type_extended(out_type, out_data, -1, length, ptr, out);
    else
        dump_any_type(out_type, out_data, -1, out);
    free(out_data);
    return true;
}

int count_gl(budgie_function func, GLenum token)
{
    return get_dump_table_entry(token)->length;
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
    ctx = tracker_get_context_state();
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
