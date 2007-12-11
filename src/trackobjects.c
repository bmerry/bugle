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
#include <GL/gl.h>
#include <GL/glext.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "src/filters.h"
#include "src/tracker.h"
#include "src/utils.h"
#include "src/glutils.h"
#include "src/glexts.h"
#include <stdbool.h>
#include "common/radixtree.h"
#include "xalloc.h"
#include "lock.h"

typedef struct
{
    gl_lock_define(, mutex);
    bugle_radix_tree objects[BUGLE_TRACKOBJECTS_COUNT];
} trackobjects_data;

/* FIXME: check which types of objects are shared between contexts (all but queries I think) */

static object_view ns_view, call_view;

static bugle_radix_tree *get_table(bugle_trackobjects_type type)
{
    trackobjects_data *data;

    data = bugle_object_get_current_data(&bugle_namespace_class, ns_view);
    if (!data) return NULL;
    return &data->objects[type];
}

static inline void lock(void)
{
    gl_lock_lock(((trackobjects_data *) bugle_object_get_current_data(&bugle_namespace_class, ns_view))->mutex);
}

static inline void unlock(void)
{
    gl_lock_unlock(((trackobjects_data *) bugle_object_get_current_data(&bugle_namespace_class, ns_view))->mutex);
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
                                    GLuint object,
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
                                       GLuint object)
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

#if defined(GL_ARB_vertex_program) || defined(GL_ARB_fragment_program)
static bool trackobjects_glBindProgramARB(function_call *call, const callback_data *data)
{
    /* NVIDIA driver 66.29 has a bug where glIsProgramARB will return
     * false until glProgramStringARB is called. We work around this
     * by blindly assuming that the bind succeeded, unless it is a bind
     * to object 0.
     */
    if (*call->typed.glBindProgramARB.arg1 != 0)
        trackobjects_add_single(BUGLE_TRACKOBJECTS_OLD_PROGRAM,
                                *call->typed.glBindProgramARB.arg0,
                                *call->typed.glBindProgramARB.arg1,
                                -1);
    return true;
}

static bool trackobjects_glDeleteProgramsARB(function_call *call, const callback_data *data)
{
    trackobjects_delete_multiple(BUGLE_TRACKOBJECTS_OLD_PROGRAM,
                                 *call->typed.glDeleteProgramsARB.arg0,
                                 *call->typed.glDeleteProgramsARB.arg1,
                                 FUNC_glIsProgramARB);
    return true;
}
#endif

/* Shader and program objects are tricky, because unlike texture and buffer
 * objects they don't necessarily disappear immediately. Shaders disappear
 * when no longer attached to ANY program, and programs disappear when no
 * longer active in ANY context. The only reliable way to check is to try
 * to get a parameter and see if it generates an error.
 */

typedef struct
{
    bugle_trackobjects_type type;
    GLuint object;
} check_data;

static void init_checks(const void *key, void *data)
{
    bugle_list_init((linked_list *) data, true);
}

static void done_checks(void *data)
{
    bugle_list_clear((linked_list *) data);
}

static void add_check(object *call_object,
                      bugle_trackobjects_type type,
                      GLuint object)
{
    check_data *c;

    c = XMALLOC(check_data);
    c->type = type;
    c->object = object;
    bugle_list_append((linked_list *) bugle_object_get_data(call_object, call_view), c);
}

#ifdef GL_ARB_shader_objects
static bool trackobjects_glCreateShaderObjectARB(function_call *call, const callback_data *data)
{
    trackobjects_add_single(BUGLE_TRACKOBJECTS_SHADER,
                            *call->typed.glCreateShaderObjectARB.arg0,
                            *call->typed.glCreateShaderObjectARB.retn,
                            NULL_FUNCTION);
    return true;
}

static bool trackobjects_glCreateProgramObjectARB(function_call *call, const callback_data *data)
{
    trackobjects_add_single(BUGLE_TRACKOBJECTS_PROGRAM,
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
                attached = XNMALLOC(count, GLhandleARB);
                CALL_glGetAttachedObjectsARB(object, count, NULL, attached);
                for (i = 0; i < count; i++)
                    add_check(data->call_object, BUGLE_TRACKOBJECTS_SHADER, attached[i]);
                free(attached);
            }
            add_check(data->call_object, BUGLE_TRACKOBJECTS_PROGRAM, object);
            break;
        case GL_SHADER_OBJECT_ARB:
            add_check(data->call_object, BUGLE_TRACKOBJECTS_SHADER, object);
            break;
        }
        bugle_end_internal_render("trackobjects_pre_glDeleteObjectARB", true);
    }
    return true;
}

static bool trackobjects_pre_glUseProgramObjectARB(function_call *call, const callback_data *data)
{
    GLhandleARB program;
    GLhandleARB *attached;
    GLint i, count;

    if (bugle_begin_internal_render())
    {
        program = CALL_glGetHandleARB(GL_PROGRAM_OBJECT_ARB);
        if (program != 0)
        {
            add_check(data->call_object, BUGLE_TRACKOBJECTS_PROGRAM, program);
            CALL_glGetObjectParameterivARB(program, GL_OBJECT_ATTACHED_OBJECTS_ARB, &count);
            attached = XNMALLOC(count, GLhandleARB);
            CALL_glGetAttachedObjectsARB(program, count, NULL, attached);
            for (i = 0; i < count; i++)
                add_check(data->call_object, BUGLE_TRACKOBJECTS_SHADER, attached[i]);
            free(attached);
        }
        bugle_end_internal_render("trackobjects_pre_glUseProgramARB", true);
    }
    return true;
}

static bool trackobjects_pre_glDetachObjectARB(function_call *call, const callback_data *data)
{
    add_check(data->call_object, BUGLE_TRACKOBJECTS_SHADER,
              *call->typed.glDetachObjectARB.arg1);
    return true;
}

#endif

#ifdef GL_VERSION_2_0
static bool trackobjects_pre_glDeleteShader(function_call *call, const callback_data *data)
{
    add_check(data->call_object, BUGLE_TRACKOBJECTS_SHADER, *call->typed.glDeleteShader.arg0);
    return true;
}

static bool trackobjects_pre_glDeleteProgram(function_call *call, const callback_data *data)
{
    GLint count, i;
    GLuint *attached;
    GLuint object;

    object = *call->typed.glDeleteProgram.arg0;
    if (bugle_begin_internal_render())
    {
        CALL_glGetProgramiv(object, GL_ATTACHED_SHADERS, &count);
        if (count)
        {
            attached = XNMALLOC(count, GLuint);
            CALL_glGetAttachedShaders(object, count, NULL, attached);
            for (i = 0; i < count; i++)
                add_check(data->call_object, BUGLE_TRACKOBJECTS_SHADER, attached[i]);
            free(attached);
        }
        bugle_end_internal_render("trackobjects_pre_DeleteProgram", true);
    }
    add_check(data->call_object, BUGLE_TRACKOBJECTS_PROGRAM, object);
    return true;
}

#endif

#ifdef GL_EXT_framebuffer_object
static bool trackobjects_glBindFramebuffer(function_call *call, const callback_data *data)
{
    trackobjects_add_single(BUGLE_TRACKOBJECTS_FRAMEBUFFER,
                            *call->typed.glBindFramebufferEXT.arg0,
                            *call->typed.glBindFramebufferEXT.arg1,
                            FUNC_glIsFramebufferEXT);
    return true;
}

static bool trackobjects_glDeleteFramebuffers(function_call *call, const callback_data *data)
{
    trackobjects_delete_multiple(BUGLE_TRACKOBJECTS_FRAMEBUFFER,
                                 *call->typed.glDeleteFramebuffersEXT.arg0,
                                 *call->typed.glDeleteFramebuffersEXT.arg1,
                                 FUNC_glIsFramebufferEXT);
    return true;
}

static bool trackobjects_glBindRenderbuffer(function_call *call, const callback_data *data)
{
    trackobjects_add_single(BUGLE_TRACKOBJECTS_RENDERBUFFER,
                            *call->typed.glBindRenderbufferEXT.arg0,
                            *call->typed.glBindRenderbufferEXT.arg1,
                            FUNC_glIsRenderbufferEXT);
    return true;
}

static bool trackobjects_glDeleteRenderbuffers(function_call *call, const callback_data *data)
{
    trackobjects_delete_multiple(BUGLE_TRACKOBJECTS_RENDERBUFFER,
                                 *call->typed.glDeleteRenderbuffersEXT.arg0,
                                 *call->typed.glDeleteRenderbuffersEXT.arg1,
                                 FUNC_glIsRenderbufferEXT);
    return true;
}
#endif /* GL_EXT_framebuffer_object */

static bool trackobjects_checks(function_call *call, const callback_data *data)
{
    linked_list *l;
    linked_list_node *i;
    const check_data *d;

    l = (linked_list *) bugle_object_get_data(data->call_object, call_view);
    for (i = bugle_list_head(l); i; i = bugle_list_next(i))
    {
        d = (const check_data *) bugle_list_data(i);
        switch (d->type)
        {
#ifdef GL_ARB_shader_objects
        case BUGLE_TRACKOBJECTS_SHADER:
        case BUGLE_TRACKOBJECTS_PROGRAM:
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
        default:
            abort();
        }
    }
    return true;
}

static void initialise_objects(const void *key, void *data)
{
    trackobjects_data *d;
    size_t i;

    d = (trackobjects_data *) data;
    gl_lock_init(d->mutex);
    for (i = 0; i < BUGLE_TRACKOBJECTS_COUNT; i++)
        bugle_radix_tree_init(&d->objects[i], false);
}

static void destroy_objects(void *data)
{
    trackobjects_data *d;
    size_t i;

    d = (trackobjects_data *) data;
    gl_lock_destroy(d->mutex);
    for (i = 0; i < BUGLE_TRACKOBJECTS_COUNT; i++)
        bugle_radix_tree_clear(&d->objects[i]);
}

static bool initialise_trackobjects(filter_set *handle)
{
    filter *f;

    f = bugle_filter_register(handle, "trackobjects_pre");
#ifdef GL_ARB_shader_objects
    bugle_filter_catches(f, GROUP_glDeleteObjectARB, true, trackobjects_pre_glDeleteObjectARB);
    bugle_filter_catches(f, GROUP_glUseProgramObjectARB, true, trackobjects_pre_glUseProgramObjectARB);
    bugle_filter_catches(f, GROUP_glDetachObjectARB, true, trackobjects_pre_glDetachObjectARB);
#endif
#ifdef GL_VERSION_2_0
    bugle_filter_catches(f, GROUP_glDeleteShader, true, trackobjects_pre_glDeleteShader);
    bugle_filter_catches(f, GROUP_glDeleteProgram, true, trackobjects_pre_glDeleteProgram);
#endif

    f = bugle_filter_register(handle, "trackobjects");
    bugle_filter_catches(f, GROUP_glBindTexture, true, trackobjects_glBindTexture);
    bugle_filter_catches(f, GROUP_glDeleteTextures, true, trackobjects_glDeleteTextures);
#ifdef GL_ARB_vertex_buffer_object
    bugle_filter_catches(f, GROUP_glBindBufferARB, true, trackobjects_glBindBuffer);
    bugle_filter_catches(f, GROUP_glDeleteBuffersARB, true, trackobjects_glDeleteBuffers);
#endif
#ifdef GL_ARB_occlusion_query
    bugle_filter_catches(f, GROUP_glBeginQueryARB, true, trackobjects_glBeginQuery);
    bugle_filter_catches(f, GROUP_glDeleteQueriesARB, true, trackobjects_glDeleteQueries);
#endif
#if defined(GL_ARB_vertex_program) || defined(GL_ARB_fragment_program)
    bugle_filter_catches(f, GROUP_glBindProgramARB, true, trackobjects_glBindProgramARB);
    bugle_filter_catches(f, GROUP_glDeleteProgramsARB, true, trackobjects_glDeleteProgramsARB);
#endif
#ifdef GL_ARB_shader_objects
    bugle_filter_catches(f, GROUP_glCreateShaderObjectARB, true, trackobjects_glCreateShaderObjectARB);
    bugle_filter_catches(f, GROUP_glCreateProgramObjectARB, true, trackobjects_glCreateProgramObjectARB);
    bugle_filter_catches(f, GROUP_glDeleteObjectARB, true, trackobjects_checks);
    bugle_filter_catches(f, GROUP_glUseProgramObjectARB, true, trackobjects_checks);
    bugle_filter_catches(f, GROUP_glDetachObjectARB, true, trackobjects_checks);
#endif
#ifdef GL_VERSION_2_0
    bugle_filter_catches(f, GROUP_glDeleteShader, true, trackobjects_checks);
    bugle_filter_catches(f, GROUP_glDeleteProgram, true, trackobjects_checks);
#endif
#ifdef GL_EXT_framebuffer_object
    bugle_filter_catches(f, GROUP_glBindRenderbufferEXT, true, trackobjects_glBindRenderbuffer);
    bugle_filter_catches(f, GROUP_glDeleteRenderbuffersEXT, true, trackobjects_glDeleteRenderbuffers);
    bugle_filter_catches(f, GROUP_glBindFramebufferEXT, true, trackobjects_glBindFramebuffer);
    bugle_filter_catches(f, GROUP_glDeleteFramebuffersEXT, true, trackobjects_glDeleteFramebuffers);
#endif
    bugle_filter_order("invoke", "trackobjects");
    bugle_filter_order("trackobjects_pre", "invoke");
    bugle_filter_post_renders("trackobjects");
    ns_view = bugle_object_view_register(&bugle_namespace_class,
                                         initialise_objects,
                                         destroy_objects,
                                         sizeof(trackobjects_data));
    call_view = bugle_object_view_register(&bugle_call_class,
                                           init_checks,
                                           done_checks,
                                           sizeof(linked_list));
    return true;
}

typedef struct
{
    void (*walker)(GLuint, GLenum, void *);
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
                             void (*walker)(GLuint object,
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

GLenum bugle_trackobjects_get_target(bugle_trackobjects_type type, GLuint id)
{
    bugle_radix_tree *table;
    GLenum ans;

    lock();
    table = get_table(type);
    ans = (GLenum) (size_t) bugle_radix_tree_get(table, id);
    unlock();
    return ans;
}

void trackobjects_initialise(void)
{
    static const filter_set_info trackobjects_info =
    {
        "trackobjects",
        initialise_trackobjects,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL /* no documentation */
    };

    bugle_filter_set_register(&trackobjects_info);
    bugle_filter_set_depends("trackobjects", "trackcontext");
    bugle_filter_set_depends("trackobjects", "trackextensions");
    bugle_filter_set_renders("trackobjects");
}
