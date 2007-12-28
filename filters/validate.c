/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2007  Bruce Merry
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
#include <GL/gl.h>
#include <bugle/filters.h>
#include <bugle/glutils.h>
#include <bugle/gltypes.h>
#include <bugle/gldump.h>
#include <bugle/tracker.h>
#include <bugle/log.h>
#include "common/threads.h"
#include <budgie/addresses.h>
#include <budgie/types.h>
#include <budgie/reflect.h>
#include "src/glexts.h"
#include "xalloc.h"
#include "lock.h"

static bool trap = false;
static filter_set *error_handle = NULL;
static object_view error_context_view, error_call_view;

GLenum bugle_call_get_error_internal(object *call_object)
{
    GLenum *call_error;
    call_error = bugle_object_get_data(call_object, error_call_view);
    return call_error ? *call_error : GL_NO_ERROR;
}

static bool error_callback(function_call *call, const callback_data *data)
{
    GLenum error;
    GLenum *stored_error;
    GLenum *call_error;

    stored_error = bugle_object_get_current_data(bugle_context_class, error_context_view);
    call_error = bugle_object_get_current_data(bugle_call_class, error_call_view);
    *call_error = GL_NO_ERROR;

    if (budgie_function_name(call->generic.id)[2] == 'X') return true; /* GLX */
    if (call->generic.group == BUDGIE_GROUP_ID(glGetError))
    {
        /* We hope that it returns GL_NO_ERROR, since otherwise something
         * slipped through our own net. If not, we return it to the app
         * rather than whatever we have saved. Also, we must make sure to
         * return nothing else inside begin/end.
         */
        if (*call->glGetError.retn != GL_NO_ERROR)
        {
            const char *name;
            name = bugle_gl_enum_to_token(*call->glGetError.retn);
            if (name)
                bugle_log_printf("error", "callback", BUGLE_LOG_WARNING,
                                 "glGetError() returned %s when GL_NO_ERROR was expected",
                                 name);
            else
                bugle_log_printf("error", "callback", BUGLE_LOG_WARNING,
                                 "glGetError() returned %#08x when GL_NO_ERROR was expected",
                                 (unsigned int) *call->glGetError.retn);
        }
        else if (bugle_in_begin_end())
        {
            *call_error = GL_INVALID_OPERATION;
        }
        else if (*stored_error)
        {
            *call->glGetError.retn = *stored_error;
            *stored_error = GL_NO_ERROR;
        }
    }
    else if (!bugle_in_begin_end())
    {
        /* Note: we deliberately don't call begin_internal_render here,
         * since it will beat us to calling glGetError().
         */
        while ((error = CALL(glGetError)()) != GL_NO_ERROR)
        {
            if (stored_error && !*stored_error)
                *stored_error = error;
            *call_error = error;
            if (trap && bugle_filter_set_is_active(data->filter_set_handle))
            {
                fflush(stderr);
                /* SIGTRAP is technically a BSD extension, and various
                 * versions of FreeBSD do weird things (e.g. 4.8 will
                 * never define it if _POSIX_SOURCE is defined). Rather
                 * than try all possibilities we just SIGABRT instead.
                 */
#ifdef SIGTRAP
                bugle_thread_raise(SIGTRAP);
#else
                abort();
#endif
            }
        }
    }
    return true;
}

static bool error_initialise(filter_set *handle)
{
    filter *f;

    error_handle = handle;
    f = bugle_filter_new(handle, "error");
    bugle_filter_catches_all(f, true, error_callback);
    bugle_filter_order("invoke", "error");
    bugle_filter_post_queries_begin_end("error");
    /* We don't call filter_post_renders, because that would make the
     * error filter-set depend on itself.
     */
    error_context_view = bugle_object_view_new(bugle_context_class,
                                               NULL,
                                               NULL,
                                               sizeof(GLenum));
    error_call_view = bugle_object_view_new(bugle_call_class,
                                            NULL,
                                            NULL,
                                            sizeof(GLenum));
    return true;
}

static bool showerror_callback(function_call *call, const callback_data *data)
{
    GLenum error;
    if ((error = bugle_call_get_error_internal(data->call_object)) != GL_NO_ERROR)
    {
        const char *name;
        name = bugle_gl_enum_to_token(error);
        if (name)
            bugle_log_printf("showerror", "gl", BUGLE_LOG_NOTICE,
                             "%s in %s", name,
                             budgie_function_name(call->generic.id));
        else
            bugle_log_printf("showerror", "gl", BUGLE_LOG_NOTICE,
                             "%#08x in %s", (unsigned int) error,
                             budgie_function_name(call->generic.id));
    }
    return true;
}

static bool showerror_initialise(filter_set *handle)
{
    filter *f;

    f = bugle_filter_new(handle, "showerror");
    bugle_filter_catches_all(f, false, showerror_callback);
    bugle_filter_order("error", "showerror");
    bugle_filter_order("invoke", "showerror");
    return true;
}

/* Stack unwind hack, to get a usable stack trace after a segfault inside
 * the OpenGL driver, if it was compiled with -fomit-frame-pointer (such
 * as the NVIDIA drivers). This implementation violates the requirement
 * that the function calling setjmp shouldn't return (see setjmp(3)), but
 * It Works For Me (tm). Unfortunately there doesn't seem to be a way
 * to satisfy the requirements of setjmp without breaking the nicely
 * modular filter system.
 *
 * This is also grossly thread-unsafe, but since the semantics for
 * signal delivery in multi-threaded programs are so vague anyway, we
 * won't worry about it too much.
 */
static struct sigaction unwindstack_old_sigsegv_act;
static sigjmp_buf unwind_buf;

static void unwindstack_sigsegv_handler(int sig)
{
    siglongjmp(unwind_buf, 1);
}

static bool unwindstack_pre_callback(function_call *call, const callback_data *data)
{
    struct sigaction act;

    if (sigsetjmp(unwind_buf, 1))
    {
        fputs("A segfault occurred, which was caught by the unwindstack\n"
              "filter-set. It will now be rethrown. If you are running in\n"
              "a debugger, you should get a useful stack trace. Do not\n"
              "try to continue again, as gdb will get confused.\n", stderr);
        fflush(stderr);
        /* avoid hitting the same handler */
        while (sigaction(SIGSEGV, &unwindstack_old_sigsegv_act, NULL) != 0)
            if (errno != EINTR)
            {
                perror("failed to set SIGSEGV handler");
                exit(1);
            }
        bugle_thread_raise(SIGSEGV);
        exit(1); /* make sure we don't recover */
    }
    act.sa_handler = unwindstack_sigsegv_handler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);

    while (sigaction(SIGSEGV, &act, &unwindstack_old_sigsegv_act) != 0)
        if (errno != EINTR)
        {
            perror("failed to set SIGSEGV handler");
            exit(1);
        }
    return true;
}

static bool unwindstack_post_callback(function_call *call, const callback_data *data)
{
    while (sigaction(SIGSEGV, &unwindstack_old_sigsegv_act, NULL) != 0)
        if (errno != EINTR)
        {
            perror("failed to restore SIGSEGV handler");
            exit(1);
        }
    return true;
}

static bool unwindstack_initialise(filter_set *handle)
{
    filter *f;

    f = bugle_filter_new(handle, "unwindstack_pre");
    bugle_filter_catches_all(f, false, unwindstack_pre_callback);
    f = bugle_filter_new(handle, "unwindstack_post");
    bugle_filter_catches_all(f, false, unwindstack_post_callback);
    bugle_filter_order("invoke", "unwindstack_post");
    bugle_filter_order("unwindstack_pre", "invoke");
    return true;
}

/* This is set to some description of the thing being tested, and if it
 * causes a SIGSEGV it is used to describe the error.
 */
static const char *checks_error;
static int checks_error_attribute;  /* generic attribute number for error */
static bool checks_error_vbo;
static sigjmp_buf checks_buf;
gl_lock_define_initialized(static, checks_mutex);

static void checks_sigsegv_handler(int sig)
{
    siglongjmp(checks_buf, 1);
}

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
    volatile char dummy;
    const char *cdata;
    size_t i;

    checks_error_vbo = false;
    cdata = (const char *) data;
    for (i = 0; i < size; i++)
        dummy = cdata[i];
}

/* We don't want to have to make *calls* to checks_buffer conditional
 * on GL_ARB_vertex_buffer_object, but the bindings are only defined by
 * that extension. Instead we wrap them in this macro.
 */
#ifdef GL_ARB_vertex_buffer_object
# define VBO_ENUM(x) (x)
#else
# define VBO_ENUM(x) (GL_NONE)
#endif

#ifdef GL_ARB_vertex_buffer_object
static void checks_buffer_vbo(size_t size, const void *data,
                              GLuint buffer)
{
    GLint tmp, bsize;
    size_t end;

    checks_error_vbo = true;
    assert(buffer && !bugle_in_begin_end() && bugle_gl_has_extension(BUGLE_GL_ARB_vertex_buffer_object));

    CALL(glGetIntegerv)(GL_ARRAY_BUFFER_BINDING_ARB, &tmp);
    CALL(glBindBufferARB)(GL_ARRAY_BUFFER_ARB, buffer);
    CALL(glGetBufferParameterivARB)(GL_ARRAY_BUFFER_ARB, GL_BUFFER_SIZE_ARB, &bsize);
    CALL(glBindBufferARB)(GL_ARRAY_BUFFER_ARB, tmp);
    end = ((const char *) data - (const char *) NULL) + size;
    if (end > (size_t) bsize)
        bugle_thread_raise(SIGSEGV);
}
#endif

/* Like checks_memory, but handles buffer objects */
static void checks_buffer(size_t size, const void *data,
                          GLenum binding)
{
#ifdef GL_ARB_vertex_buffer_object
    GLint id = 0;
    if (!bugle_in_begin_end() && bugle_gl_has_extension(BUGLE_GL_ARB_vertex_buffer_object))
        CALL(glGetIntegerv)(binding, &id);
    if (id) checks_buffer_vbo(size, data, id);
    else
#endif
    {
        checks_memory(size, data);
    }
}

static void checks_attribute(size_t first, size_t count,
                             const char *text, GLenum name,
                             GLenum size_name, GLint size,
                             GLenum type_name, GLenum type,
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
            type = gltype;
        }
        CALL(glGetIntegerv)(stride_name, &stride);
        CALL(glGetPointerv)(ptr_name, &ptr);
        group_size = bugle_gl_type_to_size(type) * size;
        if (!stride) stride = group_size;
        cptr = (const char *) ptr;
        cptr += group_size * first;
        checks_buffer((count - 1) * stride + group_size, cptr,
                      binding);
    }
}

#ifdef GL_ARB_vertex_program
static void checks_generic_attribute(size_t first, size_t count,
                                     GLint number)
{
    /* See comment about Mesa below */
    GLint stride, gltype, enabled = GL_RED_BITS, size;
    size_t group_size;
    GLvoid *ptr;
    budgie_type type;
    const char *cptr;
#ifdef GL_ARB_vertex_buffer_object
    GLint id;
#endif

    CALL(glGetVertexAttribivARB)(number, GL_VERTEX_ATTRIB_ARRAY_ENABLED_ARB, &enabled);
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
        CALL(glGetVertexAttribivARB)(number, GL_VERTEX_ATTRIB_ARRAY_SIZE_ARB, &size);
        CALL(glGetVertexAttribivARB)(number, GL_VERTEX_ATTRIB_ARRAY_TYPE_ARB, &gltype);
        if (gltype <= 1)
        {
            bugle_log("checks", "warning", BUGLE_LOG_WARNING,
                      "An incorrect value was returned for a vertex array type. "
                      "This is a known bug in Mesa <= 6.5.3. GL_FLOAT will be assumed.");
            gltype = GL_FLOAT;
        }
        type = bugle_gl_type_to_type(gltype);
        CALL(glGetVertexAttribivARB)(number, GL_VERTEX_ATTRIB_ARRAY_STRIDE_ARB, &stride);
        CALL(glGetVertexAttribPointervARB)(number, GL_VERTEX_ATTRIB_ARRAY_POINTER_ARB, &ptr);
        group_size = budgie_type_size(type) * size;
        if (!stride) stride = group_size;
        cptr = (const char *) ptr;
        cptr += group_size * first;

        size = (count - 1) * stride + group_size;
#ifdef GL_ARB_vertex_buffer_object
        id = 0;
        if (!bugle_in_begin_end() && bugle_gl_has_extension(BUGLE_GL_ARB_vertex_buffer_object))
            CALL(glGetVertexAttribivARB)(number, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING_ARB, &id);
        if (id) checks_buffer_vbo(size, cptr, id);
        else
#endif
        {
            checks_memory(size, cptr);
        }
    }
}
#endif /* GL_ARB_vertex_program */

static void checks_attributes(size_t first, size_t count)
{
    GLenum i;

    if (!count) return;
    checks_attribute(first, count,
                     "vertex array", GL_VERTEX_ARRAY,
                     GL_VERTEX_ARRAY_SIZE, 0,
                     GL_VERTEX_ARRAY_TYPE, 0,
                     GL_VERTEX_ARRAY_STRIDE,
                     GL_VERTEX_ARRAY_POINTER,
                     VBO_ENUM(GL_VERTEX_ARRAY_BUFFER_BINDING_ARB));
    checks_attribute(first, count,
                     "normal array", GL_NORMAL_ARRAY,
                     0, 3,
                     GL_NORMAL_ARRAY_TYPE, 0,
                     GL_NORMAL_ARRAY_STRIDE,
                     GL_NORMAL_ARRAY_POINTER,
                     VBO_ENUM(GL_NORMAL_ARRAY_BUFFER_BINDING_ARB));
    checks_attribute(first, count,
                     "color array", GL_COLOR_ARRAY,
                     GL_COLOR_ARRAY_SIZE, 0,
                     GL_COLOR_ARRAY_TYPE, 0,
                     GL_COLOR_ARRAY_STRIDE,
                     GL_COLOR_ARRAY_POINTER,
                     VBO_ENUM(GL_COLOR_ARRAY_BUFFER_BINDING_ARB));
    checks_attribute(first, count,
                     "index array", GL_INDEX_ARRAY,
                     0, 1,
                     GL_INDEX_ARRAY_TYPE, 0,
                     GL_INDEX_ARRAY_STRIDE,
                     GL_INDEX_ARRAY_POINTER,
                     VBO_ENUM(GL_INDEX_ARRAY_BUFFER_BINDING_ARB));
    checks_attribute(first, count,
                     "edge flag array", GL_EDGE_FLAG_ARRAY,
                     0, 1,
                     0, GL_BOOL,
                     GL_EDGE_FLAG_ARRAY_STRIDE,
                     GL_EDGE_FLAG_ARRAY_POINTER,
                     VBO_ENUM(GL_EDGE_FLAG_ARRAY_BUFFER_BINDING_ARB));
    /* FIXME: there are others (fog, secondary colour, ?) */

#ifdef GL_ARB_multitexture
    /* FIXME: if there is a failure, the current texture unit will be wrong */
    if (bugle_gl_has_extension(BUGLE_GL_ARB_multitexture))
    {
        GLint texunits, old;

        CALL(glGetIntegerv)(GL_MAX_TEXTURE_UNITS_ARB, &texunits);
        CALL(glGetIntegerv)(GL_CLIENT_ACTIVE_TEXTURE_ARB, &old);
        for (i = GL_TEXTURE0_ARB; i < GL_TEXTURE0_ARB + (GLenum) texunits; i++)
        {
            CALL(glClientActiveTextureARB)(i);
            checks_attribute(first, count,
                             "texture coordinate array", GL_TEXTURE_COORD_ARRAY,
                             GL_TEXTURE_COORD_ARRAY_SIZE, 0,
                             GL_TEXTURE_COORD_ARRAY_TYPE, 0,
                             GL_TEXTURE_COORD_ARRAY_STRIDE,
                             GL_TEXTURE_COORD_ARRAY_POINTER,
                             VBO_ENUM(GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING_ARB));
        }
        CALL(glClientActiveTextureARB)(old);
    }
    else
#endif
    {
        checks_attribute(first, count,
                         "texture coordinate array", GL_TEXTURE_COORD_ARRAY,
                         GL_TEXTURE_COORD_ARRAY_SIZE, 0,
                         GL_TEXTURE_COORD_ARRAY_TYPE, 0,
                         GL_TEXTURE_COORD_ARRAY_STRIDE,
                         GL_TEXTURE_COORD_ARRAY_POINTER,
                         VBO_ENUM(GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING_ARB));
    }

#ifdef GL_ARB_vertex_program
    if (bugle_gl_has_extension(BUGLE_GL_ARB_vertex_program))
    {
        GLint attribs, i;

        CALL(glGetIntegerv)(GL_MAX_VERTEX_ATTRIBS_ARB, &attribs);
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
    if (bugle_in_begin_end()) return;
    type = bugle_gl_type_to_type(gltype);

    /* Check for element array buffer */
#ifdef GL_ARB_vertex_buffer_object
    if (bugle_gl_has_extension(BUGLE_GL_ARB_vertex_buffer_object))
    {
        GLint id, mapped;
        size_t size;
        CALL(glGetIntegerv)(GL_ELEMENT_ARRAY_BUFFER_BINDING_ARB, &id);
        if (id)
        {
            /* We are not allowed to call glGetBufferSubDataARB on a
             * mapped buffer. Fortunately, if the buffer is mapped, the
             * call is illegal and should generate INVALID_OPERATION anyway.
             */
            CALL(glGetBufferParameterivARB)(GL_ELEMENT_ARRAY_BUFFER_ARB,
                                           GL_BUFFER_MAPPED_ARB,
                                           &mapped);
            if (mapped) return;

            size = count * budgie_type_size(type);
            vbo_indices = xmalloc(size);
            CALL(glGetBufferSubDataARB)(GL_ELEMENT_ARRAY_BUFFER_ARB,
                                       (const char *) indices - (const char *) NULL,
                                       size, vbo_indices);
            indices = vbo_indices;
        }
    }
#endif

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

static bool checks_glDrawArrays(function_call *call, const callback_data *data)
{
    if (*call->glDrawArrays.arg1 < 0)
    {
        bugle_log("checks", "error", BUGLE_LOG_NOTICE,
                  "glDrawArrays called with a negative argument; call will be ignored.");
        return false;
    }

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
                      VBO_ENUM(GL_ELEMENT_ARRAY_BUFFER_BINDING_ARB));
        checks_min_max(count, type, indices, &min, &max);
        checks_attributes(min, max - min + 1);
    }
    CHECKS_STOP();
}

#ifdef GL_EXT_draw_range_elements
static bool checks_glDrawRangeElements(function_call *call, const callback_data *data)
{
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
        count = *call->glDrawRangeElementsEXT.arg3;
        type = *call->glDrawRangeElementsEXT.arg4;
        indices = *call->glDrawRangeElementsEXT.arg5;
        checks_buffer(count * bugle_gl_type_to_size(type),
                      indices,
                      VBO_ENUM(GL_ELEMENT_ARRAY_BUFFER_BINDING_ARB));
        checks_min_max(count, type, indices, &min, &max);
        if (min < *call->glDrawRangeElementsEXT.arg1
            || max > *call->glDrawRangeElementsEXT.arg2)
        {
            bugle_log("checks", "error", BUGLE_LOG_NOTICE,
                      "glDrawRangeElements indices fall outside range; call will be ignored.");
            ret = false;
        }
        else
        {
            min = *call->glDrawRangeElementsEXT.arg1;
            max = *call->glDrawRangeElementsEXT.arg2;
            checks_attributes(min, max - min + 1);
        }
    }
    CHECKS_STOP();
}
#endif

#ifdef GL_EXT_multi_draw_arrays
static bool checks_glMultiDrawArrays(function_call *call, const callback_data *data)
{
    if (CHECKS_START())
    {
        checks_pointer_message("glMultiDrawArrays");
    }
    else
    {
        const GLint *first_ptr;
        const GLsizei *count_ptr;
        GLsizei count, i;

        count = *call->glMultiDrawArraysEXT.arg3;
        first_ptr = *call->glMultiDrawArraysEXT.arg1;
        count_ptr = *call->glMultiDrawArraysEXT.arg2;

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
                          VBO_ENUM(GL_ELEMENT_ARRAY_BUFFER_BINDING_ARB));
            checks_min_max(count_ptr[i], type, indices_ptr[i], &min, &max);
            checks_attributes(min, max - min + 1);
        }
    }
    CHECKS_STOP();
}
#endif

/* OpenGL defines certain calls to be illegal inside glBegin/glEnd but
 * allows undefined behaviour when it happens (these are client-side calls).
 */
static bool checks_no_begin_end(function_call *call, const callback_data *data)
{
    if (bugle_in_begin_end())
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

    if (!bugle_in_begin_end())
    {
        /* VertexAttrib commands are ok if they are not affecting attrib 0 */
        name = budgie_function_name(call->generic.id);
#ifdef GL_ARB_vertex_program
        if (strncmp(name, "glVertexAttrib", 14) == 0)
        {
            GLuint attrib;

            attrib = *(const GLuint *) call->generic.args[0];
            if (attrib) return true;
        }
#endif
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
#ifdef GL_ARB_multitexture
static bool checks_glMultiTexCoord(function_call *call, const callback_data *data)
{
    GLenum texture;
    GLint max = 0;

    texture = *(GLenum *) call->generic.args[0];
    if (bugle_begin_internal_render())
    {
#ifdef GL_ARB_fragment_program
        if (bugle_gl_has_extension_group(BUGLE_EXTGROUP_texunits))
        {
            CALL(glGetIntegerv)(GL_MAX_TEXTURE_COORDS_ARB, &max);
            /* NVIDIA ship a driver that just generates an error on this
             * call on NV20. Instead of borking we check for the error and
             * fall back to GL_MAX_TEXTURE_UNITS.
             */
            CALL(glGetError)();
        }
#endif
        if (!max)
            CALL(glGetIntegerv)(GL_MAX_TEXTURE_UNITS_ARB, &max);
        bugle_end_internal_render("checks_glMultiTexCoord", true);
    }

    /* FIXME: This is the most likely scenario i.e. inside glBegin/glEnd.
     * We should pre-lookup the implementation dependent values so that
     * we can actually check things here.
     */
    if (!max) return true;

    if (texture < GL_TEXTURE0_ARB || texture >= GL_TEXTURE0_ARB + (GLenum) max)
    {
        bugle_log_printf("checks", "error", BUGLE_LOG_NOTICE,
                         "%s called with out of range texture unit; call will be ignored.",
                         budgie_function_name(call->generic.id));
        return false;
    }
    return true;
}
#endif

static bool checks_initialise(filter_set *handle)
{
    filter *f;

    f = bugle_filter_new(handle, "checks");
    /* Pointer checks */
    bugle_filter_catches(f, "glDrawArrays", false, checks_glDrawArrays);
    bugle_filter_catches(f, "glDrawElements", false, checks_glDrawElements);
#ifdef GL_EXT_draw_range_elements
    bugle_filter_catches(f, "glDrawRangeElementsEXT", false, checks_glDrawRangeElements);
#endif
#ifdef GL_EXT_multi_draw_arrays
    bugle_filter_catches(f, "glMultiDrawArraysEXT", false, checks_glMultiDrawArrays);
    bugle_filter_catches(f, "glMultiDrawElementsEXT", false, checks_glMultiDrawElements);
#endif
    /* Checks that we are outside begin/end */
    bugle_filter_catches(f, "glEnableClientState", false, checks_no_begin_end);
    bugle_filter_catches(f, "glDisableClientState", false, checks_no_begin_end);
    bugle_filter_catches(f, "glPushClientAttrib", false, checks_no_begin_end);
    bugle_filter_catches(f, "glPopClientAttrib", false, checks_no_begin_end);
    bugle_filter_catches(f, "glColorPointer", false, checks_no_begin_end);
#ifdef GL_EXT_fog_coord
    bugle_filter_catches(f, "glFogCoordPointerEXT", false, checks_no_begin_end);
#endif
    bugle_filter_catches(f, "glEdgeFlagPointer", false, checks_no_begin_end);
    bugle_filter_catches(f, "glIndexPointer", false, checks_no_begin_end);
    bugle_filter_catches(f, "glNormalPointer", false, checks_no_begin_end);
    bugle_filter_catches(f, "glTexCoordPointer", false, checks_no_begin_end);
#ifdef GL_EXT_secondary_color
    bugle_filter_catches(f, "glSecondaryColorPointerEXT", false, checks_no_begin_end);
#endif
    bugle_filter_catches(f, "glVertexPointer", false, checks_no_begin_end);
#ifdef GL_ARB_vertex_program
    bugle_filter_catches(f, "glVertexAttribPointerARB", false, checks_no_begin_end);
#endif
#ifdef GL_ARB_multitexture
    bugle_filter_catches(f, "glClientActiveTextureARB", false, checks_no_begin_end);
#endif
    bugle_filter_catches(f, "glInterleavedArrays", false, checks_no_begin_end);
    bugle_filter_catches(f, "glPixelStorei", false, checks_no_begin_end);
    bugle_filter_catches(f, "glPixelStoref", false, checks_no_begin_end);
    /* Checks that we are inside begin/end */
    bugle_filter_catches_drawing_immediate(f, false, checks_begin_end);
    /* This call has undefined behaviour if given a negative argument */
    bugle_filter_catches(f, "glArrayElement", false, checks_glArrayElement);
    /* Other */
#ifdef GL_ARB_multitexture
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
    bugle_filter_order("checks", "trackbeginend");
    bugle_filter_order("checks", "trackdisplaylist");
    return true;
}

void bugle_initialise_filter_library(void)
{
    static const filter_set_info error_info =
    {
        "error",
        error_initialise,
        NULL,
        NULL,
        NULL,
        NULL,
        "checks for OpenGL errors after each call (see also `showerror')"
    };
    static const filter_set_info showerror_info =
    {
        "showerror",
        showerror_initialise,
        NULL,
        NULL,
        NULL,
        NULL,
        "logs OpenGL errors"
    };
    static const filter_set_info unwindstack_info =
    {
        "unwindstack",
        unwindstack_initialise,
        NULL,
        NULL,
        NULL,
        NULL,
        "catches segfaults and allows recovery of the stack (see docs)"
    };
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
    bugle_filter_set_new(&error_info);
    bugle_filter_set_new(&showerror_info);
    bugle_filter_set_new(&unwindstack_info);
    bugle_filter_set_new(&checks_info);

    bugle_filter_set_renders("error");
    bugle_filter_set_depends("showerror", "error");
    bugle_filter_set_queries_error("showerror");
    bugle_filter_set_renders("checks");
    bugle_filter_set_depends("checks", "trackextensions");
}
