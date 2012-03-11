/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2009, 2011-2012  Bruce Merry
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
#define _POSIX_SOURCE
#include <bugle/bool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <setjmp.h>
#include <errno.h>
#include <assert.h>
#include <bugle/gl/glheaders.h>
#include <bugle/glwin/trackcontext.h>
#include <bugle/gl/glutils.h>
#include <bugle/gl/glsl.h>
#include <bugle/gl/glbeginend.h>
#include <bugle/gl/glextensions.h>
#include <bugle/filters.h>
#include <bugle/log.h>
#include <bugle/apireflect.h>
#include <bugle/memory.h>
#include "platform/threads.h"
#include <budgie/addresses.h>
#include <budgie/types.h>
#include <budgie/reflect.h>

#ifdef GL_VERSION_1_1
static void checks_texture_complete_fail(int unit, GLenum target, const char *reason)
{
    const char *target_name;

    target_name = bugle_api_enum_name(target, BUGLE_API_EXTENSION_BLOCK_GL);
    if (!target_name) target_name = "<unknown target>";
    bugle_log_printf("checks", "texture", BUGLE_LOG_NOTICE,
                     "GL_TEXTURE%d / %s: incomplete texture (%s)",
                     unit, target_name, reason);
}

/* Tests whether the given texture face (bound to the active texture unit)
 * is complete (unit is passed only for error reporting). The face is either
 * a target, or a single face of a cube map. Returns BUGLE_TRUE if complete,
 * BUGLE_FALSE (and a log message) if not.  It assumes that the minification filter
 * requires mipmapping and that base <= max.
 *
 * TODO: handle sampler objects
 * TODO: handle other reasons for incompleteness e.g. linear filtering of integer data
 */
static bugle_bool checks_texture_face_complete(GLuint unit, GLenum face, int dims, int mip_dims,
                                               int base, int max, bugle_bool needs_mip)
{
    GLint sizes[3], border, format;
    int d, lvl;
    const GLenum dim_enum[3] =
    {
        GL_TEXTURE_WIDTH,
        GL_TEXTURE_HEIGHT,
        GL_TEXTURE_DEPTH
    };

    for (d = 0; d < dims; d++)
    {
        CALL(glGetTexLevelParameteriv)(face, base, dim_enum[d], &sizes[d]);
        if (sizes[d] <= 0)
        {
            checks_texture_complete_fail(unit, face,
                                         "base level does not have positive dimensions");
            return BUGLE_FALSE;
        }
    }

    if (!needs_mip)
        return BUGLE_TRUE;

    CALL(glGetTexLevelParameteriv)(face, base, GL_TEXTURE_BORDER, &border);
    CALL(glGetTexLevelParameteriv)(face, base, GL_TEXTURE_INTERNAL_FORMAT, &format);
    for (lvl = base + 1; lvl <= max; lvl++)
    {
        GLint lformat, lborder;
        bugle_bool more = BUGLE_FALSE;
        for (d = 0; d < mip_dims; d++)
            if (sizes[d] > 1)
            {
                more = BUGLE_TRUE;
                sizes[d] /= 2;
            }
        if (!more) break;
        for (d = 0; d < dims; d++)
        {
            GLint size;
            CALL(glGetTexLevelParameteriv)(face, lvl, dim_enum[d], &size);
            if (size <= 0)
            {
                checks_texture_complete_fail(unit, face,
                                             "missing image in mipmap sequence");
                return BUGLE_FALSE;
            }
            if (size != sizes[d])
            {
                checks_texture_complete_fail(unit, face,
                                             "incorrect size in mipmap sequence");
                return BUGLE_FALSE;
            }
        }
        CALL(glGetTexLevelParameteriv)(face, lvl, GL_TEXTURE_INTERNAL_FORMAT, &lformat);
        CALL(glGetTexLevelParameteriv)(face, lvl, GL_TEXTURE_BORDER, &lborder);
        if (format != lformat)
        {
            checks_texture_complete_fail(unit, face,
                                         "inconsistent internal formats");
            return BUGLE_FALSE;
        }
        if (border != lborder)
        {
            checks_texture_complete_fail(unit, face,
                                         "inconsistent borders");
            return BUGLE_FALSE;
        }
    }
    return BUGLE_TRUE;
}

/* Tests whether the texture bound to the given target is complete, and
 * prints a warning if not. It is assumed that we are already inside
 * bugle_gl_begin_internal_render. The texture unit is a number from 0, not
 * an enumerant.
 */
static void checks_texture_complete(int unit, GLenum target)
{
    GLint min_filter, base, max, size, width, height;
    GLint format, lformat, border, lborder;
    GLint old_unit = 0;
    GLenum face;
    bugle_bool needs_mip = BUGLE_TRUE;
    bugle_bool success = BUGLE_TRUE;
    int i;

    if (BUGLE_GL_HAS_EXTENSION_GROUP(GL_ARB_multitexture))
    {
        CALL(glGetIntegerv)(GL_ACTIVE_TEXTURE, &old_unit);
        CALL(glActiveTexture)(GL_TEXTURE0 + unit);
    }
    CALL(glGetTexParameteriv)(target, GL_TEXTURE_MIN_FILTER, &min_filter);
    CALL(glGetTexParameteriv)(target, GL_TEXTURE_BASE_LEVEL, &base);
    CALL(glGetTexParameteriv)(target, GL_TEXTURE_MAX_LEVEL, &max);

    switch (min_filter)
    {
    case GL_NEAREST:
    case GL_LINEAR:
        needs_mip = BUGLE_FALSE;
    }

    if (base > max && needs_mip)
        checks_texture_complete_fail(unit, target, "base > max");
    else
        switch (target)
        {
        case GL_TEXTURE_CUBE_MAP:
            CALL(glGetTexLevelParameteriv)(GL_TEXTURE_CUBE_MAP_POSITIVE_X, base, GL_TEXTURE_WIDTH, &size);
            CALL(glGetTexLevelParameteriv)(GL_TEXTURE_CUBE_MAP_POSITIVE_X, base, GL_TEXTURE_INTERNAL_FORMAT, &format);
            CALL(glGetTexLevelParameteriv)(GL_TEXTURE_CUBE_MAP_POSITIVE_X, base, GL_TEXTURE_BORDER, &border);
            for (i = 0; i < 6; i++)
            {
                face = GL_TEXTURE_CUBE_MAP_POSITIVE_X + i;
                CALL(glGetTexLevelParameteriv)(face, base, GL_TEXTURE_WIDTH, &width);
                CALL(glGetTexLevelParameteriv)(face, base, GL_TEXTURE_HEIGHT, &height);
                CALL(glGetTexLevelParameteriv)(face, base, GL_TEXTURE_INTERNAL_FORMAT, &lformat);
                CALL(glGetTexLevelParameteriv)(face, base, GL_TEXTURE_BORDER, &lborder);
                if (width != height)
                {
                    checks_texture_complete_fail(unit, face, "cube map face is not square");
                    success = BUGLE_FALSE;
                    break;
                }
                if (width != size)
                {
                    checks_texture_complete_fail(unit, face, "cube map faces have different sizes");
                    success = BUGLE_FALSE;
                    break;
                }
                if (format != lformat)
                {
                    checks_texture_complete_fail(unit, face, "cube map faces have different internal formats");
                    success = BUGLE_FALSE;
                    break;
                }
                if (border != lborder)
                {
                    checks_texture_complete_fail(unit, face, "cube map faces have different border widths");
                    success = BUGLE_FALSE;
                    break;
                }
            }
            if (!success)
                break;
            for (i = 0; i < 6; i++)
            {
                face = GL_TEXTURE_CUBE_MAP_POSITIVE_X + i;
                if (!checks_texture_face_complete(unit, face, 2, 2, base, max, needs_mip))
                    break;
            }
            break;
        case GL_TEXTURE_3D:
            success = checks_texture_face_complete(unit, target, 3, 3, base, max, needs_mip);
            break;
        case GL_TEXTURE_2D:
        case GL_TEXTURE_RECTANGLE:
            checks_texture_face_complete(unit, target, 2, 2, base, max, needs_mip);
            break;
        case GL_TEXTURE_1D:
            checks_texture_face_complete(unit, target, 1, 1, base, max, needs_mip);
            break;
        case GL_TEXTURE_2D_ARRAY:
            checks_texture_face_complete(unit, target, 3, 2, base, max, needs_mip);
            break;
        case GL_TEXTURE_1D_ARRAY:
            checks_texture_face_complete(unit, target, 2, 1, base, max, needs_mip);
            break;
        }

    if (old_unit != unit)
        CALL(glActiveTexture)(old_unit);
}
#endif /* GL_VERSION_1_1 */

static void checks_completeness(void)
{
    if (bugle_gl_begin_internal_render())
    {
        /* FIXME: should unify this code with glstate.c */
        GLint num_textures = 1;
        if (BUGLE_FALSE)
            (void) 0; /* makes sure the "else if" works regardless of ifdefs */
#if GL_ES_VERSION_2_0 || GL_VERSION_2_0
        else if (BUGLE_GL_HAS_EXTENSION_GROUP(EXTGROUP_texunits))
            CALL(glGetIntegerv)(GL_MAX_TEXTURE_IMAGE_UNITS, &num_textures);
#endif
#if GL_VERSION_ES_CM_1_0 || GL_VERSION_1_3
        else if (BUGLE_GL_HAS_EXTENSION_GROUP(GL_ARB_multitexture))
            CALL(glGetIntegerv)(GL_MAX_TEXTURE_UNITS, &num_textures);
#endif

#if GL_ES_VERSION_2_0 || GL_VERSION_2_0
        if (BUGLE_GL_HAS_EXTENSION_GROUP(GL_ARB_shader_objects))
        {
            GLuint program;
            GLint num_uniforms, u;
            GLenum type;
            GLint size, length, loc, unit;
            char *name;
            GLint target;
            program = bugle_gl_get_current_program();
            if (program)
            {
                bugle_glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &num_uniforms);
                bugle_glGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &length);
                name = BUGLE_NMALLOC(length + 1, char);
                for (u = 0; u < num_uniforms; u++)
                {
                    bugle_glGetActiveUniform(program, u, length + 1, NULL, &size, &type, name);
                    target = 0;
                    switch (type)
                    {
#if BUGLE_GLTYPE_GL
                    case GL_SAMPLER_1D:
                    case GL_SAMPLER_1D_SHADOW:
                        target = GL_TEXTURE_1D;
                        break;
#endif
                    case GL_SAMPLER_2D:
#if BUGLE_GLTYPE_GL
                    case GL_SAMPLER_2D_SHADOW:
#endif
                        target = GL_TEXTURE_2D;
                        break;
                    case GL_SAMPLER_CUBE:
#if BUGLE_GLTYPE_GL
                    case GL_SAMPLER_CUBE_SHADOW:
#endif
                        target = GL_TEXTURE_CUBE_MAP;
                        break;
#if BUGLE_GLTYPE_GL
                    case GL_SAMPLER_2D_RECT:
                    case GL_SAMPLER_2D_RECT_SHADOW:
                        target = GL_TEXTURE_RECTANGLE;
                        break;
#endif
#if BUGLE_GLTYPE_GL
                    case GL_SAMPLER_3D:
                        target = GL_TEXTURE_3D;
                        break;
#endif
#if BUGLE_GLTYPE_GL
                    case GL_SAMPLER_1D_ARRAY:
                    case GL_SAMPLER_1D_ARRAY_SHADOW:
                        target = GL_TEXTURE_1D_ARRAY;
                        break;
                    case GL_SAMPLER_2D_ARRAY:
                    case GL_SAMPLER_2D_ARRAY_SHADOW:
                        target = GL_TEXTURE_2D_ARRAY;
                        break;
#endif
                    }
                    if (target)
                    {
                        loc = bugle_glGetUniformLocation(program, name);
                        bugle_glGetUniformiv(program, loc, &unit);
#ifdef GL_VERSION_2_0 /* not because it doesn't apply to ES, but it can't currently be checked */
                        checks_texture_complete(unit, target);
#endif
                    }
                }
                bugle_free(name);
            }
        }
#endif /* GLES2 || GL2 */
        bugle_gl_end_internal_render("checks_completeness", BUGLE_TRUE);
    }
}

#if HAVE_SIGLONGJMP
static sigjmp_buf checks_buf;
static bugle_thread_lock_t checks_mutex;

static void checks_sigsegv_handler(int sig)
{
    siglongjmp(checks_buf, 1);
}
#endif

/* Writes a log message about a failed pointer check.
 * If attribute is -1, then description contains the name of the thing that was invalid.
 * Otherwise it is a generic attribute.
 * vbo indicates whether this was a client pointer error or a VBO overrun
 * function is the ID of the GL function in which the error occurred.
 */
static void checks_pointer_message(
    const char *description, int attribute, bugle_bool vbo, budgie_function function)
{
    if (attribute != -1)
        bugle_log_printf("checks", "error", BUGLE_LOG_NOTICE,
                         "illegal generic attribute array %d caught in %s (%s); call will be ignored.",
                         attribute,
                         budgie_function_name(function),
                         vbo ? "VBO overrun" : "unreadable memory");
    else
        bugle_log_printf("checks", "error", BUGLE_LOG_NOTICE,
                         "illegal %s caught in %s (%s); call will be ignored.",
                         description ? description : "pointer",
                         budgie_function_name(function),
                         vbo ? "VBO overrun" : "unreadable memory");
}

/* Determines whether the range [data, data + size) is safe to read.
 * Writes a message to the log if not.
 */
static bugle_bool valid_read_range(const void *data, size_t size,
                                   const char *description, int attribute, budgie_function function)
{
    bugle_bool result = BUGLE_TRUE;
#if HAVE_SIGLONGJMP
    struct sigaction act, old_act;
    /* sigsetjmp affects the whole process, so we have to serialise these
     * checks.
     */
    bugle_thread_lock_lock(&checks_mutex);
    if (sigsetjmp(checks_buf, 1) == 0)
    {
        /* Returned directly, so do the checks. We use
         * volatile to force the reads to touch memory.
         */
        const volatile char *cdata;
        size_t i;

        act.sa_handler = checks_sigsegv_handler;
        act.sa_flags = 0;
        sigemptyset(&act.sa_mask);
        while (sigaction(SIGSEGV, &act, &old_act) != 0)
            if (errno != EINTR)
            {
                perror("failed to set SIGSEGV handler");
                exit(1);
            }

        cdata = (const char *) data;
        for (i = 0; i < size; i++)
            (void) cdata[i];
        /* If we get here, all is well. */
    }
    else
    {
        /* Signal handler fired */
        result = BUGLE_FALSE;
    }
    while (sigaction(SIGSEGV, &old_act, NULL) != 0)
        if (errno != EINTR)
        {
            perror("failed to restore SIGSEGV handler");
            exit(1);
        }
    bugle_thread_lock_unlock(&checks_mutex);
#elif BUGLE_PLATFORM_WIN32
    /* TODO: Do something clever with VirtualQueryEx */
#else
    if (data == NULL && size > 0)
        result = BUGLE_FALSE;
#endif

    if (!result)
    {
        checks_pointer_message(description, attribute, BUGLE_FALSE, function);
    }
    return result;
}

/* Checks whether (data, data + size) describes a valid range within a VBO.
 * If not, it records a log message.
 * TODO: use GetBufferParameteri64v where available.
 */
static bugle_bool valid_vbo_range(const void *data, size_t size, GLuint buffer,
                            const char *description, int attribute, budgie_function function)
{
    GLint tmp, bsize;
    size_t start, end;
    bugle_bool result;

    assert(buffer && BUGLE_GL_HAS_EXTENSION_GROUP(GL_ARB_vertex_buffer_object));

    CALL(glGetIntegerv)(GL_ARRAY_BUFFER_BINDING, &tmp);
    CALL(glBindBuffer)(GL_ARRAY_BUFFER, buffer);
    CALL(glGetBufferParameteriv)(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &bsize);
    CALL(glBindBuffer)(GL_ARRAY_BUFFER, tmp);
    start = ((const char *) data - (const char *) NULL);
    end = start + size;

    result = (start <= (size_t) bsize) && (end <= (size_t) bsize);
    if (!result)
    {
        checks_pointer_message(description, attribute, BUGLE_TRUE, function);
    }
    return result;
}

/* Combines valid_read_range and valid_vbo_range, depending on binding */
static bugle_bool valid_range(const void *data, size_t size, GLenum binding,
                              const char *description, int attribute, budgie_function function)
{
    GLint id = 0;
    if (BUGLE_GL_HAS_EXTENSION_GROUP(GL_ARB_vertex_buffer_object))
        CALL(glGetIntegerv)(binding, &id);
    if (id)
        return valid_vbo_range(data, size, id, description, attribute, function);
    else
        return valid_read_range(data, size, description, attribute, function);
}

#if GL_VERSION_ES_CM_1_0 || GL_VERSION_1_1
/* Validates a fixed-function attribute, and writes the log
 * message if it is bad. Returns true if the attribute is good.
 */
static bugle_bool checks_attribute(size_t first, size_t count,
                                   GLenum name,
                                   GLenum size_name, GLint size,
                                   GLenum type_name, budgie_type type,
                                   GLenum stride_name,
                                   GLenum ptr_name, GLenum binding,
                                   const char *description,
                                   budgie_function function)
{
    GLint stride, gltype;
    size_t group_size;
    GLvoid *ptr;
    const char *cptr;

    if (CALL(glIsEnabled)(name))
    {
        if (size_name) CALL(glGetIntegerv)(size_name, &size);
        if (type_name)
        {
            CALL(glGetIntegerv)(type_name, &gltype);
            if (gltype <= 1)
            {
                bugle_log("checks", "warning", BUGLE_LOG_WARNING,
                          "An incorrect value was returned for a vertex array type. "
                          "This is a known bug in Mesa <= 6.5.3. GL_FLOAT will be assumed.");
                gltype = GL_FLOAT;
            }
            type = bugle_gl_type_to_type(gltype);
        }
        CALL(glGetIntegerv)(stride_name, &stride);
        CALL(glGetPointerv)(ptr_name, &ptr);
        group_size = budgie_type_size(type) * size;
        if (!stride) stride = group_size;
        cptr = (const char *) ptr;
        cptr += group_size * first;
        if (!valid_range(cptr, (count - 1) * stride + group_size, binding,
                         description, -1, function))
            return BUGLE_FALSE;
    }
    return BUGLE_TRUE;
}
#endif /* GL_VERSION_1_1 */

#if GL_ES_VERSION_2_0 || GL_VERSION_2_0
/* Like checks_attribute, but for generic vertex array attributes.
 */
static bugle_bool checks_generic_attribute(size_t first, size_t count,
                                           size_t firstInstance, size_t numInstances,
                                           GLint number, budgie_function function)
{
    /* See comment about Mesa below */
    GLint stride, gltype, enabled = GL_RED_BITS, size;
    size_t group_size;
    GLvoid *ptr;
    budgie_type type;
    const char *cptr;
    GLint id;
    bugle_bool result = BUGLE_TRUE;

    CALL(glGetVertexAttribiv)(number, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enabled);
    /* Mesa (up to at least 6.5.1) returns an error when querying attribute 0.
     * In this case clear the error and just assume that everything is valid.
     */
    if (enabled == GL_RED_BITS)
    {
        enabled = GL_FALSE;
        CALL(glGetError)();
    }
    if (enabled)
    {
        size_t attrib_first = first;
        size_t attrib_count = count;
#ifdef GL_ARB_instanced_arrays
        if (BUGLE_GL_HAS_EXTENSION(GL_ARB_instanced_arrays))
        {
            GLint divisor = 0;
            CALL(glGetVertexAttribiv)(number, GL_VERTEX_ATTRIB_ARRAY_DIVISOR_ARB, &divisor);
            if (divisor != 0)
            {
                attrib_first = firstInstance / divisor;
                attrib_count = (firstInstance + numInstances + divisor - 1) / divisor - attrib_first;
            }
        }
#else
        {
            /* Prevent compiler warnings */
            (void) firstInstance;
            (void) numInstances;
        }
#endif
        CALL(glGetVertexAttribiv)(number, GL_VERTEX_ATTRIB_ARRAY_SIZE, &size);
        CALL(glGetVertexAttribiv)(number, GL_VERTEX_ATTRIB_ARRAY_TYPE, &gltype);
        if (gltype <= 1)
        {
            bugle_log("checks", "warning", BUGLE_LOG_WARNING,
                      "An incorrect value was returned for a vertex array type. "
                      "This is a known bug in Mesa <= 6.5.3. GL_FLOAT will be assumed.");
            gltype = GL_FLOAT;
        }
        type = bugle_gl_type_to_type(gltype);
        CALL(glGetVertexAttribiv)(number, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &stride);
        CALL(glGetVertexAttribPointerv)(number, GL_VERTEX_ATTRIB_ARRAY_POINTER, &ptr);
        group_size = budgie_type_size(type) * size;
        if (!stride) stride = group_size;
        cptr = (const char *) ptr;
        cptr += group_size * attrib_first;

        size = (attrib_count - 1) * stride + group_size;
        id = 0;
        if (BUGLE_GL_HAS_EXTENSION_GROUP(GL_ARB_vertex_buffer_object))
        {
            CALL(glGetVertexAttribiv)(number, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &id);
        }
        if (id)
            result = valid_vbo_range(cptr, size, id, NULL, number, function);
        else
            result = valid_read_range(cptr, size, NULL, number, function);
    }
    return result;
}
#endif

static bugle_bool checks_attributes(size_t first, size_t count,
                                    size_t firstInstance, size_t numInstances,
                                    budgie_function function)
{
    bugle_bool result = BUGLE_TRUE;

    if (!count) return BUGLE_TRUE;
#if GL_VERSION_ES_CM_1_0 || GL_VERSION_1_1
    result = result && checks_attribute(first, count,
                     GL_VERTEX_ARRAY,
                     GL_VERTEX_ARRAY_SIZE, 0,
                     GL_VERTEX_ARRAY_TYPE, 0,
                     GL_VERTEX_ARRAY_STRIDE,
                     GL_VERTEX_ARRAY_POINTER,
                     GL_VERTEX_ARRAY_BUFFER_BINDING,
                     "vertex array", function);
    result = result && checks_attribute(first, count,
                     GL_NORMAL_ARRAY,
                     0, 3,
                     GL_NORMAL_ARRAY_TYPE, NULL_TYPE,
                     GL_NORMAL_ARRAY_STRIDE,
                     GL_NORMAL_ARRAY_POINTER,
                     GL_NORMAL_ARRAY_BUFFER_BINDING,
                     "normal array", function);
    result = result && checks_attribute(first, count,
                     GL_COLOR_ARRAY,
                     GL_COLOR_ARRAY_SIZE, 0,
                     GL_COLOR_ARRAY_TYPE, NULL_TYPE,
                     GL_COLOR_ARRAY_STRIDE,
                     GL_COLOR_ARRAY_POINTER,
                     GL_COLOR_ARRAY_BUFFER_BINDING,
                     "color array", function);
#endif
#ifdef GL_VERSION_1_1
    result = result && checks_attribute(first, count,
                     GL_INDEX_ARRAY,
                     0, 1,
                     GL_INDEX_ARRAY_TYPE, NULL_TYPE,
                     GL_INDEX_ARRAY_STRIDE,
                     GL_INDEX_ARRAY_POINTER,
                     GL_INDEX_ARRAY_BUFFER_BINDING,
                     "index array", function);
    result = result && checks_attribute(first, count,
                     GL_EDGE_FLAG_ARRAY,
                     0, 1,
                     0, BUDGIE_TYPE_ID(9GLboolean),
                     GL_EDGE_FLAG_ARRAY_STRIDE,
                     GL_EDGE_FLAG_ARRAY_POINTER,
                     GL_EDGE_FLAG_ARRAY_BUFFER_BINDING,
                     "edge flag array", function);
    /* FIXME: there are others (fog, secondary colour, ?) */

    if (BUGLE_GL_HAS_EXTENSION_GROUP(GL_ARB_multitexture))
    {
        GLenum i;
        GLint texunits, old;

        CALL(glGetIntegerv)(GL_MAX_TEXTURE_UNITS, &texunits);
        CALL(glGetIntegerv)(GL_CLIENT_ACTIVE_TEXTURE, &old);
        for (i = GL_TEXTURE0; result && i < GL_TEXTURE0 + (GLenum) texunits; i++)
        {
            CALL(glClientActiveTexture)(i);
            result = result && checks_attribute(first, count,
                             GL_TEXTURE_COORD_ARRAY,
                             GL_TEXTURE_COORD_ARRAY_SIZE, 0,
                             GL_TEXTURE_COORD_ARRAY_TYPE, 0,
                             GL_TEXTURE_COORD_ARRAY_STRIDE,
                             GL_TEXTURE_COORD_ARRAY_POINTER,
                             GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING,
                             "texture coordinate array", function);
        }
        CALL(glClientActiveTexture)(old);
    }
    else
    {
        result = result && checks_attribute(first, count,
                         GL_TEXTURE_COORD_ARRAY,
                         GL_TEXTURE_COORD_ARRAY_SIZE, 0,
                         GL_TEXTURE_COORD_ARRAY_TYPE, 0,
                         GL_TEXTURE_COORD_ARRAY_STRIDE,
                         GL_TEXTURE_COORD_ARRAY_POINTER,
                         GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING,
                         "texture coordinate array", function);
    }
#endif /* GLES1 || GL */

#if GL_ES_VERSION_2_0 || GL_VERSION_2_0
    if (BUGLE_GL_HAS_EXTENSION_GROUP(EXTGROUP_vertex_attrib))
    {
        GLint attribs, i;

        CALL(glGetIntegerv)(GL_MAX_VERTEX_ATTRIBS, &attribs);
        for (i = 0; i < attribs; i++)
            result = result && checks_generic_attribute(first, count, firstInstance, numInstances, i, function);
    }
#endif
    return result;
}

/* Determines the range of indices encoded in <indices>, and returns it
 * through min_out and max_out. Returns false if the parameters are invalid.
 * TODO: handle primitive restart.
 */
static bugle_bool checks_min_max(GLsizei count, GLenum gltype, const GLvoid *indices,
                                 GLuint *min_out, GLuint *max_out)
{
    GLuint *out;
    GLsizei i;
    GLuint min, max;
    budgie_type type;
    GLvoid *vbo_indices = NULL;

    if (count <= 0)
        return BUGLE_FALSE;
    if (gltype != GL_UNSIGNED_INT
        && gltype != GL_UNSIGNED_SHORT
        && gltype != GL_UNSIGNED_BYTE)
        return BUGLE_FALSE; /* It will just generate a GL error and be ignored */
    type = bugle_gl_type_to_type(gltype);

    /* Check for element array buffer */
    if (BUGLE_GL_HAS_EXTENSION_GROUP(GL_ARB_vertex_buffer_object))
    {
        GLint id;
        CALL(glGetIntegerv)(GL_ELEMENT_ARRAY_BUFFER_BINDING, &id);
        if (id)
        {
#if GL_VERSION_ES_CM_1_1 || GL_ES_VERSION_2_0
            /* FIXME-GLES: save data on load */
            return BUGLE_FALSE;
#else
            GLint mapped;
            size_t size;
            /* We are not allowed to call glGetBufferSubDataARB on a
             * mapped buffer. Fortunately, if the buffer is mapped, the
             * call is illegal and should generate INVALID_OPERATION anyway.
             */
            CALL(glGetBufferParameteriv)(GL_ELEMENT_ARRAY_BUFFER,
                                         GL_BUFFER_MAPPED,
                                         &mapped);
            if (mapped)
                return BUGLE_FALSE;

            size = count * budgie_type_size(type);
            vbo_indices = bugle_malloc(size);
            CALL(glGetBufferSubData)(GL_ELEMENT_ARRAY_BUFFER,
                                    (const char *) indices - (const char *) NULL,
                                    size, vbo_indices);
            indices = vbo_indices;
#endif /* !GLES */
        }
    }

    out = BUGLE_NMALLOC(count, GLuint);
    budgie_type_convert(out, bugle_gl_type_to_type(GL_UNSIGNED_INT), indices, type, count);
    min = max = out[0];
    for (i = 0; i < count; i++)
    {
        if (out[i] < min) min = out[i];
        if (out[i] > max) max = out[i];
    }
    if (min_out) *min_out = min;
    if (max_out) *max_out = max;
    bugle_free(out);
    if (vbo_indices) bugle_free(vbo_indices);
    return BUGLE_TRUE;
}

static bugle_bool checks_glDrawArrays(function_call *call, const callback_data *data)
{
    if (*call->glDrawArrays.arg1 < 0)
    {
        bugle_log("checks", "error", BUGLE_LOG_NOTICE,
                  "glDrawArrays called with a negative argument; call will be ignored.");
        return BUGLE_FALSE;
    }

    if (bugle_gl_in_begin_end())
        return BUGLE_TRUE;

    checks_completeness();
    return checks_attributes(*call->glDrawArrays.arg1, *call->glDrawArrays.arg2,
                             0, 1, call->generic.id);
}

static bugle_bool checks_glDrawElements(function_call *call, const callback_data *data)
{
    GLsizei count;
    GLenum type;
    const GLvoid *indices;
    GLuint min, max;

    if (bugle_gl_in_begin_end())
        return BUGLE_TRUE;

    count = *call->glDrawElements.arg1;
    type = *call->glDrawElements.arg2;
    indices = *call->glDrawElements.arg3;
    checks_completeness();
    if (!valid_range(indices, count * bugle_gl_type_to_size(type),
        GL_ELEMENT_ARRAY_BUFFER_BINDING,
        "index array", -1, call->generic.id))
        return BUGLE_FALSE;
    if (checks_min_max(count, type, indices, &min, &max))
        if (!checks_attributes(min, max - min + 1, 0, 1, call->generic.id))
            return BUGLE_FALSE;
    return BUGLE_TRUE;
}

#ifdef GL_VERSION_1_1
static bugle_bool checks_glDrawRangeElements(function_call *call, const callback_data *data)
{
    GLsizei count;
    GLenum type;
    const GLvoid *indices;
    GLuint min, max;

    if (bugle_gl_in_begin_end())
        return BUGLE_TRUE;

    count = *call->glDrawRangeElements.arg3;
    type = *call->glDrawRangeElements.arg4;
    indices = *call->glDrawRangeElements.arg5;
    checks_completeness();
    if (!valid_range(indices, count * bugle_gl_type_to_size(type),
        GL_ELEMENT_ARRAY_BUFFER_BINDING,
        "index array", -1, call->generic.id))
        return BUGLE_FALSE;

    if (checks_min_max(count, type, indices, &min, &max))
    {
        if (min < *call->glDrawRangeElements.arg1
            || max > *call->glDrawRangeElements.arg2)
        {
            bugle_log("checks", "error", BUGLE_LOG_NOTICE,
                      "glDrawRangeElements indices fall outside range; call will be ignored.");
            return BUGLE_FALSE;
        }
    }
    min = *call->glDrawRangeElements.arg1;
    max = *call->glDrawRangeElements.arg2;
    if (!checks_attributes(min, max - min + 1, 0, 1, call->generic.id))
        return BUGLE_FALSE;
    return BUGLE_TRUE;
}

static bugle_bool checks_glMultiDrawArrays(function_call *call, const callback_data *data)
{
    const GLint *first_ptr;
    const GLsizei *count_ptr;
    GLsizei count, i;

    if (bugle_gl_in_begin_end())
        return BUGLE_TRUE;

    count = *call->glMultiDrawArrays.arg3;
    first_ptr = *call->glMultiDrawArrays.arg1;
    count_ptr = *call->glMultiDrawArrays.arg2;

    checks_completeness();
    if (!valid_read_range(first_ptr, sizeof(GLint) * count, "first array", -1, call->generic.id))
        return BUGLE_FALSE;
    if (!valid_read_range(count_ptr, sizeof(GLsizei) * count, "count array", -1, call->generic.id))
        return BUGLE_FALSE;
    for (i = 0; i < count; i++)
        if (!checks_attributes(first_ptr[i], count_ptr[i], 0, 1, call->generic.id))
            return BUGLE_FALSE;
    return BUGLE_TRUE;
}

static bugle_bool checks_glMultiDrawElements(function_call *call, const callback_data *data)
{
    const GLsizei *count_ptr;
    const GLvoid * const * indices_ptr;
    GLsizei count, i;
    GLenum type;
    GLuint min, max;

    if (bugle_gl_in_begin_end())
        return BUGLE_TRUE;

    count = *call->glMultiDrawElements.arg4;
    type = *call->glMultiDrawElements.arg2;
    count_ptr = *call->glMultiDrawElements.arg1;
    indices_ptr = *call->glMultiDrawElements.arg3;

    checks_completeness();
    if (!valid_read_range(count_ptr, sizeof(GLsizei) * count, "count array", -1, call->generic.id))
        return BUGLE_FALSE;
    if (!valid_read_range(indices_ptr, sizeof(GLvoid *) * count, "indices array", -1, call->generic.id))
        return BUGLE_FALSE;

    for (i = 0; i < count; i++)
    {
        if (!valid_range(indices_ptr[i], count_ptr[i] * bugle_gl_type_to_size(type),
                         GL_ELEMENT_ARRAY_BUFFER_BINDING,
                         "index array", -1, call->generic.id))
            return BUGLE_FALSE;
        if (checks_min_max(count, type, indices_ptr[i], &min, &max))
            if (!checks_attributes(min, max - min + 1, 0, 1, call->generic.id))
                return BUGLE_FALSE;
    }
    return BUGLE_TRUE;
}

#ifdef GL_EXT_draw_instanced
static bugle_bool checks_glDrawArraysInstanced(function_call *call, const callback_data *data)
{
    if (*call->glDrawArraysInstancedEXT.arg1 < 0)
    {
        bugle_log("checks", "error", BUGLE_LOG_NOTICE,
                  "glDrawArraysInstanced called with a negative argument; call will be ignored.");
        return BUGLE_FALSE;
    }

    if (bugle_gl_in_begin_end())
        return BUGLE_TRUE;

    checks_completeness();
    return checks_attributes(*call->glDrawArraysInstancedEXT.arg1, *call->glDrawArraysInstancedEXT.arg2,
                             0, *call->glDrawArraysInstancedEXT.arg3,
                             call->generic.id);
}

static bugle_bool checks_glDrawElementsInstanced(function_call *call, const callback_data *data)
{
    GLsizei count;
    GLenum type;
    const GLvoid *indices;
    GLuint min, max;

    if (bugle_gl_in_begin_end())
        return BUGLE_TRUE;

    count = *call->glDrawElementsInstancedEXT.arg1;
    type = *call->glDrawElementsInstancedEXT.arg2;
    indices = *call->glDrawElementsInstancedEXT.arg3;
    checks_completeness();
    if (!valid_range(indices, count * bugle_gl_type_to_size(type),
        GL_ELEMENT_ARRAY_BUFFER_BINDING,
        "index array", -1, call->generic.id))
        return BUGLE_FALSE;
    if (checks_min_max(count, type, indices, &min, &max))
        if (!checks_attributes(min, max - min + 1,
                               0, *call->glDrawElementsInstanced.arg4,
                               call->generic.id))
            return BUGLE_FALSE;
    return BUGLE_TRUE;
}
#endif /* GL_EXT_draw_instanced */

/* OpenGL defines certain calls to be illegal inside glBegin/glEnd but
 * allows undefined behaviour when it happens (these are client-side calls).
 */
static bugle_bool checks_no_begin_end(function_call *call, const callback_data *data)
{
    if (bugle_gl_in_begin_end())
    {
        bugle_log_printf("checks", "error", BUGLE_LOG_NOTICE,
                         "%s called inside glBegin/glEnd; call will be ignored.",
                         budgie_function_name(call->generic.id));
        return BUGLE_FALSE;
    }
    else
        return BUGLE_TRUE;
}

/* Vertex drawing commands have undefined behaviour outside of begin/end. */
static bugle_bool checks_begin_end(function_call *call, const callback_data *data)
{
    const char *name;

    if (!bugle_gl_in_begin_end())
    {
        /* VertexAttrib commands are ok if they are not affecting attrib 0 */
        name = budgie_function_name(call->generic.id);
        if (strncmp(name, "glVertexAttrib", 14) == 0)
        {
            GLuint attrib;

            attrib = *(const GLuint *) call->generic.args[0];
            if (attrib) return BUGLE_TRUE;
        }
        bugle_log_printf("checks", "error", BUGLE_LOG_NOTICE,
                         "%s called outside glBegin/glEnd; call will be ignored.",
                         name);
        return BUGLE_FALSE;
    }
    else
        return BUGLE_TRUE;
}

static bugle_bool checks_glArrayElement(function_call *call, const callback_data *data)
{
    if (*call->glArrayElement.arg0 < 0)
    {
        bugle_log("checks", "error", BUGLE_LOG_NOTICE,
                  "glArrayElement called with a negative argument; call will be ignored.");
        return BUGLE_FALSE;
    }
    return BUGLE_TRUE;
}

/* glMultiTexCoord with an illegal texture has undefined behaviour */
static bugle_bool checks_glMultiTexCoord(function_call *call, const callback_data *data)
{
    GLenum texture;
    GLint max = 0;

    texture = *(GLenum *) call->generic.args[0];
    if (bugle_gl_begin_internal_render())
    {
        if (BUGLE_GL_HAS_EXTENSION_GROUP(EXTGROUP_texunits))
        {
            CALL(glGetIntegerv)(GL_MAX_TEXTURE_COORDS, &max);
            /* NVIDIA ship a driver that just generates an error on this
             * call on NV20. Instead of borking we check for the error and
             * fall back to GL_MAX_TEXTURE_UNITS.
             */
            CALL(glGetError)();
        }
        if (!max)
            CALL(glGetIntegerv)(GL_MAX_TEXTURE_UNITS, &max);
        bugle_gl_end_internal_render("checks_glMultiTexCoord", BUGLE_TRUE);
    }

    /* FIXME: This is the most likely scenario i.e. inside glBegin/glEnd.
     * We should pre-lookup the implementation dependent values so that
     * we can actually check things here.
     */
    if (!max) return BUGLE_TRUE;

    if (texture < GL_TEXTURE0 || texture >= GL_TEXTURE0 + (GLenum) max)
    {
        bugle_log_printf("checks", "error", BUGLE_LOG_NOTICE,
                         "%s called with out of range texture unit; call will be ignored.",
                         budgie_function_name(call->generic.id));
        return BUGLE_FALSE;
    }
    return BUGLE_TRUE;
}
#endif /* GL_VERSION_1_1 */

static bugle_bool checks_initialise(filter_set *handle)
{
    filter *f;

    f = bugle_filter_new(handle, "checks");
    /* Pointer checks */
    bugle_filter_catches(f, "glDrawArrays", BUGLE_FALSE, checks_glDrawArrays);
    bugle_filter_catches(f, "glDrawElements", BUGLE_FALSE, checks_glDrawElements);
#ifdef GL_VERSION_1_1
    bugle_filter_catches(f, "glDrawRangeElements", BUGLE_FALSE, checks_glDrawRangeElements);
    bugle_filter_catches(f, "glMultiDrawArrays", BUGLE_FALSE, checks_glMultiDrawArrays);
    bugle_filter_catches(f, "glMultiDrawElements", BUGLE_FALSE, checks_glMultiDrawElements);
#ifdef GL_EXT_draw_instanced
    bugle_filter_catches(f, "glDrawArraysInstancedEXT", BUGLE_FALSE, checks_glDrawArraysInstanced);
    bugle_filter_catches(f, "glDrawElementsInstancedEXT", BUGLE_FALSE, checks_glDrawElementsInstanced);
#endif
    /* Checks that we are outside begin/end */
    bugle_filter_catches(f, "glEnableClientState", BUGLE_FALSE, checks_no_begin_end);
    bugle_filter_catches(f, "glDisableClientState", BUGLE_FALSE, checks_no_begin_end);
    bugle_filter_catches(f, "glPushClientAttrib", BUGLE_FALSE, checks_no_begin_end);
    bugle_filter_catches(f, "glPopClientAttrib", BUGLE_FALSE, checks_no_begin_end);
    bugle_filter_catches(f, "glColorPointer", BUGLE_FALSE, checks_no_begin_end);
    bugle_filter_catches(f, "glFogCoordPointer", BUGLE_FALSE, checks_no_begin_end);
    bugle_filter_catches(f, "glEdgeFlagPointer", BUGLE_FALSE, checks_no_begin_end);
    bugle_filter_catches(f, "glIndexPointer", BUGLE_FALSE, checks_no_begin_end);
    bugle_filter_catches(f, "glNormalPointer", BUGLE_FALSE, checks_no_begin_end);
    bugle_filter_catches(f, "glTexCoordPointer", BUGLE_FALSE, checks_no_begin_end);
    bugle_filter_catches(f, "glSecondaryColorPointer", BUGLE_FALSE, checks_no_begin_end);
    bugle_filter_catches(f, "glVertexPointer", BUGLE_FALSE, checks_no_begin_end);
    bugle_filter_catches(f, "glVertexAttribPointer", BUGLE_FALSE, checks_no_begin_end);
    bugle_filter_catches(f, "glClientActiveTexture", BUGLE_FALSE, checks_no_begin_end);
    bugle_filter_catches(f, "glInterleavedArrays", BUGLE_FALSE, checks_no_begin_end);
    bugle_filter_catches(f, "glPixelStorei", BUGLE_FALSE, checks_no_begin_end);
    bugle_filter_catches(f, "glPixelStoref", BUGLE_FALSE, checks_no_begin_end);
    /* Checks that we are inside begin/end */
    bugle_gl_filter_catches_drawing_immediate(f, BUGLE_FALSE, checks_begin_end);
    /* This call has undefined behaviour if given a negative argument */
    bugle_filter_catches(f, "glArrayElement", BUGLE_FALSE, checks_glArrayElement);
    /* Other */
    bugle_filter_catches(f, "glMultiTexCoord1s", BUGLE_FALSE, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord1i", BUGLE_FALSE, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord1f", BUGLE_FALSE, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord1d", BUGLE_FALSE, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord2s", BUGLE_FALSE, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord2i", BUGLE_FALSE, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord2f", BUGLE_FALSE, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord2d", BUGLE_FALSE, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord3s", BUGLE_FALSE, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord3i", BUGLE_FALSE, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord3f", BUGLE_FALSE, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord3d", BUGLE_FALSE, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord4s", BUGLE_FALSE, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord4i", BUGLE_FALSE, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord4f", BUGLE_FALSE, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord4d", BUGLE_FALSE, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord1sv", BUGLE_FALSE, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord1iv", BUGLE_FALSE, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord1fv", BUGLE_FALSE, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord1dv", BUGLE_FALSE, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord2sv", BUGLE_FALSE, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord2iv", BUGLE_FALSE, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord2fv", BUGLE_FALSE, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord2dv", BUGLE_FALSE, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord3sv", BUGLE_FALSE, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord3iv", BUGLE_FALSE, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord3fv", BUGLE_FALSE, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord3dv", BUGLE_FALSE, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord4sv", BUGLE_FALSE, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord4iv", BUGLE_FALSE, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord4fv", BUGLE_FALSE, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord4dv", BUGLE_FALSE, checks_glMultiTexCoord);
#endif /* GL 1.1 */

    /* FIXME: still perhaps to do:
     * - check for passing a glMapBuffer region to a command
     */

    /* We try to push this early, since it would defeat the whole thing if
     * bugle crashed while examining the data in another filter.
     */
    bugle_filter_order("checks", "invoke");
    bugle_filter_order("checks", "stats");
    bugle_filter_order("checks", "trace");
    bugle_filter_order("checks", "trackcontext");
    bugle_filter_order("checks", "glbeginend");
    bugle_filter_order("checks", "gldisplaylist");
    return BUGLE_TRUE;
}

void bugle_initialise_filter_library(void)
{
    static const filter_set_info checks_info =
    {
        "checks",
        checks_initialise,
        NULL,
        NULL,
        NULL,
        NULL,
        "checks for illegal values passed to OpenGL in some places"
    };

#if HAVE_SIGLONGJMP
    bugle_thread_lock_init(&checks_mutex);
#endif

    bugle_filter_set_new(&checks_info);

    bugle_gl_filter_set_renders("checks");
    bugle_filter_set_depends("checks", "glextensions");
}
