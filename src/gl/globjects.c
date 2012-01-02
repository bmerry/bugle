/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2009, 2011  Bruce Merry
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
#include <stddef.h>
#include <bugle/bool.h>
#include <bugle/memory.h>
#include <bugle/glwin/trackcontext.h>
#include <bugle/gl/glheaders.h>
#include <bugle/gl/glfbo.h>
#include <bugle/gl/glutils.h>
#include <bugle/gl/glsl.h>
#include <bugle/gl/globjects.h>
#include <bugle/gl/glfbo.h>
#include <bugle/filters.h>
#include <bugle/hashtable.h>
#include <budgie/types.h>
#include <budgie/call.h>
#include "platform/threads.h"

typedef struct
{
    bugle_thread_lock_t mutex;
    hashptr_table objects[BUGLE_GLOBJECTS_COUNT];
} globjects_data;

/* FIXME: check which types of objects are shared between contexts (all but queries I think) */

static object_view ns_view, context_view, call_view;

static bugle_bool object_type_shared(bugle_globjects_type type)
{
    switch (type)
    {
    case BUGLE_GLOBJECTS_QUERY:
    case BUGLE_GLOBJECTS_VERTEX_ARRAY:
    case BUGLE_GLOBJECTS_FRAMEBUFFER:
        /* ARB_framebuffer_object doesn't allow sharing. This is thus wrong for
         * EXT_framebuffer_object, but the interactions aren't defined anyway.
         */
        return BUGLE_FALSE;
    default:
        return BUGLE_TRUE;
    }
    return BUGLE_TRUE; /* should never be reached */
}

static hashptr_table *get_table(bugle_globjects_type type)
{
    globjects_data *data;

    if (object_type_shared(type))
        data = bugle_object_get_current_data(bugle_get_namespace_class(), ns_view);
    else
        data = bugle_object_get_current_data(bugle_get_context_class(), context_view);
    if (!data) return NULL;
    return &data->objects[type];
}

static inline void lock(void)
{
    bugle_thread_lock_lock(&((globjects_data *) bugle_object_get_current_data(bugle_get_namespace_class(), ns_view))->mutex);
}

static inline void unlock(void)
{
    bugle_thread_lock_unlock(&((globjects_data *) bugle_object_get_current_data(bugle_get_namespace_class(), ns_view))->mutex);
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
static void globjects_add_single(bugle_globjects_type type,
                                 GLenum target,
                                 GLuint object,
                                 GLboolean (BUDGIEAPIP is)(GLuint))
{
    hashptr_table *table;

    lock();
    table = get_table(type);
    if (table && bugle_gl_begin_internal_render())
    {
        if (is == NULL || is(object))
            bugle_hashptr_set_int(table, object, (void *) (size_t) target);
        bugle_gl_end_internal_render("globjects_add_single", BUGLE_TRUE);
    }
    unlock();
}

static void globjects_delete_multiple(bugle_globjects_type type,
                                      GLsizei count,
                                      const GLuint *objects,
                                      GLboolean (BUDGIEAPIP is)(GLuint))
{
    GLsizei i;
    hashptr_table *table;

    lock();
    table = get_table(type);
    if (table && bugle_gl_begin_internal_render())
    {
        for (i = 0; i < count; i++)
            if (is == NULL || !is(objects[i]))
                bugle_hashptr_set_int(table, objects[i], NULL);
        bugle_gl_end_internal_render("globjects_delete_multiple", BUGLE_TRUE);
    }
    unlock();
}

static bugle_bool globjects_glBindTexture(function_call *call, const callback_data *data)
{
    globjects_add_single(BUGLE_GLOBJECTS_TEXTURE,
                         *call->glBindTexture.arg0,
                         *call->glBindTexture.arg1,
                         CALL(glIsTexture));
    return BUGLE_TRUE;
}

static bugle_bool globjects_glDeleteTextures(function_call *call, const callback_data *data)
{
    globjects_delete_multiple(BUGLE_GLOBJECTS_TEXTURE,
                              *call->glDeleteTextures.arg0,
                              *call->glDeleteTextures.arg1,
                              CALL(glIsTexture));
    return BUGLE_TRUE;
}

static bugle_bool globjects_glBindBuffer(function_call *call, const callback_data *data)
{
    globjects_add_single(BUGLE_GLOBJECTS_BUFFER,
                         *call->glBindBuffer.arg0,
                         *call->glBindBuffer.arg1,
                         CALL(glIsBuffer));
    return BUGLE_TRUE;
}

static bugle_bool globjects_glDeleteBuffers(function_call *call, const callback_data *data)
{
    globjects_delete_multiple(BUGLE_GLOBJECTS_BUFFER,
                              *call->glDeleteBuffers.arg0,
                              *call->glDeleteBuffers.arg1,
                              CALL(glIsBuffer));
    return BUGLE_TRUE;
}

#if BUGLE_GLTYPE_GL
static bugle_bool globjects_glBeginQuery(function_call *call, const callback_data *data)
{
    globjects_add_single(BUGLE_GLOBJECTS_QUERY,
                         *call->glBeginQuery.arg0,
                         *call->glBeginQuery.arg1,
                         CALL(glIsQuery));
    return BUGLE_TRUE;
}

static bugle_bool globjects_glDeleteQueries(function_call *call, const callback_data *data)
{
    globjects_delete_multiple(BUGLE_GLOBJECTS_QUERY,
                              *call->glDeleteQueries.arg0,
                              *call->glDeleteQueries.arg1,
                              CALL(glIsQueryARB));
    return BUGLE_TRUE;
}
#endif

#if GL_ES_VERSION_2_0 || GL_VERSION_2_0
static void globjects_delete_single(bugle_globjects_type type,
                                       GLuint object)
{
    hashptr_table *table;

    lock();
    table = get_table(type);
    if (table)
        bugle_hashptr_set_int(table, object, NULL);
    unlock();
}
#endif /* GL_ES_VERSION_2_0 || GL_VERSION_2_0 */

#if defined(GL_ARB_vertex_program) || defined(GL_ARB_fragment_program)
static bugle_bool globjects_glBindProgramARB(function_call *call, const callback_data *data)
{
    /* NVIDIA driver 66.29 has a bug where glIsProgramARB will return
     * BUGLE_FALSE until glProgramStringARB is called. We work around this
     * by blindly assuming that the bind succeeded, unless it is a bind
     * to object 0.
     */
    if (*call->glBindProgramARB.arg1 != 0)
        globjects_add_single(BUGLE_GLOBJECTS_OLD_PROGRAM,
                             *call->glBindProgramARB.arg0,
                             *call->glBindProgramARB.arg1,
                             NULL);
    return BUGLE_TRUE;
}

static bugle_bool globjects_glDeleteProgramsARB(function_call *call, const callback_data *data)
{
    globjects_delete_multiple(BUGLE_GLOBJECTS_OLD_PROGRAM,
                              *call->glDeleteProgramsARB.arg0,
                              *call->glDeleteProgramsARB.arg1,
                              CALL(glIsProgramARB));
    return BUGLE_TRUE;
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
    bugle_globjects_type type;
    GLuint object;
} check_data;

static void checks_init(const void *key, void *data)
{
    bugle_list_init((linked_list *) data, bugle_free);
}

static void add_check(object *call_object,
                      bugle_globjects_type type,
                      GLuint object)
{
    check_data *c;

    c = BUGLE_MALLOC(check_data);
    c->type = type;
    c->object = object;
    bugle_list_append((linked_list *) bugle_object_get_data(call_object, call_view), c);
}

#if GL_ES_VERSION_2_0 || GL_VERSION_2_0
static bugle_bool globjects_glCreateShader(function_call *call, const callback_data *data)
{
    globjects_add_single(BUGLE_GLOBJECTS_SHADER,
                         *call->glCreateShader.arg0,
                         *call->glCreateShader.retn,
                         NULL);
    return BUGLE_TRUE;
}

static bugle_bool globjects_glCreateProgram(function_call *call, const callback_data *data)
{
    globjects_add_single(BUGLE_GLOBJECTS_PROGRAM,
                         1,  /* arbitrary non-zero value */
                         *call->glCreateProgram.retn,
                         NULL);
    return BUGLE_TRUE;
}

/* Used for glDeleteProgram, glUseProgram and glDeleteObjectARB */
static void globjects_pre_glsl_delete_program(GLuint object, const callback_data *data)
{
    GLint count, i;
    GLuint *attached;

    bugle_glGetProgramiv(object, GL_ATTACHED_SHADERS, &count);
    if (count)
    {
        attached = BUGLE_NMALLOC(count, GLuint);
        bugle_glGetAttachedShaders(object, count, NULL, attached);
        for (i = 0; i < count; i++)
            add_check(data->call_object, BUGLE_GLOBJECTS_SHADER, attached[i]);
        bugle_free(attached);
    }
    add_check(data->call_object, BUGLE_GLOBJECTS_PROGRAM, object);
}

#ifdef GL_ARB_shader_objects
static bugle_bool globjects_pre_glDeleteObjectARB(function_call *call, const callback_data *data)
{
    GLhandleARB object;
    GLint type;

    if (bugle_gl_begin_internal_render())
    {
        object = *call->glDeleteObjectARB.arg0;
        CALL(glGetObjectParameterivARB)(object, GL_OBJECT_TYPE_ARB, &type);
        switch (type)
        {
        case GL_PROGRAM_OBJECT_ARB:
            globjects_pre_glsl_delete_program(object, data);
            break;
        case GL_SHADER_OBJECT_ARB:
            add_check(data->call_object, BUGLE_GLOBJECTS_SHADER, object);
            break;
        }
        bugle_gl_end_internal_render("globjects_pre_glDeleteObjectARB", BUGLE_TRUE);
    }
    return BUGLE_TRUE;
}
#endif

static bugle_bool globjects_pre_glUseProgram(function_call *call, const callback_data *data)
{
    GLuint program;

    if (bugle_gl_begin_internal_render())
    {
        program = bugle_gl_get_current_program();
        if (program != 0)
            globjects_pre_glsl_delete_program(program, data);
        bugle_gl_end_internal_render("globjects_pre_glUseProgram", BUGLE_TRUE);
    }
    return BUGLE_TRUE;
}

static bugle_bool globjects_pre_glDetachShader(function_call *call, const callback_data *data)
{
    add_check(data->call_object, BUGLE_GLOBJECTS_SHADER,
              *call->glDetachShader.arg1);
    return BUGLE_TRUE;
}

static bugle_bool globjects_pre_glDeleteShader(function_call *call, const callback_data *data)
{
    add_check(data->call_object, BUGLE_GLOBJECTS_SHADER, *call->glDeleteShader.arg0);
    return BUGLE_TRUE;
}

static bugle_bool globjects_pre_glDeleteProgram(function_call *call, const callback_data *data)
{
    GLuint object;

    object = *call->glDeleteProgram.arg0;
    if (bugle_gl_begin_internal_render())
    {
        globjects_pre_glsl_delete_program(object, data);
        bugle_gl_end_internal_render("globjects_pre_glDeleteProgram", BUGLE_TRUE);
    }
    return BUGLE_TRUE;
}

static bugle_bool globjects_checks(function_call *call, const callback_data *data)
{
    linked_list *l;
    linked_list_node *i;
    const check_data *d;

    l = (linked_list *) bugle_object_get_data(data->call_object, call_view);
    for (i = bugle_list_head(l); i; i = bugle_list_next(i))
    {
        d = (const check_data *) bugle_list_data(i);
        if (bugle_gl_begin_internal_render())
        {
            switch (d->type)
            {
            case BUGLE_GLOBJECTS_SHADER:
                if (!bugle_glIsShader(d->object))
                    globjects_delete_single(d->type, d->object);
                break;
            case BUGLE_GLOBJECTS_PROGRAM:
                if (!bugle_glIsProgram(d->object))
                    globjects_delete_single(d->type, d->object);
                break;
            default:
                abort();
            }
            bugle_gl_end_internal_render("globjects_checks", BUGLE_TRUE);
        }
    }
    return BUGLE_TRUE;
}
#endif /* GLES2 || GL2 */

static bugle_bool globjects_glBindFramebuffer(function_call *call, const callback_data *data)
{
    globjects_add_single(BUGLE_GLOBJECTS_FRAMEBUFFER,
                         *(const GLenum *) call->generic.args[0],
                         *(const GLuint *) call->generic.args[1],
                         bugle_glIsFramebuffer);
    return BUGLE_TRUE;
}

static bugle_bool globjects_glDeleteFramebuffers(function_call *call, const callback_data *data)
{
    globjects_delete_multiple(BUGLE_GLOBJECTS_FRAMEBUFFER,
                              *(const GLsizei *) call->generic.args[0],
                              *(const GLuint * const *) call->generic.args[1],
                              bugle_glIsFramebuffer);
    return BUGLE_TRUE;
}

static bugle_bool globjects_glBindRenderbuffer(function_call *call, const callback_data *data)
{
    globjects_add_single(BUGLE_GLOBJECTS_RENDERBUFFER,
                         *(const GLenum *) call->generic.args[0],
                         *(const GLuint *) call->generic.args[1],
                         bugle_glIsRenderbuffer);
    return BUGLE_TRUE;
}

static bugle_bool globjects_glDeleteRenderbuffers(function_call *call, const callback_data *data)
{
    globjects_delete_multiple(BUGLE_GLOBJECTS_RENDERBUFFER,
                              *(const GLsizei *) call->generic.args[0],
                              *(const GLuint * const *) call->generic.args[1],
                              bugle_glIsRenderbuffer);
    return BUGLE_TRUE;
}

#if BUGLE_GLTYPE_GL
static bugle_bool globjects_glBindVertexArray(function_call *call, const callback_data *data)
{
    globjects_add_single(BUGLE_GLOBJECTS_VERTEX_ARRAY,
                         1,  /* arbitrary non-zero value */
                         *call->glBindVertexArray.arg0,
                         CALL(glIsVertexArray));
    return BUGLE_TRUE;
}

static bugle_bool globjects_glDeleteVertexArrays(function_call *call, const callback_data *data)
{
    globjects_delete_multiple(BUGLE_GLOBJECTS_VERTEX_ARRAY,
                              *call->glDeleteVertexArrays.arg0,
                              *call->glDeleteVertexArrays.arg1,
                              CALL(glIsVertexArray));
    return BUGLE_TRUE;
}
#endif

static void globjects_data_init(const void *key, void *data)
{
    globjects_data *d;
    size_t i;

    d = (globjects_data *) data;
    bugle_thread_lock_init(&d->mutex);
    for (i = 0; i < BUGLE_GLOBJECTS_COUNT; i++)
        bugle_hashptr_init(&d->objects[i], NULL);
}

static void globjects_data_clear(void *data)
{
    globjects_data *d;
    size_t i;

    d = (globjects_data *) data;
    bugle_thread_lock_destroy(&d->mutex);
    for (i = 0; i < BUGLE_GLOBJECTS_COUNT; i++)
        bugle_hashptr_clear(&d->objects[i]);
}

static bugle_bool globjects_filter_set_initialise(filter_set *handle)
{
    filter *f;

#if GL_ES_VERSION_2_0 || GL_VERSION_2_0
    f = bugle_filter_new(handle, "globjects_pre");
#ifdef GL_ARB_shader_objects
    bugle_filter_catches(f, "glDeleteObjectARB", BUGLE_TRUE, globjects_pre_glDeleteObjectARB);
#endif
    bugle_filter_catches(f, "glUseProgram", BUGLE_TRUE, globjects_pre_glUseProgram);
    bugle_filter_catches(f, "glDetachShader", BUGLE_TRUE, globjects_pre_glDetachShader);
    bugle_filter_catches(f, "glDeleteShader", BUGLE_TRUE, globjects_pre_glDeleteShader);
    bugle_filter_catches(f, "glDeleteProgram", BUGLE_TRUE, globjects_pre_glDeleteProgram);
#endif

    f = bugle_filter_new(handle, "globjects");
    bugle_filter_catches(f, "glBindTexture", BUGLE_TRUE, globjects_glBindTexture);
    bugle_filter_catches(f, "glDeleteTextures", BUGLE_TRUE, globjects_glDeleteTextures);
    bugle_filter_catches(f, "glBindBuffer", BUGLE_TRUE, globjects_glBindBuffer);
    bugle_filter_catches(f, "glDeleteBuffers", BUGLE_TRUE, globjects_glDeleteBuffers);
#if BUGLE_GLTYPE_GL
    bugle_filter_catches(f, "glBeginQuery", BUGLE_TRUE, globjects_glBeginQuery);
    bugle_filter_catches(f, "glDeleteQueries", BUGLE_TRUE, globjects_glDeleteQueries);
    bugle_filter_catches(f, "glBindVertexArray", BUGLE_TRUE, globjects_glBindVertexArray);
    bugle_filter_catches(f, "glDeleteVertexArrays", BUGLE_TRUE, globjects_glDeleteVertexArrays);
#endif
#if defined(GL_ARB_vertex_program) || defined(GL_ARB_fragment_program)
    bugle_filter_catches(f, "glBindProgramARB", BUGLE_TRUE, globjects_glBindProgramARB);
    bugle_filter_catches(f, "glDeleteProgramsARB", BUGLE_TRUE, globjects_glDeleteProgramsARB);
#endif
#ifdef GL_ARB_shader_objects
    bugle_filter_catches(f, "glDeleteObjectARB", BUGLE_TRUE, globjects_checks);
#endif
#if GL_ES_VERSION_2_0 || GL_VERSION_2_0
    bugle_filter_catches(f, "glUseProgram", BUGLE_TRUE, globjects_checks);
    bugle_filter_catches(f, "glDetachShader", BUGLE_TRUE, globjects_checks);
    bugle_filter_catches(f, "glCreateShader", BUGLE_TRUE, globjects_glCreateShader);
    bugle_filter_catches(f, "glCreateProgram", BUGLE_TRUE, globjects_glCreateProgram);
    bugle_filter_catches(f, "glDeleteShader", BUGLE_TRUE, globjects_checks);
    bugle_filter_catches(f, "glDeleteProgram", BUGLE_TRUE, globjects_checks);
#endif

    bugle_gl_filter_catches_glBindRenderbuffer(f, BUGLE_TRUE, globjects_glBindRenderbuffer);
    bugle_gl_filter_catches_glDeleteRenderbuffers(f, BUGLE_TRUE, globjects_glDeleteRenderbuffers);
    bugle_gl_filter_catches_glBindFramebuffer(f, BUGLE_TRUE, globjects_glBindFramebuffer);
    bugle_gl_filter_catches_glDeleteFramebuffers(f, BUGLE_TRUE, globjects_glDeleteFramebuffers);

    bugle_filter_order("invoke", "globjects");
    bugle_filter_order("globjects_pre", "invoke");
    bugle_gl_filter_post_renders("globjects");
    ns_view = bugle_object_view_new(bugle_get_namespace_class(),
                                    globjects_data_init,
                                    globjects_data_clear,
                                    sizeof(globjects_data));
    context_view = bugle_object_view_new(bugle_get_context_class(),
                                         globjects_data_init,
                                         globjects_data_clear,
                                         sizeof(globjects_data));
    call_view = bugle_object_view_new(bugle_get_call_class(),
                                      checks_init,
                                      (void (*)(void *)) bugle_list_clear,
                                      sizeof(linked_list));
    return BUGLE_TRUE;
}

typedef struct
{
    void (*walker)(GLuint, GLenum, void *);
    void *data;
} globjects_walker;

static int cmp_size_t(const void *a, const void *b)
{
    size_t A, B;
    A = *(const size_t *) a;
    B = *(const size_t *) b;
    return (A == B) ? 0 : (A < B) ? -1 : 1;
}

void bugle_globjects_walk(bugle_globjects_type type,
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
    keyvalues = (size_t (*)[2]) bugle_nmalloc(count, sizeof(size_t [2]));
    for (i = bugle_hashptr_begin(table), j = 0; i; i = bugle_hashptr_next(table, i))
        if (i->value)
        {
            keyvalues[j][0] = (size_t) i->key;
            keyvalues[j][1] = (size_t) i->value;
            j++;
        }
    qsort(keyvalues, count, 2 * sizeof(size_t), cmp_size_t);
    for (j = 0; j < count; j++)
        (*walker)(keyvalues[j][0], keyvalues[j][1], data);
    bugle_free(keyvalues);
    unlock();
}

GLenum bugle_globjects_get_target(bugle_globjects_type type, GLuint id)
{
    hashptr_table *table;
    GLenum ans;

    lock();
    table = get_table(type);
    ans = (GLenum) (size_t) bugle_hashptr_get_int(table, id);
    unlock();
    return ans;
}

void globjects_initialise(void)
{
    static const filter_set_info globjects_info =
    {
        "globjects",
        globjects_filter_set_initialise,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL /* no documentation */
    };

    bugle_filter_set_new(&globjects_info);
    bugle_filter_set_depends("globjects", "trackcontext");
    bugle_filter_set_depends("globjects", "glextensions");
    bugle_gl_filter_set_renders("globjects");
}
