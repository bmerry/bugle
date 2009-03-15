/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2009  Bruce Merry
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
#define GL_GLEXT_PROTOTYPES
#include <stdbool.h>
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
#include "common/threads.h"
#include <budgie/addresses.h>
#include <budgie/types.h>
#include <budgie/reflect.h>
#include "xalloc.h"
#include "lock.h"

#ifdef GL_VERSION_1_1
static void checks_texture_complete_fail(int unit, GLenum target, const char *reason)
{
    const char *target_name;

    target_name = bugle_api_enum_name(target);
    if (!target_name) target_name = "<unknown target>";
    bugle_log_printf("checks", "texture", BUGLE_LOG_NOTICE,
                     "GL_TEXTURE%d / %s: incomplete texture (%s)",
                     unit, target_name, reason);
}

/* Tests whether the given texture face (bound to the active texture unit)
 * is complete (unit is passed only for error reporting). The face is either
 * a target, or a single face of a cube map. Returns true if complete,
 * false (and a log message) if not.  It assumes that the minification filter
 * requires mipmapping and that base <= max.
 */
static bool checks_texture_face_complete(GLuint unit, GLenum face, int dims,
                                         int base, int max, bool needs_mip)
{
    GLint sizes[3], border, format;
    int d, lvl;
    const GLenum dim_enum[3] =
    {
        GL_TEXTURE_WIDTH,
        GL_TEXTURE_HEIGHT,
        GL_TEXTURE_DEPTH
    };

    /* FIXME: cannot query depth unless extension is present */
    for (d = 0; d < dims; d++)
    {
        CALL(glGetTexLevelParameteriv)(face, base, dim_enum[d], &sizes[d]);
        if (sizes[d] <= 0)
        {
            checks_texture_complete_fail(unit, face,
                                         "base level does not have positive dimensions");
            return false;
        }
    }

    if (!needs_mip)
        return true;

    CALL(glGetTexLevelParameteriv)(face, base, GL_TEXTURE_BORDER, &border);
    CALL(glGetTexLevelParameteriv)(face, base, GL_TEXTURE_INTERNAL_FORMAT, &format);
    for (lvl = base + 1; lvl <= max; lvl++)
    {
        GLint lformat, lborder;
        bool more = false;
        for (d = 0; d < dims; d++)
            if (sizes[d] > 1)
            {
                more = true;
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
                return false;
            }
            if (size != sizes[d])
            {
                checks_texture_complete_fail(unit, face,
                                             "incorrect size in mipmap sequence");
                return false;
            }
        }
        CALL(glGetTexLevelParameteriv)(face, lvl, GL_TEXTURE_INTERNAL_FORMAT, &lformat);
        CALL(glGetTexLevelParameteriv)(face, lvl, GL_TEXTURE_BORDER, &lborder);
        if (format != lformat)
        {
            checks_texture_complete_fail(unit, face,
                                         "inconsistent internal formats");
            return false;
        }
        if (border != lborder)
        {
            checks_texture_complete_fail(unit, face,
                                         "inconsistent borders");
            return false;
        }
    }
    return true;
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
    bool needs_mip = true;
    bool success = true;
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
        needs_mip = false;
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
                    success = false;
                    break;
                }
                if (width != size)
                {
                    checks_texture_complete_fail(unit, face, "cube map faces have different sizes");
                    success = false;
                    break;
                }
                if (format != lformat)
                {
                    checks_texture_complete_fail(unit, face, "cube map faces have different internal formats");
                    success = false;
                    break;
                }
                if (border != lborder)
                {
                    checks_texture_complete_fail(unit, face, "cube map faces have different border widths");
                    success = false;
                    break;
                }
            }
            if (!success)
                break;
            for (i = 0; i < 6; i++)
            {
                face = GL_TEXTURE_CUBE_MAP_POSITIVE_X + i;
                if (!checks_texture_face_complete(unit, face, 2, base, max, needs_mip))
                    break;
            }
            break;
        case GL_TEXTURE_3D:
            success = checks_texture_face_complete(unit, target, 3, base, max, needs_mip);
            break;
        case GL_TEXTURE_2D:
        case GL_TEXTURE_RECTANGLE_ARB:
            checks_texture_face_complete(unit, target, 2, base, max, needs_mip);
            break;
        case GL_TEXTURE_1D:
            checks_texture_face_complete(unit, target, 1, base, max, needs_mip);
            break;
        }

    if (old_unit != unit)
        CALL(glActiveTexture)(old_unit);
}
#endif /* GL_VERSION_1_1 */

static void checks_completeness()
{
    if (bugle_gl_begin_internal_render())
    {
        /* FIXME: should unify this code with glstate.c */
        GLint num_textures = 1;
        if (false)
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
                name = xcharalloc(length + 1);
                for (u = 0; u < num_uniforms; u++)
                {
                    bugle_glGetActiveUniform(program, u, length + 1, NULL, &size, &type, name);
                    target = 0;
                    switch (type)
                    {
                    case GL_SAMPLER_2D:        target = GL_TEXTURE_2D; break;
                    case GL_SAMPLER_CUBE:      target = GL_TEXTURE_CUBE_MAP; break;
#ifdef GL_VERSION_2_0
                    case GL_SAMPLER_1D:        target = GL_TEXTURE_1D; break;
                    case GL_SAMPLER_3D:        target = GL_TEXTURE_3D; break;
                    case GL_SAMPLER_1D_SHADOW: target = GL_TEXTURE_1D; break;
                    case GL_SAMPLER_2D_SHADOW: target = GL_TEXTURE_2D; break;
                    case GL_SAMPLER_2D_RECT_ARB:        target = GL_TEXTURE_RECTANGLE_ARB; break;
                    case GL_SAMPLER_2D_RECT_SHADOW_ARB: target = GL_TEXTURE_RECTANGLE_ARB; break;
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
                free(name);
            }
        }
#endif /* GLES2 || GL2 */
        bugle_gl_end_internal_render("checks_completeness", true);
    }
}

/* This is set to some description of the thing being tested, and if it
 * causes a SIGSEGV it is used to describe the error.
 */
static const char *checks_error;
static int checks_error_attribute;  /* generic attribute number for error */
static bool checks_error_vbo;

#if HAVE_SIGLONGJMP
static sigjmp_buf checks_buf;
gl_lock_define_initialized(static, checks_mutex)

static void checks_sigsegv_handler(int sig)
{
    siglongjmp(checks_buf, 1);
}
#endif

static void checks_pointer_message(const char *function)
{
    if (checks_error_attribute != -1)
        bugle_log_printf("checks", "error", BUGLE_LOG_NOTICE,
                         "illegal generic attribute array %d caught in %s (%s); call will be ignored.",
                         checks_error_attribute,
                         function,
                         checks_error_vbo ? "VBO overrun" : "unreadable memory");
    else
        bugle_log_printf("checks", "error", BUGLE_LOG_NOTICE,
                         "illegal %s caught in %s (%s); call will be ignored.",
                         checks_error ? checks_error : "pointer",
                         function,
                         checks_error_vbo ? "VBO overrun" : "unreadable memory");
}

/* Just reads every byte of the given address range. We use the volatile
 * keyword to (hopefully) force the read.
 */
static void checks_memory(size_t size, const void *data)
{
#if HAVE_SIGLONGJMP
    volatile char dummy;
    const char *cdata;
    size_t i;

    checks_error_vbo = false;
    cdata = (const char *) data;
    for (i = 0; i < size; i++)
        dummy = cdata[i];
#endif
}

static void checks_buffer_vbo(size_t size, const void *data,
                              GLuint buffer)
{
    GLint tmp, bsize;
    size_t end;

    checks_error_vbo = true;
    assert(buffer && !bugle_gl_in_begin_end() && BUGLE_GL_HAS_EXTENSION_GROUP(GL_ARB_vertex_buffer_object));

    CALL(glGetIntegerv)(GL_ARRAY_BUFFER_BINDING, &tmp);
    CALL(glBindBuffer)(GL_ARRAY_BUFFER, buffer);
    CALL(glGetBufferParameteriv)(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &bsize);
    CALL(glBindBuffer)(GL_ARRAY_BUFFER, tmp);
    end = ((const char *) data - (const char *) NULL) + size;
    if (end > (size_t) bsize)
        bugle_thread_raise(SIGSEGV);
}

/* Like checks_memory, but handles buffer objects */
static void checks_buffer(size_t size, const void *data,
                          GLenum binding)
{
    GLint id = 0;
    if (!bugle_gl_in_begin_end() && BUGLE_GL_HAS_EXTENSION_GROUP(GL_ARB_vertex_buffer_object))
        CALL(glGetIntegerv)(binding, &id);
    if (id)
        checks_buffer_vbo(size, data, id);
    else
        checks_memory(size, data);
}

#if GL_VERSION_ES_CM_1_0 || GL_VERSION_1_1
static void checks_attribute(size_t first, size_t count,
                             const char *text, GLenum name,
                             GLenum size_name, GLint size,
                             GLenum type_name, budgie_type type,
                             GLenum stride_name,
                             GLenum ptr_name, GLenum binding)
{
    GLint stride, gltype;
    size_t group_size;
    GLvoid *ptr;
    const char *cptr;

    if (CALL(glIsEnabled)(name))
    {
        checks_error = text;
        checks_error_attribute = -1;
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
        checks_buffer((count - 1) * stride + group_size, cptr,
                      binding);
    }
}
#endif /* GL_VERSION_1_1 */

#if GL_ES_VERSION_2_0 || GL_VERSION_2_0
static void checks_generic_attribute(size_t first, size_t count,
                                     GLint number)
{
    /* See comment about Mesa below */
    GLint stride, gltype, enabled = GL_RED_BITS, size;
    size_t group_size;
    GLvoid *ptr;
    budgie_type type;
    const char *cptr;
    GLint id;

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
        checks_error = NULL;
        checks_error_attribute = number;
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
        cptr += group_size * first;

        size = (count - 1) * stride + group_size;
        id = 0;
        if (!bugle_gl_in_begin_end() && BUGLE_GL_HAS_EXTENSION_GROUP(GL_ARB_vertex_buffer_object))
        {
            CALL(glGetVertexAttribiv)(number, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &id);
        }
        if (id)
            checks_buffer_vbo(size, cptr, id);
        else
            checks_memory(size, cptr);
    }
}
#endif

static void checks_attributes(size_t first, size_t count)
{
    GLenum i;

    if (!count) return;
#if GL_VERSION_ES_CM_1_0 || GL_VERSION_1_1
    checks_attribute(first, count,
                     "vertex array", GL_VERTEX_ARRAY,
                     GL_VERTEX_ARRAY_SIZE, 0,
                     GL_VERTEX_ARRAY_TYPE, 0,
                     GL_VERTEX_ARRAY_STRIDE,
                     GL_VERTEX_ARRAY_POINTER,
                     GL_VERTEX_ARRAY_BUFFER_BINDING);
    checks_attribute(first, count,
                     "normal array", GL_NORMAL_ARRAY,
                     0, 3,
                     GL_NORMAL_ARRAY_TYPE, NULL_TYPE,
                     GL_NORMAL_ARRAY_STRIDE,
                     GL_NORMAL_ARRAY_POINTER,
                     GL_NORMAL_ARRAY_BUFFER_BINDING);
    checks_attribute(first, count,
                     "color array", GL_COLOR_ARRAY,
                     GL_COLOR_ARRAY_SIZE, 0,
                     GL_COLOR_ARRAY_TYPE, NULL_TYPE,
                     GL_COLOR_ARRAY_STRIDE,
                     GL_COLOR_ARRAY_POINTER,
                     GL_COLOR_ARRAY_BUFFER_BINDING);
#endif
#ifdef GL_VERSION_1_1
    checks_attribute(first, count,
                     "index array", GL_INDEX_ARRAY,
                     0, 1,
                     GL_INDEX_ARRAY_TYPE, NULL_TYPE,
                     GL_INDEX_ARRAY_STRIDE,
                     GL_INDEX_ARRAY_POINTER,
                     GL_INDEX_ARRAY_BUFFER_BINDING);
    checks_attribute(first, count,
                     "edge flag array", GL_EDGE_FLAG_ARRAY,
                     0, 1,
                     0, BUDGIE_TYPE_ID(9GLboolean),
                     GL_EDGE_FLAG_ARRAY_STRIDE,
                     GL_EDGE_FLAG_ARRAY_POINTER,
                     GL_EDGE_FLAG_ARRAY_BUFFER_BINDING);
    /* FIXME: there are others (fog, secondary colour, ?) */

    /* FIXME: if there is a failure, the current texture unit will be wrong */
    if (BUGLE_GL_HAS_EXTENSION_GROUP(GL_ARB_multitexture))
    {
        GLint texunits, old;

        CALL(glGetIntegerv)(GL_MAX_TEXTURE_UNITS, &texunits);
        CALL(glGetIntegerv)(GL_CLIENT_ACTIVE_TEXTURE, &old);
        for (i = GL_TEXTURE0; i < GL_TEXTURE0 + (GLenum) texunits; i++)
        {
            CALL(glClientActiveTexture)(i);
            checks_attribute(first, count,
                             "texture coordinate array", GL_TEXTURE_COORD_ARRAY,
                             GL_TEXTURE_COORD_ARRAY_SIZE, 0,
                             GL_TEXTURE_COORD_ARRAY_TYPE, 0,
                             GL_TEXTURE_COORD_ARRAY_STRIDE,
                             GL_TEXTURE_COORD_ARRAY_POINTER,
                             GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING);
        }
        CALL(glClientActiveTexture)(old);
    }
    else
    {
        checks_attribute(first, count,
                         "texture coordinate array", GL_TEXTURE_COORD_ARRAY,
                         GL_TEXTURE_COORD_ARRAY_SIZE, 0,
                         GL_TEXTURE_COORD_ARRAY_TYPE, 0,
                         GL_TEXTURE_COORD_ARRAY_STRIDE,
                         GL_TEXTURE_COORD_ARRAY_POINTER,
                         GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING);
    }
#endif /* GLES1 || GL */

#if GL_ES_VERSION_2_0 || GL_VERSION_2_0
    if (BUGLE_GL_HAS_EXTENSION_GROUP(EXTGROUP_vertex_attrib))
    {
        GLint attribs, i;

        CALL(glGetIntegerv)(GL_MAX_VERTEX_ATTRIBS, &attribs);
        for (i = 0; i < attribs; i++)
            checks_generic_attribute(first, count, i);
    }
#endif
}

/* FIXME: breaks when using an element array buffer for indices */
static void checks_min_max(GLsizei count, GLenum gltype, const GLvoid *indices,
                           GLuint *min_out, GLuint *max_out)
{
    GLuint *out;
    GLsizei i;
    GLuint min, max;
    budgie_type type;
    GLvoid *vbo_indices = NULL;

    if (count <= 0) return;
    if (gltype != GL_UNSIGNED_INT
        && gltype != GL_UNSIGNED_SHORT
        && gltype != GL_UNSIGNED_BYTE)
        return; /* It will just generate a GL error and be ignored */
    if (bugle_gl_in_begin_end()) return;
    type = bugle_gl_type_to_type(gltype);

    /* Check for element array buffer */
    if (BUGLE_GL_HAS_EXTENSION_GROUP(GL_ARB_vertex_buffer_object))
    {
        GLint id, mapped;
        size_t size;
        CALL(glGetIntegerv)(GL_ELEMENT_ARRAY_BUFFER_BINDING, &id);
        if (id)
        {
#if GL_VERSION_ES_CM_1_1 || GL_ES_VERSION_2_0
            /* FIXME-GLES: save data on load */
            return;
#else
            /* We are not allowed to call glGetBufferSubDataARB on a
             * mapped buffer. Fortunately, if the buffer is mapped, the
             * call is illegal and should generate INVALID_OPERATION anyway.
             */
            CALL(glGetBufferParameteriv)(GL_ELEMENT_ARRAY_BUFFER,
                                         GL_BUFFER_MAPPED,
                                         &mapped);
            if (mapped) return;

            size = count * budgie_type_size(type);
            vbo_indices = xmalloc(size);
            CALL(glGetBufferSubData)(GL_ELEMENT_ARRAY_BUFFER,
                                    (const char *) indices - (const char *) NULL,
                                    size, vbo_indices);
            indices = vbo_indices;
#endif // 
        }
    }

    out = XNMALLOC(count, GLuint);
    budgie_type_convert(out, bugle_gl_type_to_type(GL_UNSIGNED_INT), indices, type, count);
    min = max = out[0];
    for (i = 0; i < count; i++)
    {
        if (out[i] < min) min = out[i];
        if (out[i] > max) max = out[i];
    }
    if (min_out) *min_out = min;
    if (max_out) *max_out = max;
    free(out);
    if (vbo_indices) free(vbo_indices);
}

/* Note: this cannot be a function, because a jmpbuf becomes invalid
 * once the function calling setjmp exits. It is also written in a
 * funny way, so that it can be used as if (CHECKS_START()) { ... },
 * which will be true if there was an error.
 *
 * The entire checks method is fundamentally non-reentrant, so
 * we protect it with a big lock.
 *
 */
#if HAVE_SIGLONGJMP
#define CHECKS_START() 1) { \
    struct sigaction act, old_act; \
    volatile bool ret = true; \
    \
    gl_lock_lock(checks_mutex); \
    checks_error = NULL; \
    checks_error_attribute = -1; \
    checks_error_vbo = false; \
    if (sigsetjmp(checks_buf, 1) == 1) ret = false; \
    if (ret) \
    { \
        act.sa_handler = checks_sigsegv_handler; \
        act.sa_flags = 0; \
        sigemptyset(&act.sa_mask); \
        while (sigaction(SIGSEGV, &act, &old_act) != 0) \
            if (errno != EINTR) \
            { \
                perror("failed to set SIGSEGV handler"); \
                exit(1); \
            } \
    } \
    if (!ret

#define CHECKS_STOP() \
    while (sigaction(SIGSEGV, &old_act, NULL) != 0) \
        if (errno != EINTR) \
        { \
            perror("failed to restore SIGSEGV handler"); \
            exit(1); \
        } \
    gl_lock_unlock(checks_mutex); \
    return ret; \
    } else (void) 0

#else /* !HAVE_SIGLONGJMP */
#define CHECKS_START() 1) { \
    bool ret = true; \
    if (0
#define CHECKS_STOP() \
    return ret; \
    } else (void) 0
#endif

static bool checks_glDrawArrays(function_call *call, const callback_data *data)
{
    if (*call->glDrawArrays.arg1 < 0)
    {
        bugle_log("checks", "error", BUGLE_LOG_NOTICE,
                  "glDrawArrays called with a negative argument; call will be ignored.");
        return false;
    }

    checks_completeness();
    if (CHECKS_START())
    {
        checks_pointer_message("glDrawArrays");
    }
    else
    {
        checks_attributes(*call->glDrawArrays.arg1,
                          *call->glDrawArrays.arg2);
    }
    CHECKS_STOP();
}

static bool checks_glDrawElements(function_call *call, const callback_data *data)
{
    checks_completeness();
    if (CHECKS_START())
    {
        checks_pointer_message("glDrawElements");
    }
    else
    {
        GLsizei count;
        GLenum type;
        const GLvoid *indices;
        GLuint min, max;

        checks_error = "index array";
        checks_error_attribute = -1;
        count = *call->glDrawElements.arg1;
        type = *call->glDrawElements.arg2;
        indices = *call->glDrawElements.arg3;
        checks_buffer(count * bugle_gl_type_to_size(type),
                      indices,
                      GL_ELEMENT_ARRAY_BUFFER_BINDING);
        checks_min_max(count, type, indices, &min, &max);
        checks_attributes(min, max - min + 1);
    }
    CHECKS_STOP();
}

#ifdef GL_VERSION_1_1
static bool checks_glDrawRangeElements(function_call *call, const callback_data *data)
{
    checks_completeness();
    if (CHECKS_START())
    {
        checks_pointer_message("glDrawRangeElements");
    }
    else
    {
        GLsizei count;
        GLenum type;
        const GLvoid *indices;
        GLuint min, max;

        checks_error = "index array";
        checks_error_attribute = -1;
        count = *call->glDrawRangeElements.arg3;
        type = *call->glDrawRangeElements.arg4;
        indices = *call->glDrawRangeElements.arg5;
        checks_buffer(count * bugle_gl_type_to_size(type),
                      indices,
                      GL_ELEMENT_ARRAY_BUFFER_BINDING);
        checks_min_max(count, type, indices, &min, &max);
        if (min < *call->glDrawRangeElements.arg1
            || max > *call->glDrawRangeElements.arg2)
        {
            bugle_log("checks", "error", BUGLE_LOG_NOTICE,
                      "glDrawRangeElements indices fall outside range; call will be ignored.");
            ret = false;
        }
        else
        {
            min = *call->glDrawRangeElements.arg1;
            max = *call->glDrawRangeElements.arg2;
            checks_attributes(min, max - min + 1);
        }
    }
    CHECKS_STOP();
}

static bool checks_glMultiDrawArrays(function_call *call, const callback_data *data)
{
    checks_completeness();
    if (CHECKS_START())
    {
        checks_pointer_message("glMultiDrawArrays");
    }
    else
    {
        const GLint *first_ptr;
        const GLsizei *count_ptr;
        GLsizei count, i;

        count = *call->glMultiDrawArrays.arg3;
        first_ptr = *call->glMultiDrawArrays.arg1;
        count_ptr = *call->glMultiDrawArrays.arg2;

        checks_error = "first array";
        checks_error_attribute = -1;
        checks_memory(sizeof(GLint) * count, first_ptr);
        checks_error = "count array";
        checks_error_attribute = -1;
        checks_memory(sizeof(GLsizei) * count, count_ptr);

        for (i = 0; i < count; i++)
            checks_attributes(first_ptr[i], count_ptr[i]);
    }
    CHECKS_STOP();
}

static bool checks_glMultiDrawElements(function_call *call, const callback_data *data)
{
    checks_completeness();
    if (CHECKS_START())
    {
        checks_pointer_message("glMultiDrawElements");
    }
    else
    {
        const GLsizei *count_ptr;
        const GLvoid * const * indices_ptr;
        GLsizei count, i;
        GLenum type;
        GLuint min, max;

        count = *call->glMultiDrawElements.arg4;
        type = *call->glMultiDrawElements.arg2;
        count_ptr = *call->glMultiDrawElements.arg1;
        indices_ptr = *call->glMultiDrawElements.arg3;

        checks_error = "count array";
        checks_error_attribute = -1;
        checks_memory(sizeof(GLsizei) * count, count_ptr);
        checks_error = "indices array";
        checks_error_attribute = -1;
        checks_memory(sizeof(GLvoid *) * count, indices_ptr);
        checks_error = "index array";
        checks_error_attribute = -1;

        for (i = 0; i < count; i++)
        {
            checks_buffer(count_ptr[i] * bugle_gl_type_to_size(type),
                          indices_ptr[i],
                          GL_ELEMENT_ARRAY_BUFFER_BINDING);
            checks_min_max(count_ptr[i], type, indices_ptr[i], &min, &max);
            checks_attributes(min, max - min + 1);
        }
    }
    CHECKS_STOP();
}

/* OpenGL defines certain calls to be illegal inside glBegin/glEnd but
 * allows undefined behaviour when it happens (these are client-side calls).
 */
static bool checks_no_begin_end(function_call *call, const callback_data *data)
{
    if (bugle_gl_in_begin_end())
    {
        bugle_log_printf("checks", "error", BUGLE_LOG_NOTICE,
                         "%s called inside glBegin/glEnd; call will be ignored.",
                         budgie_function_name(call->generic.id));
        return false;
    }
    else return true;
}

/* Vertex drawing commands have undefined behaviour outside of begin/end. */
static bool checks_begin_end(function_call *call, const callback_data *data)
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
            if (attrib) return true;
        }
        bugle_log_printf("checks", "error", BUGLE_LOG_NOTICE,
                         "%s called outside glBegin/glEnd; call will be ignored.",
                         name);
        return false;
    }
    else
        return true;
}

static bool checks_glArrayElement(function_call *call, const callback_data *data)
{
    if (*call->glArrayElement.arg0 < 0)
    {
        bugle_log("checks", "error", BUGLE_LOG_NOTICE,
                  "glArrayElement called with a negative argument; call will be ignored.");
        return false;
    }
    return true;
}

/* glMultiTexCoord with an illegal texture has undefined behaviour */
static bool checks_glMultiTexCoord(function_call *call, const callback_data *data)
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
        bugle_gl_end_internal_render("checks_glMultiTexCoord", true);
    }

    /* FIXME: This is the most likely scenario i.e. inside glBegin/glEnd.
     * We should pre-lookup the implementation dependent values so that
     * we can actually check things here.
     */
    if (!max) return true;

    if (texture < GL_TEXTURE0 || texture >= GL_TEXTURE0 + (GLenum) max)
    {
        bugle_log_printf("checks", "error", BUGLE_LOG_NOTICE,
                         "%s called with out of range texture unit; call will be ignored.",
                         budgie_function_name(call->generic.id));
        return false;
    }
    return true;
}
#endif /* GL_VERSION_1_1 */

static bool checks_initialise(filter_set *handle)
{
    filter *f;

    f = bugle_filter_new(handle, "checks");
    /* Pointer checks */
    bugle_filter_catches(f, "glDrawArrays", false, checks_glDrawArrays);
    bugle_filter_catches(f, "glDrawElements", false, checks_glDrawElements);
#ifdef GL_VERSION_1_1
    bugle_filter_catches(f, "glDrawRangeElements", false, checks_glDrawRangeElements);
    bugle_filter_catches(f, "glMultiDrawArrays", false, checks_glMultiDrawArrays);
    bugle_filter_catches(f, "glMultiDrawElements", false, checks_glMultiDrawElements);
    /* Checks that we are outside begin/end */
    bugle_filter_catches(f, "glEnableClientState", false, checks_no_begin_end);
    bugle_filter_catches(f, "glDisableClientState", false, checks_no_begin_end);
    bugle_filter_catches(f, "glPushClientAttrib", false, checks_no_begin_end);
    bugle_filter_catches(f, "glPopClientAttrib", false, checks_no_begin_end);
    bugle_filter_catches(f, "glColorPointer", false, checks_no_begin_end);
    bugle_filter_catches(f, "glFogCoordPointer", false, checks_no_begin_end);
    bugle_filter_catches(f, "glEdgeFlagPointer", false, checks_no_begin_end);
    bugle_filter_catches(f, "glIndexPointer", false, checks_no_begin_end);
    bugle_filter_catches(f, "glNormalPointer", false, checks_no_begin_end);
    bugle_filter_catches(f, "glTexCoordPointer", false, checks_no_begin_end);
    bugle_filter_catches(f, "glSecondaryColorPointer", false, checks_no_begin_end);
    bugle_filter_catches(f, "glVertexPointer", false, checks_no_begin_end);
    bugle_filter_catches(f, "glVertexAttribPointer", false, checks_no_begin_end);
    bugle_filter_catches(f, "glClientActiveTexture", false, checks_no_begin_end);
    bugle_filter_catches(f, "glInterleavedArrays", false, checks_no_begin_end);
    bugle_filter_catches(f, "glPixelStorei", false, checks_no_begin_end);
    bugle_filter_catches(f, "glPixelStoref", false, checks_no_begin_end);
    /* Checks that we are inside begin/end */
    bugle_gl_filter_catches_drawing_immediate(f, false, checks_begin_end);
    /* This call has undefined behaviour if given a negative argument */
    bugle_filter_catches(f, "glArrayElement", false, checks_glArrayElement);
    /* Other */
    bugle_filter_catches(f, "glMultiTexCoord1s", false, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord1i", false, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord1f", false, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord1d", false, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord2s", false, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord2i", false, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord2f", false, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord2d", false, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord3s", false, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord3i", false, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord3f", false, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord3d", false, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord4s", false, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord4i", false, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord4f", false, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord4d", false, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord1sv", false, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord1iv", false, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord1fv", false, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord1dv", false, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord2sv", false, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord2iv", false, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord2fv", false, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord2dv", false, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord3sv", false, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord3iv", false, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord3fv", false, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord3dv", false, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord4sv", false, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord4iv", false, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord4fv", false, checks_glMultiTexCoord);
    bugle_filter_catches(f, "glMultiTexCoord4dv", false, checks_glMultiTexCoord);
#endif

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
    return true;
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
    bugle_filter_set_new(&checks_info);

    bugle_gl_filter_set_renders("checks");
    bugle_filter_set_depends("checks", "glextensions");
}
