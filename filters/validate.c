/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004, 2005  Bruce Merry
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
#define _POSIX_SOURCE
#include "src/filters.h"
#include "src/utils.h"
#include "src/glutils.h"
#include "src/gldump.h"
#include "src/tracker.h"
#include "src/glexts.h"
#include "common/bool.h"
#include "common/safemem.h"
#include "common/threads.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <setjmp.h>
#include <errno.h>
#include <assert.h>

static bool trap = false;
static filter_set *error_handle = NULL;
static bugle_object_view error_context_view;

static bool error_callback(function_call *call, const callback_data *data)
{
    GLenum error;
    GLenum *stored_error;

    *(GLenum *) data->call_data = GL_NO_ERROR;
    if (budgie_function_table[call->generic.id].name[2] == 'X') return true; /* GLX */
    if (call->generic.group == GROUP_glGetError)
    {
        /* We hope that it returns GL_NO_ERROR, since otherwise something
         * slipped through our own net. If not, we return it to the app
         * rather than whatever we have saved. Also, we must make sure to
         * return nothing else inside begin/end.
         */
        stored_error = bugle_object_get_current_data(&bugle_context_class, error_context_view);
        if (*call->typed.glGetError.retn != GL_NO_ERROR)
        {
            flockfile(stderr);
            fputs("Warning: glGetError() returned ", stderr);
            bugle_dump_GLerror(*call->typed.glGetError.retn, stderr);
            fputs("\n", stderr);
            funlockfile(stderr);
        }
        else if (!bugle_in_begin_end() && *stored_error)
        {
            *call->typed.glGetError.retn = *stored_error;
            *stored_error = GL_NO_ERROR;
        }
    }
    else if (!bugle_in_begin_end())
    {
        /* Note: we deliberately don't call begin_internal_render here,
         * since it will beat us to calling glGetError().
         */
        stored_error = bugle_object_get_current_data(&bugle_context_class, error_context_view);
        while ((error = CALL_glGetError()) != GL_NO_ERROR)
        {
            if (stored_error && !*stored_error)
                *stored_error = error;
            *(GLenum *) data->call_data = error;
            if (trap)
            {
                fflush(stderr);
                /* SIGTRAP is technically a BSD extension, and various
                 * versions of FreeBSD do weird things (e.g. 4.8 will
                 * never define it if _POSIX_SOURCE is defined). Rather
                 * than try all possibilities we just SIGABRT instead.
                 */
#ifdef SIGTRAP
                raise(SIGTRAP);
#else
                abort();
#endif
            }
        }
    }
    return true;
}

static bool initialise_error(filter_set *handle)
{
    filter *f;

    error_handle = handle;
    f = bugle_register_filter(handle, "error");
    bugle_register_filter_catches_all(f, false, error_callback);
    bugle_register_filter_depends("error", "invoke");
    /* We don't call filter_post_renders, because that would make the
     * error filterset depend on itself.
     */
    error_context_view = bugle_object_class_register(&bugle_context_class,
                                                     NULL,
                                                     NULL,
                                                     sizeof(GLenum));
    return true;
}

static bool showerror_callback(function_call *call, const callback_data *data)
{
    GLenum error;
    if ((error = *(GLenum *) bugle_get_filter_set_call_state(call, error_handle)) != GL_NO_ERROR)
    {
        flockfile(stderr);
        budgie_dump_any_type(TYPE_7GLerror, &error, -1, stderr);
        fprintf(stderr, " in %s\n", budgie_function_table[call->generic.id].name);
        funlockfile(stderr);
    }
    return true;
}

static bool initialise_showerror(filter_set *handle)
{
    filter *f;

    f = bugle_register_filter(handle, "showerror");
    bugle_register_filter_catches_all(f, false, showerror_callback);
    bugle_register_filter_depends("showerror", "error");
    bugle_register_filter_depends("showerror", "invoke");
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
        raise(SIGSEGV);
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

static bool initialise_unwindstack(filter_set *handle)
{
    filter *f;

    f = bugle_register_filter(handle, "unwindstack_pre");
    bugle_register_filter_catches_all(f, false, unwindstack_pre_callback);
    f = bugle_register_filter(handle, "unwindstack_post");
    bugle_register_filter_catches_all(f, false, unwindstack_post_callback);
    bugle_register_filter_depends("unwindstack_post", "invoke");
    bugle_register_filter_depends("invoke", "unwindstack_pre");
    return true;
}

/* This is set to some description of the thing being tested, and if it
 * causes a SIGSEGV it is used to describe the error.
 */
static const char *checks_error;
static sigjmp_buf checks_buf;
static bugle_thread_mutex_t checks_mutex = BUGLE_THREAD_MUTEX_INITIALIZER;

static void checks_sigsegv_handler(int sig)
{
    siglongjmp(checks_buf, 1);
}

/* Just reads every byte of the given address range. We use the volatile
 * keyword to (hopefully) force the read.
 */
static void checks_memory(size_t size, const void *data)
{
    volatile char dummy;
    const char *cdata;
    size_t i;

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

    assert(buffer && !bugle_in_begin_end() && bugle_gl_has_extension(BUGLE_GL_ARB_vertex_buffer_object));

    CALL_glGetIntegerv(GL_ARRAY_BUFFER_BINDING_ARB, &tmp);
    CALL_glBindBufferARB(GL_ARRAY_BUFFER_ARB, buffer);
    CALL_glGetBufferParameterivARB(GL_ARRAY_BUFFER_ARB, GL_BUFFER_SIZE_ARB, &bsize);
    CALL_glBindBufferARB(GL_ARRAY_BUFFER_ARB, tmp);
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
        CALL_glGetIntegerv(binding, &id);
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
                             GLenum type_name, budgie_type type,
                             GLenum stride_name,
                             GLenum ptr_name, GLenum binding)
{
    GLint stride, gltype;
    size_t group_size;
    GLvoid *ptr;
    const char *cptr;

    if (CALL_glIsEnabled(name))
    {
        checks_error = text;
        if (size_name) CALL_glGetIntegerv(size_name, &size);
        if (type_name)
        {
            CALL_glGetIntegerv(type_name, &gltype);
            type = bugle_gl_type_to_type(gltype);
        }
        CALL_glGetIntegerv(stride_name, &stride);
        CALL_glGetPointerv(ptr_name, &ptr);
        group_size = budgie_type_table[type].size * size;
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
    GLint stride, gltype, enabled, size;
    size_t group_size;
    GLvoid *ptr;
    budgie_type type;
    const char *cptr;
#ifdef GL_ARB_vertex_buffer_object
    GLint id;
#endif

    CALL_glGetVertexAttribivARB(number, GL_VERTEX_ATTRIB_ARRAY_ENABLED_ARB, &enabled);
    if (enabled)
    {
        checks_error = "vertex attribute array";
        CALL_glGetVertexAttribivARB(number, GL_VERTEX_ATTRIB_ARRAY_SIZE_ARB, &size);
        CALL_glGetVertexAttribivARB(number, GL_VERTEX_ATTRIB_ARRAY_TYPE_ARB, &gltype);
        type = bugle_gl_type_to_type(gltype);
        CALL_glGetVertexAttribivARB(number, GL_VERTEX_ATTRIB_ARRAY_STRIDE_ARB, &stride);
        CALL_glGetVertexAttribPointervARB(number, GL_VERTEX_ATTRIB_ARRAY_POINTER_ARB, &ptr);
        group_size = budgie_type_table[type].size * size;
        if (!stride) stride = group_size;
        cptr = (const char *) ptr;
        cptr += group_size * first;

        size = (count - 1) * stride + group_size;
#ifdef GL_ARB_vertex_buffer_object
        id = 0;
        if (!bugle_in_begin_end() && bugle_gl_has_extension(BUGLE_GL_ARB_vertex_buffer_object))
            CALL_glGetVertexAttribivARB(number, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING_ARB, &id);
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
                     0, TYPE_9GLboolean,
                     GL_EDGE_FLAG_ARRAY_STRIDE,
                     GL_EDGE_FLAG_ARRAY_POINTER,
                     VBO_ENUM(GL_EDGE_FLAG_ARRAY_BUFFER_BINDING_ARB));
    /* FIXME: there are others (fog, secondary colour, ?) */

#ifdef GL_ARB_multitexture
    /* FIXME: if there is a failure, the current texture unit will be wrong */
    if (bugle_gl_has_extension(BUGLE_GL_ARB_multitexture))
    {
        GLint texunits, old;

        CALL_glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &texunits);
        CALL_glGetIntegerv(GL_CLIENT_ACTIVE_TEXTURE_ARB, &old);
        for (i = GL_TEXTURE0_ARB; i < GL_TEXTURE0_ARB + (GLenum) texunits; i++)
        {
            CALL_glClientActiveTextureARB(i);
            checks_attribute(first, count,
                             "texture coordinate array", GL_TEXTURE_COORD_ARRAY,
                             GL_TEXTURE_COORD_ARRAY_SIZE, 0,
                             GL_TEXTURE_COORD_ARRAY_TYPE, 0,
                             GL_TEXTURE_COORD_ARRAY_STRIDE,
                             GL_TEXTURE_COORD_ARRAY_POINTER,
                             VBO_ENUM(GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING_ARB));
        }
        CALL_glClientActiveTextureARB(old);
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
        GLint count, i;

        CALL_glGetIntegerv(GL_MAX_VERTEX_ATTRIBS_ARB, &count);
        for (i = 0; i < count; i++)
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
        CALL_glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING_ARB, &id);
        if (id)
        {
            /* We are not allowed to call glGetBufferSubDataARB on a
             * mapped buffer. Fortunately, if the buffer is mapped, the
             * call is illegal and should generate INVALID_OPERATION anyway.
             */
            CALL_glGetBufferParameterivARB(GL_ELEMENT_ARRAY_BUFFER_ARB,
                                           GL_BUFFER_MAPPED_ARB,
                                           &mapped);
            if (mapped) return;

            size = count * budgie_type_table[type].size;
            vbo_indices = bugle_malloc(size);
            CALL_glGetBufferSubDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB,
                                       (const char *) indices - (const char *) NULL,
                                       size, vbo_indices);
            indices = vbo_indices;
        }
    }
#endif

    out = (GLuint *) bugle_malloc(count * sizeof(GLuint));
    budgie_type_convert(out, TYPE_6GLuint, indices, type, count);
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
    bugle_thread_mutex_lock(&checks_mutex); \
    checks_error = NULL; \
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
    bugle_thread_mutex_unlock(&checks_mutex); \
    return ret; \
    } else (void) 0

static bool checks_glDrawArrays(function_call *call, const callback_data *data)
{
    if (*call->typed.glDrawArrays.arg1 < 0)
    {
        fprintf(stderr, "WARNING: glDrawArrays called with a negative argument; call will be ignored.\n");
        return false;
    }

    if (CHECKS_START())
    {
        fprintf(stderr, "WARNING: illegal %s caught in glDrawArrays; call will be ignored.\n",
                checks_error ? checks_error : "pointer");
    }
    else
    {
        checks_attributes(*call->typed.glDrawArrays.arg1,
                          *call->typed.glDrawArrays.arg2);
    }
    CHECKS_STOP();
}

static bool checks_glDrawElements(function_call *call, const callback_data *data)
{
    if (CHECKS_START())
    {
        fprintf(stderr, "WARNING: illegal %s caught in glDrawElements; call will be ignored.\n",
                checks_error ? checks_error : "pointer");
    }
    else
    {
        GLsizei count;
        GLenum type;
        const GLvoid *indices;
        GLuint min, max;

        checks_error = "index array";
        count = *call->typed.glDrawElements.arg1;
        type = *call->typed.glDrawElements.arg2;
        indices = *call->typed.glDrawElements.arg3;
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
        fprintf(stderr, "WARNING: illegal %s caught in glDrawRangeElements; call will be ignored.\n",
                checks_error ? checks_error : "pointer");
    }
    else
    {
        GLsizei count;
        GLenum type;
        const GLvoid *indices;
        GLuint min, max;

        checks_error = "index array";
        count = *call->typed.glDrawRangeElementsEXT.arg3;
        type = *call->typed.glDrawRangeElementsEXT.arg4;
        indices = *call->typed.glDrawRangeElementsEXT.arg5;
        checks_buffer(count * bugle_gl_type_to_size(type),
                      indices,
                      VBO_ENUM(GL_ELEMENT_ARRAY_BUFFER_BINDING_ARB));
        checks_min_max(count, type, indices, &min, &max);
        if (min < *call->typed.glDrawRangeElementsEXT.arg1
            || max > *call->typed.glDrawRangeElementsEXT.arg2)
        {
            fprintf(stderr, "WARNING: glDrawRangeElements indices fall outside range; call will be ignored.\n");
            ret = false;
        }
        else
        {
            min = *call->typed.glDrawRangeElementsEXT.arg1;
            max = *call->typed.glDrawRangeElementsEXT.arg2;
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
        fprintf(stderr, "WARNING: illegal %s caught in glMultiDrawArrays; call will be ignored.\n",
                checks_error ? checks_error : "pointer");
    }
    else
    {
        const GLint *first_ptr;
        const GLsizei *count_ptr;
        GLsizei count, i;

        count = *call->typed.glMultiDrawArraysEXT.arg3;
        first_ptr = *call->typed.glMultiDrawArraysEXT.arg1;
        count_ptr = *call->typed.glMultiDrawArraysEXT.arg2;

        checks_error = "first array";
        checks_memory(sizeof(GLint) * count, first_ptr);
        checks_error = "count array";
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
        fprintf(stderr, "WARNING: illegal %s caught in glMultiDrawElements; call will be ignored.\n",
                checks_error ? checks_error : "pointer");
    }
    else
    {
        const GLsizei *count_ptr;
        const GLvoid * const * indices_ptr;
        GLsizei count, i;
        GLenum type;
        GLuint min, max;

        count = *call->typed.glMultiDrawElements.arg4;
        type = *call->typed.glMultiDrawElements.arg2;
        count_ptr = *call->typed.glMultiDrawElements.arg1;
        indices_ptr = *call->typed.glMultiDrawElements.arg3;

        checks_error = "count array";
        checks_memory(sizeof(GLsizei) * count, count_ptr);
        checks_error = "indices array";
        checks_memory(sizeof(GLvoid *) * count, indices_ptr);
        checks_error = "index array";

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
        fprintf(stderr, "WARNING: %s called inside glBegin/glEnd; call will be ignored.\n",
                budgie_function_table[call->generic.id].name);
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
        name = budgie_function_table[call->generic.id].name;
#ifdef GL_ARB_vertex_program
        if (strncmp(name, "glVertexAttrib", 14) == 0)
        {
            GLuint attrib;

            attrib = *(const GLuint *) call->generic.args[0];
            if (attrib) return true;
        }
#endif
        fprintf(stderr, "WARNING: %s called outside glBegin/glEnd; call will be ignored.\n",
                name);
        return false;
    }
    else
        return true;
}

static bool checks_glArrayElement(function_call *call, const callback_data *data)
{
    if (*call->typed.glArrayElement.arg0 < 0)
    {
        fprintf(stderr, "WARNING: glArrayElement called with a negative argument; call will be ignored.\n");
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
            CALL_glGetIntegerv(GL_MAX_TEXTURE_COORDS_ARB, &max);
            /* NVIDIA ship a driver that just generates an error on this
             * call on NV20. Instead of borking we check for the error and
             * fall back to GL_MAX_TEXTURE_UNITS.
             */
            CALL_glGetError();
        }
#endif
        if (!max)
            CALL_glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &max);
        bugle_end_internal_render("checks_glMultiTexCoord", true);
    }
    if (texture < GL_TEXTURE0_ARB || texture >= GL_TEXTURE0_ARB + (GLenum) max)
    {
        fprintf(stderr, "WARNING: %s called with out of range texture unit; call will be ignored.\n",
                budgie_function_table[call->generic.id].name);
        return false;
    }
    return true;
}
#endif

static bool initialise_checks(filter_set *handle)
{
    filter *f;

    f = bugle_register_filter(handle, "checks");
    /* Pointer checks */
    bugle_register_filter_catches(f, GROUP_glDrawArrays, false, checks_glDrawArrays);
    bugle_register_filter_catches(f, GROUP_glDrawElements, false, checks_glDrawElements);
#ifdef GL_EXT_draw_range_elements
    bugle_register_filter_catches(f, GROUP_glDrawRangeElementsEXT, false, checks_glDrawRangeElements);
#endif
#ifdef GL_EXT_multi_draw_arrays
    bugle_register_filter_catches(f, GROUP_glMultiDrawArraysEXT, false, checks_glMultiDrawArrays);
    bugle_register_filter_catches(f, GROUP_glMultiDrawElementsEXT, false, checks_glMultiDrawElements);
#endif
    /* Checks that we are outside begin/end */
    bugle_register_filter_catches(f, GROUP_glEnableClientState, false, checks_no_begin_end);
    bugle_register_filter_catches(f, GROUP_glDisableClientState, false, checks_no_begin_end);
    bugle_register_filter_catches(f, GROUP_glPushClientAttrib, false, checks_no_begin_end);
    bugle_register_filter_catches(f, GROUP_glPopClientAttrib, false, checks_no_begin_end);
    bugle_register_filter_catches(f, GROUP_glColorPointer, false, checks_no_begin_end);
#ifdef GL_EXT_fog_coord
    bugle_register_filter_catches(f, GROUP_glFogCoordPointerEXT, false, checks_no_begin_end);
#endif
    bugle_register_filter_catches(f, GROUP_glEdgeFlagPointer, false, checks_no_begin_end);
    bugle_register_filter_catches(f, GROUP_glIndexPointer, false, checks_no_begin_end);
    bugle_register_filter_catches(f, GROUP_glNormalPointer, false, checks_no_begin_end);
    bugle_register_filter_catches(f, GROUP_glTexCoordPointer, false, checks_no_begin_end);
#ifdef GL_EXT_secondary_color
    bugle_register_filter_catches(f, GROUP_glSecondaryColorPointerEXT, false, checks_no_begin_end);
#endif
    bugle_register_filter_catches(f, GROUP_glVertexPointer, false, checks_no_begin_end);
#ifdef GL_ARB_vertex_program
    bugle_register_filter_catches(f, GROUP_glVertexAttribPointerARB, false, checks_no_begin_end);
#endif
#ifdef GL_ARB_multitexture
    bugle_register_filter_catches(f, GROUP_glClientActiveTextureARB, false, checks_no_begin_end);
#endif
    bugle_register_filter_catches(f, GROUP_glInterleavedArrays, false, checks_no_begin_end);
    bugle_register_filter_catches(f, GROUP_glPixelStorei, false, checks_no_begin_end);
    bugle_register_filter_catches(f, GROUP_glPixelStoref, false, checks_no_begin_end);
    /* Checks that we are inside begin/end */
    bugle_register_filter_catches_drawing_immediate(f, false, checks_begin_end);
    /* This call has undefined behaviour if given a negative argument */
    bugle_register_filter_catches(f, GROUP_glArrayElement, false, checks_glArrayElement);
    /* Other */
#ifdef GL_ARB_multitexture
    bugle_register_filter_catches(f, GROUP_glMultiTexCoord1s, false, checks_glMultiTexCoord);
    bugle_register_filter_catches(f, GROUP_glMultiTexCoord1i, false, checks_glMultiTexCoord);
    bugle_register_filter_catches(f, GROUP_glMultiTexCoord1f, false, checks_glMultiTexCoord);
    bugle_register_filter_catches(f, GROUP_glMultiTexCoord1d, false, checks_glMultiTexCoord);
    bugle_register_filter_catches(f, GROUP_glMultiTexCoord2s, false, checks_glMultiTexCoord);
    bugle_register_filter_catches(f, GROUP_glMultiTexCoord2i, false, checks_glMultiTexCoord);
    bugle_register_filter_catches(f, GROUP_glMultiTexCoord2f, false, checks_glMultiTexCoord);
    bugle_register_filter_catches(f, GROUP_glMultiTexCoord2d, false, checks_glMultiTexCoord);
    bugle_register_filter_catches(f, GROUP_glMultiTexCoord3s, false, checks_glMultiTexCoord);
    bugle_register_filter_catches(f, GROUP_glMultiTexCoord3i, false, checks_glMultiTexCoord);
    bugle_register_filter_catches(f, GROUP_glMultiTexCoord3f, false, checks_glMultiTexCoord);
    bugle_register_filter_catches(f, GROUP_glMultiTexCoord3d, false, checks_glMultiTexCoord);
    bugle_register_filter_catches(f, GROUP_glMultiTexCoord4s, false, checks_glMultiTexCoord);
    bugle_register_filter_catches(f, GROUP_glMultiTexCoord4i, false, checks_glMultiTexCoord);
    bugle_register_filter_catches(f, GROUP_glMultiTexCoord4f, false, checks_glMultiTexCoord);
    bugle_register_filter_catches(f, GROUP_glMultiTexCoord4d, false, checks_glMultiTexCoord);
    bugle_register_filter_catches(f, GROUP_glMultiTexCoord1sv, false, checks_glMultiTexCoord);
    bugle_register_filter_catches(f, GROUP_glMultiTexCoord1iv, false, checks_glMultiTexCoord);
    bugle_register_filter_catches(f, GROUP_glMultiTexCoord1fv, false, checks_glMultiTexCoord);
    bugle_register_filter_catches(f, GROUP_glMultiTexCoord1dv, false, checks_glMultiTexCoord);
    bugle_register_filter_catches(f, GROUP_glMultiTexCoord2sv, false, checks_glMultiTexCoord);
    bugle_register_filter_catches(f, GROUP_glMultiTexCoord2iv, false, checks_glMultiTexCoord);
    bugle_register_filter_catches(f, GROUP_glMultiTexCoord2fv, false, checks_glMultiTexCoord);
    bugle_register_filter_catches(f, GROUP_glMultiTexCoord2dv, false, checks_glMultiTexCoord);
    bugle_register_filter_catches(f, GROUP_glMultiTexCoord3sv, false, checks_glMultiTexCoord);
    bugle_register_filter_catches(f, GROUP_glMultiTexCoord3iv, false, checks_glMultiTexCoord);
    bugle_register_filter_catches(f, GROUP_glMultiTexCoord3fv, false, checks_glMultiTexCoord);
    bugle_register_filter_catches(f, GROUP_glMultiTexCoord3dv, false, checks_glMultiTexCoord);
    bugle_register_filter_catches(f, GROUP_glMultiTexCoord4sv, false, checks_glMultiTexCoord);
    bugle_register_filter_catches(f, GROUP_glMultiTexCoord4iv, false, checks_glMultiTexCoord);
    bugle_register_filter_catches(f, GROUP_glMultiTexCoord4fv, false, checks_glMultiTexCoord);
    bugle_register_filter_catches(f, GROUP_glMultiTexCoord4dv, false, checks_glMultiTexCoord);
#endif

    /* FIXME: still perhaps to do:
     * - check for passing a glMapBuffer region to a command
     */

    /* We try to push this early, since it would defeat the whole thing if
     * bugle crashed while examining the data in another filter.
     */
    bugle_register_filter_depends("invoke", "checks");
    bugle_register_filter_depends("stats", "checks");
    bugle_register_filter_depends("log_pre", "checks");
    bugle_register_filter_depends("trackcontext", "checks");
    bugle_register_filter_depends("trackbeginend", "checks");
    bugle_register_filter_depends("trackdisplaylist", "checks");
    return true;
}

void bugle_initialise_filter_library(void)
{
    static const filter_set_info error_info =
    {
        "error",
        initialise_error,
        NULL,
        NULL,
        NULL,
        NULL,
        sizeof(GLenum),
        "checks for OpenGL errors after each call (see also `showerror')"
    };
    static const filter_set_info showerror_info =
    {
        "showerror",
        initialise_showerror,
        NULL,
        NULL,
        NULL,
        NULL,
        0,
        "prints any OpenGL errors to standard error"
    };
    static const filter_set_info unwindstack_info =
    {
        "unwindstack",
        initialise_unwindstack,
        NULL,
        NULL,
        NULL,
        NULL,
        0,
        "catches segfaults and allows recovery of the stack (see docs)"
    };
    static const filter_set_info checks_info =
    {
        "checks",
        initialise_checks,
        NULL,
        NULL,
        NULL,
        NULL,
        0,
        "checks for illegal values passed to OpenGL in some places"
    };
    bugle_register_filter_set(&error_info);
    bugle_register_filter_set(&showerror_info);
    bugle_register_filter_set(&unwindstack_info);
    bugle_register_filter_set(&checks_info);

    bugle_register_filter_set_renders("error");
    bugle_register_filter_set_depends("showerror", "error");
    bugle_register_filter_set_renders("checks");
    bugle_register_filter_set_depends("checks", "trackextensions");
}
