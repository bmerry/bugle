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
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <bugle/filters.h>
#include <bugle/tracker.h>
#include <bugle/glutils.h>
#include <bugle/hashtable.h>
#include <budgie/types.h>
#include <budgie/call.h>
#include <bugle/glsl.h>
#include "xalloc.h"
#include "lock.h"

typedef struct
{
    gl_lock_define(, mutex);
    hashptr_table objects[BUGLE_TRACKOBJECTS_COUNT];
} trackobjects_data;

/* FIXME: check which types of objects are shared between contexts (all but queries I think) */

static object_view ns_view, call_view;

static hashptr_table *get_table(bugle_trackobjects_type type)
{
    trackobjects_data *data;

    data = bugle_object_get_current_data(bugle_namespace_class, ns_view);
    if (!data) return NULL;
    return &data->objects[type];
}

static inline void lock(void)
{
    gl_lock_lock(((trackobjects_data *) bugle_object_get_current_data(bugle_namespace_class, ns_view))->mutex);
}

static inline void unlock(void)
{
    gl_lock_unlock(((trackobjects_data *) bugle_object_get_current_data(bugle_namespace_class, ns_view))->mutex);
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
                                    GLboolean (*is)(GLuint))
{
    hashptr_table *table;

    lock();
    table = get_table(type);
    if (table && bugle_begin_internal_render())
    {
        if (is == NULL || is(object))
            bugle_hashptr_set_int(table, object, (void *) (size_t) target);
        bugle_end_internal_render("trackobjects_add_single", true);
    }
    unlock();
}

static void trackobjects_delete_single(bugle_trackobjects_type type,
                                       GLuint object)
{
    hashptr_table *table;

    lock();
    table = get_table(type);
    if (table)
        bugle_hashptr_set_int(table, object, NULL);
    unlock();
}

static void trackobjects_delete_multiple(bugle_trackobjects_type type,
                                         GLsizei count,
                                         const GLuint *objects,
                                         GLboolean (*is)(GLuint))
{
    GLsizei i;
    hashptr_table *table;

    lock();
    table = get_table(type);
    if (table && bugle_begin_internal_render())
    {
        for (i = 0; i < count; i++)
            if (is == NULL || !is(objects[i]))
                bugle_hashptr_set_int(table, objects[i], NULL);
        bugle_end_internal_render("trackobjects_delete_multiple", true);
    }
    unlock();
}

static bool trackobjects_glBindTexture(function_call *call, const callback_data *data)
{
    trackobjects_add_single(BUGLE_TRACKOBJECTS_TEXTURE,
                            *call->glBindTexture.arg0,
                            *call->glBindTexture.arg1,
                            CALL(glIsTexture));
    return true;
}

static bool trackobjects_glDeleteTextures(function_call *call, const callback_data *data)
{
    trackobjects_delete_multiple(BUGLE_TRACKOBJECTS_TEXTURE,
                                 *call->glDeleteTextures.arg0,
                                 *call->glDeleteTextures.arg1,
                                 CALL(glIsTexture));
    return true;
}

#ifdef GL_ARB_vertex_buffer_object
static bool trackobjects_glBindBuffer(function_call *call, const callback_data *data)
{
    trackobjects_add_single(BUGLE_TRACKOBJECTS_BUFFER,
                            *call->glBindBufferARB.arg0,
                            *call->glBindBufferARB.arg1,
                            CALL(glIsBufferARB));
    return true;
}

static bool trackobjects_glDeleteBuffers(function_call *call, const callback_data *data)
{
    trackobjects_delete_multiple(BUGLE_TRACKOBJECTS_BUFFER,
                                 *call->glDeleteBuffersARB.arg0,
                                 *call->glDeleteBuffersARB.arg1,
                                 CALL(glIsBufferARB));
    return true;
}
#endif

#ifdef GL_ARB_occlusion_query
static bool trackobjects_glBeginQuery(function_call *call, const callback_data *data)
{
    trackobjects_add_single(BUGLE_TRACKOBJECTS_QUERY,
                            *call->glBeginQueryARB.arg0,
                            *call->glBeginQueryARB.arg1,
                            CALL(glIsQueryARB));
    return true;
}

static bool trackobjects_glDeleteQueries(function_call *call, const callback_data *data)
{
    trackobjects_delete_multiple(BUGLE_TRACKOBJECTS_QUERY,
                                 *call->glDeleteQueriesARB.arg0,
                                 *call->glDeleteQueriesARB.arg1,
                                 CALL(glIsQueryARB));
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
    if (*call->glBindProgramARB.arg1 != 0)
        trackobjects_add_single(BUGLE_TRACKOBJECTS_OLD_PROGRAM,
                                *call->glBindProgramARB.arg0,
                                *call->glBindProgramARB.arg1,
                                NULL);
    return true;
}

static bool trackobjects_glDeleteProgramsARB(function_call *call, const callback_data *data)
{
    trackobjects_delete_multiple(BUGLE_TRACKOBJECTS_OLD_PROGRAM,
                                 *call->glDeleteProgramsARB.arg0,
                                 *call->glDeleteProgramsARB.arg1,
                                 CALL(glIsProgramARB));
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

static void checks_init(const void *key, void *data)
{
    bugle_list_init((linked_list *) data, free);
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
                            *call->glCreateShaderObjectARB.arg0,
                            *call->glCreateShaderObjectARB.retn,
                            NULL);
    return true;
}

static bool trackobjects_glCreateProgramObjectARB(function_call *call, const callback_data *data)
{
    trackobjects_add_single(BUGLE_TRACKOBJECTS_PROGRAM,
                            GL_PROGRAM_OBJECT_ARB,
                            *call->glCreateProgramObjectARB.retn,
                            NULL);
    return true;
}

/* Used for glDeleteProgram, glUseProgram, glDeleteObjectARB and
 * glUseProgramObjectARB.
 */
static void trackobjects_pre_glsl_delete_program(GLuint object, const callback_data *data)
{
    GLint count, i;
    GLuint *attached;

    bugle_glGetProgramiv(object, GL_OBJECT_ATTACHED_OBJECTS_ARB, &count);
    if (count)
    {
        attached = XNMALLOC(count, GLuint);
        bugle_glGetAttachedShaders(object, count, NULL, attached);
        for (i = 0; i < count; i++)
            add_check(data->call_object, BUGLE_TRACKOBJECTS_SHADER, attached[i]);
        free(attached);
    }
    add_check(data->call_object, BUGLE_TRACKOBJECTS_PROGRAM, object);
}

static bool trackobjects_pre_glDeleteObjectARB(function_call *call, const callback_data *data)
{
    GLhandleARB object;
    GLint type;

    if (bugle_begin_internal_render())
    {
        object = *call->glDeleteObjectARB.arg0;
        CALL(glGetObjectParameterivARB)(object, GL_OBJECT_TYPE_ARB, &type);
        switch (type)
        {
        case GL_PROGRAM_OBJECT_ARB:
            trackobjects_pre_glsl_delete_program(object, data);
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

    if (bugle_begin_internal_render())
    {
        program = bugle_glGetHandleARB(GL_PROGRAM_OBJECT_ARB);
        if (program != 0)
            trackobjects_pre_glsl_delete_program(program, data);
        bugle_end_internal_render("trackobjects_pre_glUseProgramObjectARB", true);
    }
    return true;
}

static bool trackobjects_pre_glDetachObjectARB(function_call *call, const callback_data *data)
{
    add_check(data->call_object, BUGLE_TRACKOBJECTS_SHADER,
              *call->glDetachObjectARB.arg1);
    return true;
}

#endif

#ifdef GL_VERSION_2_0
static bool trackobjects_pre_glDeleteShader(function_call *call, const callback_data *data)
{
    add_check(data->call_object, BUGLE_TRACKOBJECTS_SHADER, *call->glDeleteShader.arg0);
    return true;
}

static bool trackobjects_pre_glDeleteProgram(function_call *call, const callback_data *data)
{
    GLuint object;

    object = *call->glDeleteProgram.arg0;
    if (bugle_begin_internal_render())
    {
        trackobjects_pre_glsl_delete_program(object, data);
        bugle_end_internal_render("trackobjects_pre_glDeleteProgram", true);
    }
    return true;
}

#endif

#ifdef GL_EXT_framebuffer_object
static bool trackobjects_glBindFramebuffer(function_call *call, const callback_data *data)
{
    trackobjects_add_single(BUGLE_TRACKOBJECTS_FRAMEBUFFER,
                            *call->glBindFramebufferEXT.arg0,
                            *call->glBindFramebufferEXT.arg1,
                            CALL(glIsFramebufferEXT));
    return true;
}

static bool trackobjects_glDeleteFramebuffers(function_call *call, const callback_data *data)
{
    trackobjects_delete_multiple(BUGLE_TRACKOBJECTS_FRAMEBUFFER,
                                 *call->glDeleteFramebuffersEXT.arg0,
                                 *call->glDeleteFramebuffersEXT.arg1,
                                 CALL(glIsFramebufferEXT));
    return true;
}

static bool trackobjects_glBindRenderbuffer(function_call *call, const callback_data *data)
{
    trackobjects_add_single(BUGLE_TRACKOBJECTS_RENDERBUFFER,
                            *call->glBindRenderbufferEXT.arg0,
                            *call->glBindRenderbufferEXT.arg1,
                            CALL(glIsRenderbufferEXT));
    return true;
}

static bool trackobjects_glDeleteRenderbuffers(function_call *call, const callback_data *data)
{
    trackobjects_delete_multiple(BUGLE_TRACKOBJECTS_RENDERBUFFER,
                                 *call->glDeleteRenderbuffersEXT.arg0,
                                 *call->glDeleteRenderbuffersEXT.arg1,
                                 CALL(glIsRenderbufferEXT));
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
        if (bugle_begin_internal_render())
        {
            switch (d->type)
            {
#ifdef GL_ARB_shader_objects
            case BUGLE_TRACKOBJECTS_SHADER:
                if (!bugle_glIsShader(d->object))
                    trackobjects_delete_single(d->type, d->object);
                break;
            case BUGLE_TRACKOBJECTS_PROGRAM:
                if (!bugle_glIsProgram(d->object))
                    trackobjects_delete_single(d->type, d->object);
                break;
#endif
            default:
                abort();
            }
            bugle_end_internal_render("trackobjects_checks", true);
        }
    }
    return true;
}

static void trackobjects_data_init(const void *key, void *data)
{
    trackobjects_data *d;
    size_t i;

    d = (trackobjects_data *) data;
    gl_lock_init(d->mutex);
    for (i = 0; i < BUGLE_TRACKOBJECTS_COUNT; i++)
        bugle_hashptr_init(&d->objects[i], NULL);
}

static void trackobjects_data_clear(void *data)
{
    trackobjects_data *d;
    size_t i;

    d = (trackobjects_data *) data;
    gl_lock_destroy(d->mutex);
    for (i = 0; i < BUGLE_TRACKOBJECTS_COUNT; i++)
        bugle_hashptr_clear(&d->objects[i]);
}

static bool trackobjects_filter_set_initialise(filter_set *handle)
{
    filter *f;

    f = bugle_filter_new(handle, "trackobjects_pre");
#ifdef GL_ARB_shader_objects
    bugle_filter_catches(f, "glDeleteObjectARB", true, trackobjects_pre_glDeleteObjectARB);
    bugle_filter_catches(f, "glUseProgramObjectARB", true, trackobjects_pre_glUseProgramObjectARB);
    bugle_filter_catches(f, "glDetachObjectARB", true, trackobjects_pre_glDetachObjectARB);
#endif
#ifdef GL_VERSION_2_0
    bugle_filter_catches(f, "glDeleteShader", true, trackobjects_pre_glDeleteShader);
    bugle_filter_catches(f, "glDeleteProgram", true, trackobjects_pre_glDeleteProgram);
#endif

    f = bugle_filter_new(handle, "trackobjects");
    bugle_filter_catches(f, "glBindTexture", true, trackobjects_glBindTexture);
    bugle_filter_catches(f, "glDeleteTextures", true, trackobjects_glDeleteTextures);
#ifdef GL_ARB_vertex_buffer_object
    bugle_filter_catches(f, "glBindBufferARB", true, trackobjects_glBindBuffer);
    bugle_filter_catches(f, "glDeleteBuffersARB", true, trackobjects_glDeleteBuffers);
#endif
#ifdef GL_ARB_occlusion_query
    bugle_filter_catches(f, "glBeginQueryARB", true, trackobjects_glBeginQuery);
    bugle_filter_catches(f, "glDeleteQueriesARB", true, trackobjects_glDeleteQueries);
#endif
#if defined(GL_ARB_vertex_program) || defined(GL_ARB_fragment_program)
    bugle_filter_catches(f, "glBindProgramARB", true, trackobjects_glBindProgramARB);
    bugle_filter_catches(f, "glDeleteProgramsARB", true, trackobjects_glDeleteProgramsARB);
#endif
#ifdef GL_ARB_shader_objects
    bugle_filter_catches(f, "glCreateShaderObjectARB", true, trackobjects_glCreateShaderObjectARB);
    bugle_filter_catches(f, "glCreateProgramObjectARB", true, trackobjects_glCreateProgramObjectARB);
    bugle_filter_catches(f, "glDeleteObjectARB", true, trackobjects_checks);
    bugle_filter_catches(f, "glUseProgramObjectARB", true, trackobjects_checks);
    bugle_filter_catches(f, "glDetachObjectARB", true, trackobjects_checks);
#endif
#ifdef GL_VERSION_2_0
    bugle_filter_catches(f, "glDeleteShader", true, trackobjects_checks);
    bugle_filter_catches(f, "glDeleteProgram", true, trackobjects_checks);
#endif
#ifdef GL_EXT_framebuffer_object
    bugle_filter_catches(f, "glBindRenderbufferEXT", true, trackobjects_glBindRenderbuffer);
    bugle_filter_catches(f, "glDeleteRenderbuffersEXT", true, trackobjects_glDeleteRenderbuffers);
    bugle_filter_catches(f, "glBindFramebufferEXT", true, trackobjects_glBindFramebuffer);
    bugle_filter_catches(f, "glDeleteFramebuffersEXT", true, trackobjects_glDeleteFramebuffers);
#endif
    bugle_filter_order("invoke", "trackobjects");
    bugle_filter_order("trackobjects_pre", "invoke");
    bugle_filter_post_renders("trackobjects");
    ns_view = bugle_object_view_new(bugle_namespace_class,
                                    trackobjects_data_init,
                                    trackobjects_data_clear,
                                    sizeof(trackobjects_data));
    call_view = bugle_object_view_new(bugle_call_class,
                                      checks_init,
                                      (void (*)(void *)) bugle_list_clear,
                                      sizeof(linked_list));
    return true;
}

typedef struct
{
    void (*walker)(GLuint, GLenum, void *);
    void *data;
} trackobjects_walker;

static int cmp_size_t(const void *a, const void *b)
{
    size_t A, B;
    A = *(const size_t *) a;
    B = *(const size_t *) b;
    return (A == B) ? 0 : (A < B) ? -1 : 1;
}

void bugle_trackobjects_walk(bugle_trackobjects_type type,
                             void (*walker)(GLuint object,
                                            GLenum target,
                                            void *),
                             void *data)
{
    hashptr_table *table;
    const hashptr_table_entry *i;
    size_t count = 0, j;
    size_t (*keyvalues)[2]; /* pointer to 2 size_t's */

    lock();
    table = get_table(type);
    for (i = bugle_hashptr_begin(table); i; i = bugle_hashptr_next(table, i))
        if (i->value)
            count++;
    keyvalues = (size_t (*)[2]) xnmalloc(count, sizeof(size_t [2]));
    for (i = bugle_hashptr_begin(table), j = 0; i; i = bugle_hashptr_next(table, i), j++)
        if (i->value)
        {
            keyvalues[j][0] = (size_t) i->key;
            keyvalues[j][1] = (size_t) i->value;
        }
    qsort(keyvalues, count, 2 * sizeof(size_t), cmp_size_t);
    for (j = 0; j < count; j++)
        (*walker)(keyvalues[j][0], keyvalues[j][1], data);
    free(keyvalues);
    unlock();
}

GLenum bugle_trackobjects_get_target(bugle_trackobjects_type type, GLuint id)
{
    hashptr_table *table;
    GLenum ans;

    lock();
    table = get_table(type);
    ans = (GLenum) (size_t) bugle_hashptr_get_int(table, id);
    unlock();
    return ans;
}

void trackobjects_initialise(void)
{
    static const filter_set_info trackobjects_info =
    {
        "trackobjects",
        trackobjects_filter_set_initialise,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL /* no documentation */
    };

    bugle_filter_set_new(&trackobjects_info);
    bugle_filter_set_depends("trackobjects", "trackcontext");
    bugle_filter_set_depends("trackobjects", "trackextensions");
    bugle_filter_set_renders("trackobjects");
}
