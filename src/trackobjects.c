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
#include <GL/glext.h>
#include <stdlib.h>
#include <stddef.h>
#include "src/filters.h"
#include "src/tracker.h"
#include "src/utils.h"
#include "src/glutils.h"
#include "src/glexts.h"
#include "common/bool.h"
#include "common/threads.h"
#include "common/safemem.h"
#include "common/radixtree.h"

/* Static asserts that gl_handle has sufficient bits.
 * The array size will be negative if the assert fails and positive
 * otherwise. We deliberately avoid zero-length arrays, because GCC
 * accepts them but they are illegal in C90.
 */
#if defined(__GNUC__) && __GNUC__ >= 3
# define ATTRIB_UNUSED __attribute__ ((unused))
#else
# define ATTRIB_UNUSED
#endif

#ifdef GL_ARB_shader_objects
static int static_assert1[2 * (sizeof(gl_handle) - sizeof(GLhandleARB)) + 1] ATTRIB_UNUSED;
#endif
static int static_assert2[2 * (sizeof(gl_handle) - sizeof(GLuint)) + 1] ATTRIB_UNUSED;

typedef struct
{
    bugle_thread_mutex_t mutex;
    bugle_radix_tree objects[BUGLE_TRACKOBJECTS_COUNT];
} trackobjects_data;

/* FIXME: check which types of objects are shared between contexts (all but queries I think) */

static bugle_object_view view;

static bugle_radix_tree *get_table(bugle_trackobjects_type type)
{
    trackobjects_data *data;

    data = bugle_object_get_current_data(&bugle_namespace_class, view);
    if (!data) return NULL;
    return &data->objects[type];
}

static inline void lock(void)
{
    bugle_thread_mutex_lock(&((trackobjects_data *) bugle_object_get_current_data(&bugle_namespace_class, view))->mutex);
}

static inline void unlock(void)
{
    bugle_thread_mutex_unlock(&((trackobjects_data *) bugle_object_get_current_data(&bugle_namespace_class, view))->mutex);
}

/* We don't rely entirely on glBindTexture and glDeleteTextures etc to
 * know the list of alive objects. Rather, we examine any ids passed to
 * these functions and check whether they really do exist. This avoids
 * race conditions and other nastiness (like failed calls).
 *
 * Note: it may appear that we can skip the begin/end_internal_render if
 * "is" is NULL_FUNCTION (e.g. for glCreateShader), but it is in fact
 * still useful since it prevents us adding a bogus id if glCreateShader
 * was called from within begin/end.
 */
static void trackobjects_add_single(bugle_trackobjects_type type,
                                    GLenum target,
                                    gl_handle object,
                                    budgie_function is)
{
    bugle_radix_tree *table;

    lock();
    table = get_table(type);
    if (table && bugle_begin_internal_render())
    {
        if (is == -1 || (*(GLboolean (*)(GLuint)) budgie_function_table[is].real)(object))
            bugle_radix_tree_set(table, object, (void *) (size_t) target);
        bugle_end_internal_render("trackobjects_add_single", true);
    }
    unlock();
}

static void trackobjects_delete_single(bugle_trackobjects_type type,
                                       gl_handle object)
{
    bugle_radix_tree *table;

    lock();
    table = get_table(type);
    if (table && bugle_begin_internal_render())
    {
        bugle_radix_tree_set(table, object, NULL);
        bugle_end_internal_render("trackobjects_delete_single", true);
    }
    unlock();
}

/* NB: don't change GLuint to gl_handle here, because it is a pointer.
 * We would need a separate call.
 */
static void trackobjects_delete_multiple(bugle_trackobjects_type type,
                                         GLsizei count,
                                         const GLuint *objects,
                                         budgie_function is)
{
    GLsizei i;
    bugle_radix_tree *table;

    lock();
    table = get_table(type);
    if (table && bugle_begin_internal_render())
    {
        for (i = 0; i < count; i++)
            if (is == -1 || !(*(GLboolean (*)(GLuint)) budgie_function_table[is].real)(objects[i]))
                bugle_radix_tree_set(table, objects[i], NULL);
        bugle_end_internal_render("trackobjects_delete_multiple", true);
    }
    unlock();
}

static bool trackobjects_glBindTexture(function_call *call, const callback_data *data)
{
    trackobjects_add_single(BUGLE_TRACKOBJECTS_TEXTURE,
                            *call->typed.glBindTexture.arg0,
                            *call->typed.glBindTexture.arg1,
                            FUNC_glIsTexture);
    return true;
}

static bool trackobjects_glDeleteTextures(function_call *call, const callback_data *data)
{
    trackobjects_delete_multiple(BUGLE_TRACKOBJECTS_TEXTURE,
                                 *call->typed.glDeleteTextures.arg0,
                                 *call->typed.glDeleteTextures.arg1,
                                 FUNC_glIsTexture);
    return true;
}

#ifdef GL_ARB_vertex_buffer_object
static bool trackobjects_glBindBuffer(function_call *call, const callback_data *data)
{
    trackobjects_add_single(BUGLE_TRACKOBJECTS_BUFFER,
                            *call->typed.glBindBufferARB.arg0,
                            *call->typed.glBindBufferARB.arg1,
                            FUNC_glIsBufferARB);
    return true;
}

static bool trackobjects_glDeleteBuffers(function_call *call, const callback_data *data)
{
    trackobjects_delete_multiple(BUGLE_TRACKOBJECTS_BUFFER,
                                 *call->typed.glDeleteBuffersARB.arg0,
                                 *call->typed.glDeleteBuffersARB.arg1,
                                 FUNC_glIsBufferARB);
    return true;
}
#endif

#ifdef GL_ARB_occlusion_query
static bool trackobjects_glBeginQuery(function_call *call, const callback_data *data)
{
    trackobjects_add_single(BUGLE_TRACKOBJECTS_QUERY,
                            *call->typed.glBeginQueryARB.arg0,
                            *call->typed.glBeginQueryARB.arg1,
                            FUNC_glIsQueryARB);
    return true;
}

static bool trackobjects_glDeleteQueries(function_call *call, const callback_data *data)
{
    trackobjects_delete_multiple(BUGLE_TRACKOBJECTS_QUERY,
                                 *call->typed.glDeleteQueriesARB.arg0,
                                 *call->typed.glDeleteQueriesARB.arg1,
                                 FUNC_glIsQueryARB);
    return true;
}
#endif

/* Shader and program objects are tricky, because unlike texture and buffer
 * objects they don't necessarily disappear immediately. Shaders disappear
 * when no longer attached to ANY program, and programs disappear when no
 * longer active in ANY context. The only reliable way to check is to try
 * to get a parameter and see if it generates an error.
 *
 * FIXME: we need to do this check.
 */

typedef struct
{
    bugle_trackobjects_type type;
    gl_handle object;
} check_data;

static void init_checks(const callback_data *data)
{
    bugle_list_init((bugle_linked_list *) data->call_data, true);
}

static void add_check(const callback_data *data,
                      bugle_trackobjects_type type,
                      gl_handle object)
{
    check_data *c;

    c = bugle_malloc(sizeof(check_data));
    c->type = type;
    c->object = object;
    bugle_list_append((bugle_linked_list *) data->call_data, c);
}

#ifdef GL_ARB_shader_objects
static bool trackobjects_glCreateShaderObjectARB(function_call *call, const callback_data *data)
{
    trackobjects_add_single(BUGLE_TRACKOBJECTS_SHADER_OBJECT,
                            *call->typed.glCreateShaderObjectARB.arg0,
                            *call->typed.glCreateShaderObjectARB.retn,
                            NULL_FUNCTION);
    return true;
}

static bool trackobjects_glCreateProgramObjectARB(function_call *call, const callback_data *data)
{
    trackobjects_add_single(BUGLE_TRACKOBJECTS_PROGRAM_OBJECT,
                            GL_PROGRAM_OBJECT_ARB,
                            *call->typed.glCreateProgramObjectARB.retn,
                            NULL_FUNCTION);
    return true;
}

static bool trackobjects_pre_glDeleteObjectARB(function_call *call, const callback_data *data)
{
    GLhandleARB object;
    GLint type, count, i;
    GLhandleARB *attached;

    init_checks(data);
    if (bugle_begin_internal_render())
    {
        object = *call->typed.glDeleteObjectARB.arg0;
        CALL_glGetObjectParameterivARB(object, GL_OBJECT_TYPE_ARB, &type);
        switch (type)
        {
        case GL_PROGRAM_OBJECT_ARB:
            CALL_glGetObjectParameterivARB(object, GL_OBJECT_ATTACHED_OBJECTS_ARB, &count);
            if (count)
            {
                attached = bugle_malloc(sizeof(GLhandleARB) * count);
                CALL_glGetAttachedObjectsARB(object, count, NULL, attached);
                for (i = 0; i < count; i++)
                    add_check(data, BUGLE_TRACKOBJECTS_SHADER_OBJECT, attached[i]);
                free(attached);
            }
            add_check(data, BUGLE_TRACKOBJECTS_PROGRAM_OBJECT, object);
            break;
        case GL_SHADER_OBJECT_ARB:
            add_check(data, BUGLE_TRACKOBJECTS_SHADER_OBJECT, object);
            break;
        }
        bugle_end_internal_render("trackobjects_pre_glDeleteObjectARB", true);
    }
    return true;
}

static bool trackobjects_pre_glUseProgramObjectARB(function_call *call, const callback_data *data)
{
    init_checks(data);
    if (bugle_begin_internal_render())
    {
        add_check(data, BUGLE_TRACKOBJECTS_PROGRAM_OBJECT, CALL_glGetHandleARB(GL_PROGRAM_OBJECT_ARB));
        bugle_end_internal_render("trackobjects_pre_glUseProgramARB", true);
    }
    return true;
}

static bool trackobjects_pre_glDetachObjectARB(function_call *call, const callback_data *data)
{
    init_checks(data);
    add_check(data, BUGLE_TRACKOBJECTS_SHADER_OBJECT,
              *call->typed.glDetachObjectARB.arg1);
    return true;
}

#endif

#ifdef GL_VERSION_2_0
static bool trackobjects_glCreateShader(function_call *call, const callback_data *data)
{
    trackobjects_add_single(BUGLE_TRACKOBJECTS_SHADER,
                            *call->typed.glCreateShaderObject.arg0,
                            *call->typed.glCreateShaderObject.retn,
                            FUNC_glIsShader);
    return true;
}

static bool trackobjects_glCreateProgram(function_call *call, const callback_data *data)
{
    trackobjects_add_single(BUGLE_TRACKOBJECTS_PROGRAM,
                            GL_NONE,
                            *call->typed.glCreateProgramObject.retn,
                            FUNC_glIsProgram);
}

static bool trackobjects_pre_glDeleteShader(function_call *call, const callback_data *data)
{
    init_checks(data);
    add_check(data, BUGLE_TRACKOBJECTS_SHADER, *call->typed.glDeleteShader.arg0);
    return true;
}

static bool trackobjects_pre_glDeleteProgram(function_call *call, const callback_data *data)
{
    GLint count, i;
    GLuint *attached;

    init_checks(data);
    if (bugle_begin_internal_render())
    {
        CALL_glGetProgramiv(object, GL_ATTACHED_SHADERS, &count);
        if (count)
        {
            attached = bugle_malloc(sizeof(GLuint) * count);
            CALL_glGetAttachedShaders(object, count, NULL, attached);
            for (i = 0; i < count; i++)
                add_check(data, BUGLE_TRACKOBJECTS_SHADER, attached[i]);
            free(attached);
        }
        bugle_end_internal_render("trackobjects_pre_DeleteProgram", true);
    }
    add_check(data, BUGLE_TRACKOBJECTS_PROGRAM, *call->typed.glDeleteProgram.arg0);
    return true;
}

static bool trackobjects_pre_glUseProgram(function_call *call, const callback_data *data)
{
    GLint p;

    init_checks(data);
    if (bugle_begin_internal_render())
    {
        CALL_glGetIntegerv(GL_CURRENT_PROGRAM, &p);
        end_internal_render("trackobjects_pre_glUseProgramARB");
        add_check(data, BUGLE_TRACKOBJECTS_PROGRAM, p);
    }
    return true;
}

static bool trackobjects_pre_glDetachShader(function_call *call, const callback_data *data)
{
    init_checks(data);
    add_check(data, BGLE_TRACKOBJECTS_SHADER, *call->typed.glDetachShader.arg1);
    return true;
}

#endif

static bool trackobjects_checks(function_call *call, const callback_data *data)
{
    bugle_linked_list *l;
    bugle_list_node *i;
    const check_data *d;

    l = (bugle_linked_list *) data->call_data;
    for (i = bugle_list_head(l); i; i = bugle_list_next(i))
    {
        d = (const check_data *) bugle_list_data(i);
        switch (d->type)
        {
#ifdef GL_ARB_shader_objects
        case BUGLE_TRACKOBJECTS_SHADER_OBJECT:
        case BUGLE_TRACKOBJECTS_PROGRAM_OBJECT:
            if (bugle_begin_internal_render())
            {
                GLint status;
                CALL_glGetObjectParameterivARB(d->object, GL_OBJECT_DELETE_STATUS_ARB, &status);
                if (CALL_glGetError() != GL_NONE)
                    trackobjects_delete_single(d->type, d->object);
                bugle_end_internal_render("trackobjects_checks", true);
            }
            break;
#endif
#ifdef GL_VERSION_2_0
        case BUGLE_TRACKOBJECTS_SHADER:
            if (!CALL_glIsShader(d->object))
                trackobjects_delete_single(d->type, d->object);
            break;
        case BUGLE_TRACKOBJECTS_PROGRAM:
            if (!CALL_glIsProgram(d->object))
                trackobjects_delete_single(d->type, d->object);
            break;
#endif
        default:
            abort();
        }
    }
    bugle_list_clear(l);
    return true;
}

static void initialise_objects(const void *key, void *data)
{
    trackobjects_data *d;
    size_t i;

    d = (trackobjects_data *) data;
    bugle_thread_mutex_init(&d->mutex, NULL);
    for (i = 0; i < BUGLE_TRACKOBJECTS_COUNT; i++)
        bugle_radix_tree_init(&d->objects[i], false);
}

static void destroy_objects(void *data)
{
    trackobjects_data *d;
    size_t i;

    d = (trackobjects_data *) data;

    for (i = 0; i < BUGLE_TRACKOBJECTS_COUNT; i++)
        bugle_radix_tree_clear(&d->objects[i]);
}

static bool initialise_trackobjects(filter_set *handle)
{
    filter *f;

    f = bugle_register_filter(handle, "trackobjects_pre");
#ifdef GL_ARB_shader_objects
    bugle_register_filter_catches(f, GROUP_glDeleteObjectARB, trackobjects_pre_glDeleteObjectARB);
    bugle_register_filter_catches(f, GROUP_glUseProgramObjectARB, trackobjects_pre_glUseProgramObjectARB);
    bugle_register_filter_catches(f, GROUP_glDetachObjectARB, trackobjects_pre_glDetachObjectARB);
#endif
#ifdef GL_VERSION_2_0
    bugle_register_filter_catches(f, GROUP_glDeleteShader, trackobjects_pre_glDeleteShader);
    bugle_register_filter_catches(f, GROUP_glDeleteProgram, trackobjects_pre_glDeleteProgram);
    bugle_register_filter_catches(f, GROUP_glUseProgram, trackobjects_pre_glUseProgram);
    bugle_register_filter_catches(f, GROUP_glDetachShader, trackobjects_pre_glDetachShader);
#endif

    f = bugle_register_filter(handle, "trackobjects");
    bugle_register_filter_catches(f, GROUP_glBindTexture, trackobjects_glBindTexture);
    bugle_register_filter_catches(f, GROUP_glDeleteTextures, trackobjects_glDeleteTextures);
#ifdef GL_ARB_vertex_buffer_object
    bugle_register_filter_catches(f, GROUP_glBindBufferARB, trackobjects_glBindBuffer);
    bugle_register_filter_catches(f, GROUP_glDeleteBuffersARB, trackobjects_glDeleteBuffers);
#endif
#ifdef GL_ARB_occlusion_query
    bugle_register_filter_catches(f, GROUP_glBeginQueryARB, trackobjects_glBeginQuery);
    bugle_register_filter_catches(f, GROUP_glDeleteQueriesARB, trackobjects_glDeleteQueries);
#endif
#ifdef GL_ARB_shader_objects
    bugle_register_filter_catches(f, GROUP_glCreateShaderObjectARB, trackobjects_glCreateShaderObjectARB);
    bugle_register_filter_catches(f, GROUP_glCreateProgramObjectARB, trackobjects_glCreateProgramObjectARB);
    bugle_register_filter_catches(f, GROUP_glDeleteObjectARB, trackobjects_checks);
    bugle_register_filter_catches(f, GROUP_glUseProgramObjectARB, trackobjects_checks);
    bugle_register_filter_catches(f, GROUP_glDetachObjectARB, trackobjects_checks);
#endif
#ifdef GL_VERSION_2_0
    bugle_register_filter_catches(f, GROUP_glCreateShader, trackobjects_glCreateShader);
    bugle_register_filter_catches(f, GROUP_glCreateProgram, trackobjects_glCreateProgram);
    bugle_register_filter_catches(f, GROUP_glDeleteShader, trackobjects_checks);
    bugle_register_filter_catches(f, GROUP_glDeleteProgram, trackobjects_checks);
    bugle_register_filter_catches(f, GROUP_glUseProgram, trackobjects_check);
    bugle_register_filter_catches(f, GROUP_glDetachShader, trackobjects_checks);
#endif
    bugle_register_filter_depends("trackobjects", "invoke");
    bugle_register_filter_depends("invoke", "trackobjects_pre");
    bugle_register_filter_post_renders("trackobjects");
    view = bugle_object_class_register(&bugle_namespace_class,
                                       initialise_objects,
                                       destroy_objects,
                                       sizeof(trackobjects_data));
    return true;
}

typedef struct
{
    void (*walker)(gl_handle, GLenum, void *);
    void *data;
} trackobjects_walker;

static void trackobjects_walk(bugle_radix_tree_type object,
                              void *target,
                              void *real)
{
    trackobjects_walker *w;

    w = (trackobjects_walker *) real;
    (*w->walker)(object, (GLenum) (size_t) target, w->data);
}

void bugle_trackobjects_walk(bugle_trackobjects_type type,
                             void (*walker)(gl_handle object,
                                            GLenum target,
                                            void *),
                             void *data)
{
    bugle_radix_tree *table;
    trackobjects_walker w;

    lock();
    table = get_table(type);
    w.walker = walker;
    w.data = data;
    bugle_radix_tree_walk(table, trackobjects_walk, &w);
    unlock();
}

void trackobjects_initialise(void)
{
    static const filter_set_info trackobjects_info =
    {
        "trackobjects",
        initialise_trackobjects,
        NULL,
        NULL,
        sizeof(bugle_linked_list),
        NULL /* no documentation */
    };

    bugle_register_filter_set(&trackobjects_info);
    bugle_register_filter_set_depends("trackobjects", "trackcontext");
    bugle_register_filter_set_depends("trackobjects", "trackextensions");
    bugle_register_filter_set_renders("trackobjects");
}
