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
#define _XOPEN_SOURCE 500
#include "src/filters.h"
#include "src/utils.h"
#include "src/glutils.h"
#include "src/tracker.h"
#include "src/log.h"
#include "src/glexts.h"
#include "common/bool.h"
#include <stdio.h>
#include <GL/glx.h>
#include <GL/gl.h>
#include <sys/time.h>
#include <stdarg.h>

typedef struct
{
    /* Resources */
    GLuint query;

    /* Intermediate data for stats */
    struct timeval last_time; /* Last frame - for fps */
    GLsizei begin_mode;       /* For counting triangles in immediate mode */
    GLsizei begin_count;      /* FIXME: use per-begin/end object */

    /* current stats */
    float fps;
    GLuint fragments;
    GLsizei triangles;
} stats_struct;

typedef struct
{
    GLuint font_base;

    struct timeval last_show_time;
    int skip_frames;
    float shown_fps;
} showstats_struct;

static bugle_object_view stats_view;
static bugle_object_view showstats_view;
static bool count_fragments = false;
static bool count_triangles = false;
static bugle_object_view displaylist_view;

static void initialise_stats_struct(const void *key, void *data)
{
    stats_struct *s;

    s = (stats_struct *) data;
    s->query = 0;
    s->last_time.tv_sec = 0;
    s->last_time.tv_usec = 0;
    s->begin_mode = GL_NONE;
    s->begin_count = 0;
#ifdef GL_ARB_occlusion_query
    if (count_fragments && bugle_gl_has_extension(BUGLE_GL_ARB_occlusion_query)
        && bugle_begin_internal_render())
    {
        CALL_glGenQueriesARB(1, &s->query);
        if (s->query)
            CALL_glBeginQueryARB(GL_SAMPLES_PASSED, s->query);
        bugle_end_internal_render("init_stats_struct", true);
    }
#endif

    s->fps = 0.0f;
    s->fragments = 0;
    s->triangles = 0;
}

/* Increments the triangle count for stats struct s according to mode */
static void update_triangles(stats_struct *s, GLenum mode, GLsizei count)
{
    size_t t = 0;
    size_t *displaylist_count;

    switch (mode)
    {
    case GL_TRIANGLES:
        t = count / 3;
        break;
    case GL_QUADS:
        t = count / 4 * 2;
        break;
    case GL_TRIANGLE_STRIP:
    case GL_TRIANGLE_FAN:
    case GL_QUAD_STRIP:
    case GL_POLYGON:
        if (count >= 3)
            t = count - 2;
        break;
    }
    if (!t) return;

    displaylist_count = bugle_object_get_current_data(&bugle_displaylist_class, displaylist_view);
    switch (bugle_displaylist_mode())
    {
    case GL_NONE:
        s->triangles += t;
        break;
    case GL_COMPILE_AND_EXECUTE:
        s->triangles += t;
        /* Fall through */
    case GL_COMPILE:
        assert(displaylist_count);
        *displaylist_count += t;
        break;
    default:
        abort();
    }
}

static bool stats_glXSwapBuffers(function_call *call, const callback_data *data)
{
    stats_struct *s;
    struct timeval now;
    float elapsed;
    FILE *f;

    s = bugle_object_get_current_data(&bugle_context_class, stats_view);
    gettimeofday(&now, NULL);
    elapsed = (now.tv_sec - s->last_time.tv_sec)
        + 1.0e-6f * (now.tv_usec - s->last_time.tv_usec);
    s->last_time = now;
    s->fps = 1.0f / elapsed;
#ifdef GL_ARB_occlusion_query
    if (s->query && bugle_begin_internal_render())
    {
        CALL_glEndQueryARB(GL_SAMPLES_PASSED);
        CALL_glGetQueryObjectuivARB(s->query, GL_QUERY_RESULT_ARB, &s->fragments);
        bugle_end_internal_render("stats_callback", true);
    }
    else
#endif
    {
        s->fragments = 0;
    }

    if ((f = bugle_log_header("stats", "fps")) != NULL)
        fprintf(f, "%.3f\n", s->fps);
    if (s->query && (f = bugle_log_header("stats", "fragments")) != NULL)
        fprintf(f, "%u\n", (unsigned int) s->fragments);
    if (count_triangles && (f = bugle_log_header("stats", "triangles")) != NULL)
        fprintf(f, "%u\n", (unsigned int) s->triangles);
    return true;
}

#ifdef GL_ARB_occlusion_query
static bool stats_fragments(function_call *call, const callback_data *data)
{
    stats_struct *s;

    s = bugle_object_get_current_data(&bugle_context_class, stats_view);
    if (count_fragments)
    {
        s = bugle_object_get_current_data(&bugle_context_class, stats_view);
        if (s->query)
        {
            fputs("App is using occlusion queries, disabling fragment counting\n", stderr);
            s->query = 0;
            s->fragments = 0;
        }
    }
    return true;
}
#endif

static bool stats_immediate(function_call *call, const callback_data *data)
{
    stats_struct *s;

    if (bugle_in_begin_end())
    {
        s = bugle_object_get_current_data(&bugle_context_class, stats_view);
        s->begin_count++;
    }
    return true;
}

static bool stats_glBegin(function_call *call, const callback_data *data)
{
    stats_struct *s;

    s = bugle_object_get_current_data(&bugle_context_class, stats_view);
    s->begin_mode = *call->typed.glBegin.arg0;
    s->begin_count = 0;
    return true;
}

static bool stats_glEnd(function_call *call, const callback_data *data)
{
    stats_struct *s;

    s = bugle_object_get_current_data(&bugle_context_class, stats_view);
    update_triangles(s, s->begin_mode, s->begin_count);
    s->begin_mode = 0;
    s->begin_count = 0;
    return true;
}

static bool stats_glDrawArrays(function_call *call, const callback_data *data)
{
    stats_struct *s;

    s = bugle_object_get_current_data(&bugle_context_class, stats_view);
    update_triangles(s, *call->typed.glDrawArrays.arg0, *call->typed.glDrawArrays.arg2);
    return true;
}

static bool stats_glDrawElements(function_call *call, const callback_data *data)
{
    stats_struct *s;

    s = bugle_object_get_current_data(&bugle_context_class, stats_view);
    update_triangles(s, *call->typed.glDrawElements.arg0, *call->typed.glDrawElements.arg1);
    return true;
}

#ifdef GL_EXT_draw_range_elements
static bool stats_glDrawRangeElements(function_call *call, const callback_data *data)
{
    stats_struct *s;

    s = bugle_object_get_current_data(&bugle_context_class, stats_view);
    update_triangles(s, *call->typed.glDrawRangeElementsEXT.arg0, *call->typed.glDrawRangeElementsEXT.arg3);
    return true;
}
#endif

#ifdef GL_EXT_multi_draw_arrays
static bool stats_glMultiDrawArrays(function_call *call, const callback_data *data)
{
    stats_struct *s;
    GLsizei i, primcount;

    s = bugle_object_get_current_data(&bugle_context_class, stats_view);
    primcount = *call->typed.glMultiDrawArrays.arg3;
    for (i = 0; i < primcount; i++)
        update_triangles(s, *call->typed.glMultiDrawArrays.arg0,
                         (*call->typed.glMultiDrawArrays.arg2)[i]);
    return true;
}

static bool stats_glMultiDrawElements(function_call *call, const callback_data *data)
{
    stats_struct *s;
    GLsizei i, primcount;

    s = bugle_object_get_current_data(&bugle_context_class, stats_view);

    primcount = *call->typed.glMultiDrawElements.arg4;
    for (i = 0; i < primcount; i++)
        update_triangles(s, *call->typed.glMultiDrawElements.arg0,
                         (*call->typed.glMultiDrawElements.arg1)[i]);
    return true;
}
#endif

static bool stats_glCallList(function_call *call, const callback_data *data)
{
    stats_struct *s;
    size_t *count;

    s = bugle_object_get_current_data(&bugle_context_class, stats_view);
    count = bugle_object_get_data(&bugle_displaylist_class,
                                  bugle_displaylist_get(*call->typed.glCallList.arg0),
                                  displaylist_view);
    if (count) s->triangles += *count;
    return true;
}

static bool stats_glCallLists(function_call *call, const callback_data *data)
{
    fputs("FIXME: triangle counting in glCallLists not implemented!\n", stderr);
    return true;
}

static bool stats_post_callback(function_call *call, const callback_data *data)
{
    stats_struct *s;

    s = bugle_object_get_current_data(&bugle_context_class, stats_view);
#ifdef GL_ARB_occlusion_query
    if (s->query && bugle_begin_internal_render())
    {
        CALL_glBeginQueryARB(GL_SAMPLES_PASSED, s->query);
        bugle_end_internal_render("stats_post_callback", true);
    }
#endif
    s->triangles = 0;
    return true;
}

/* Renders a line of text to screen, and moves down one line */
static void render_stats(showstats_struct *ss, const char *fmt, ...)
{
    va_list ap;
    char buffer[128];
    char *ch;

    va_start(ap, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);

    CALL_glPushAttrib(GL_CURRENT_BIT);
    for (ch = buffer; *ch; ch++)
        CALL_glCallList(ss->font_base + *ch);
    CALL_glPopAttrib();
    CALL_glBitmap(0, 0, 0, 0, 0, -16, NULL);
}

static void initialise_showstats_struct(const void *key, void *data)
{
    Display *dpy;
    Font f;
    showstats_struct *ss;

    ss = (showstats_struct *) data;
    dpy = CALL_glXGetCurrentDisplay();
    ss->font_base = CALL_glGenLists(256);
    f = XLoadFont(dpy, "-*-courier-*-*-*");
    CALL_glXUseXFont(f, 0, 256, ss->font_base);
    XUnloadFont(dpy, f);

    ss->shown_fps = 0.0f;
    ss->last_show_time.tv_sec = 0;
    ss->last_show_time.tv_usec = 0;
}

static bool showstats_callback(function_call *call, const callback_data *data)
{
    Display *dpy;
    GLXDrawable old_read, old_write;
    GLXContext aux, real;
    stats_struct *s;
    showstats_struct *ss;
    float elapsed;
    struct timeval now;

    aux = bugle_get_aux_context();
    if (aux && bugle_begin_internal_render())
    {
        real = CALL_glXGetCurrentContext();
        old_write = CALL_glXGetCurrentDrawable();
        old_read = CALL_glXGetCurrentReadDrawable();
        dpy = CALL_glXGetCurrentDisplay();
        CALL_glXMakeContextCurrent(dpy, old_write, old_write, aux);
        s = bugle_object_get_current_data(&bugle_context_class, stats_view);
        ss = bugle_object_get_current_data(&bugle_context_class, showstats_view);

        gettimeofday(&now, NULL);
        elapsed = (now.tv_sec - ss->last_show_time.tv_sec)
            + 1.0e-6f * (now.tv_usec - ss->last_show_time.tv_usec);
        ss->skip_frames++;
        if (elapsed >= 0.2f)
        {
            ss->shown_fps = ss->skip_frames / elapsed;
            ss->last_show_time = now;
            ss->skip_frames = 0;
        }

        /* We don't want to depend on glWindowPos since it
         * needs OpenGL 1.4, but fortunately the aux context
         * has identity MVP matrix.
         */
        CALL_glPushAttrib(GL_CURRENT_BIT);
        CALL_glRasterPos2f(-0.9, 0.9);
        render_stats(ss, "%.1f fps", ss->shown_fps);
        if (s->query) render_stats(ss, "%u fragments", (unsigned int) s->fragments);
        if (count_triangles) render_stats(ss, "%u triangles", (unsigned int) s->triangles);
        CALL_glPopAttrib();
        CALL_glXMakeContextCurrent(dpy, old_write, old_read, real);
        bugle_end_internal_render("showstats_callback", true);
    }
    return true;
}

static bool initialise_stats(filter_set *handle)
{
    filter *f;

    f = bugle_register_filter(handle, "stats");
    bugle_register_filter_catches(f, GROUP_glXSwapBuffers, stats_glXSwapBuffers);
    if (count_fragments)
    {
        bugle_register_filter_catches(f, GROUP_glBeginQueryARB, stats_fragments);
        bugle_register_filter_catches(f, GROUP_glEndQueryARB, stats_fragments);
    }
    if (count_triangles)
    {
        bugle_register_filter_catches_drawing_immediate(f, stats_immediate);
        bugle_register_filter_catches(f, GROUP_glDrawElements, stats_glDrawElements);
        bugle_register_filter_catches(f, GROUP_glDrawArrays, stats_glDrawArrays);
#ifdef GL_EXT_draw_range_elements
        bugle_register_filter_catches(f, GROUP_glDrawRangeElementsEXT, stats_glDrawRangeElements);
#endif
#ifdef GL_EXT_multi_draw_arrays
        bugle_register_filter_catches(f, GROUP_glMultiDrawElementsEXT, stats_glMultiDrawElements);
        bugle_register_filter_catches(f, GROUP_glMultiDrawArraysEXT, stats_glMultiDrawArrays);
#endif

        bugle_register_filter_catches(f, GROUP_glBegin, stats_glBegin);
        bugle_register_filter_catches(f, GROUP_glEnd, stats_glEnd);
        bugle_register_filter_catches(f, GROUP_glCallList, stats_glCallList);
        bugle_register_filter_catches(f, GROUP_glCallLists, stats_glCallLists);
    }
    bugle_register_filter_depends("invoke", "stats");

    if (count_triangles || count_fragments)
    {
        f = bugle_register_filter(handle, "stats_post");
        if (count_fragments || count_triangles)
            bugle_register_filter_catches(f, GROUP_glXSwapBuffers, stats_post_callback);
        bugle_register_filter_post_renders("stats_post");
        bugle_register_filter_depends("stats_post", "invoke");
    }
    bugle_log_register_filter("stats");
    stats_view = bugle_object_class_register(&bugle_context_class,
                                             initialise_stats_struct,
                                             NULL,
                                             sizeof(stats_struct));
    if (count_triangles)
        displaylist_view = bugle_object_class_register(&bugle_displaylist_class,
                                                       NULL,
                                                       NULL,
                                                       sizeof(size_t));
    return true;
}

static bool initialise_showstats(filter_set *handle)
{
    filter *f;

    f = bugle_register_filter(handle, "showstats");
    bugle_register_filter_depends("showstats", "stats");
    bugle_register_filter_depends("invoke", "showstats");
    bugle_register_filter_depends("screenshot", "showstats");
    /* make sure that screenshots capture the stats */
    bugle_register_filter_depends("debugger", "showstats");
    bugle_register_filter_depends("screenshot", "showstats");
    bugle_register_filter_catches(f, GROUP_glXSwapBuffers, showstats_callback);
    showstats_view = bugle_object_class_register(&bugle_context_class,
                                                 initialise_showstats_struct,
                                                 NULL,
                                                 sizeof(showstats_struct));
    return true;
}

void bugle_initialise_filter_library(void)
{
    static const filter_set_variable_info stats_variables[] =
    {
        { "fragments", "count fragments that pass the depth test [no]", FILTER_SET_VARIABLE_BOOL, &count_fragments, NULL },
        { "triangles", "count the number of triangles draw [no]", FILTER_SET_VARIABLE_BOOL, &count_triangles, NULL },
        { NULL, NULL, 0, NULL, NULL }
    };

    static const filter_set_info stats_info =
    {
        "stats",
        initialise_stats,
        NULL,
        stats_variables,
        0,
        "collects statistical information such as frame-rate"
    };

    static const filter_set_info showstats_info =
    {
        "showstats",
        initialise_showstats,
        NULL,
        NULL,
        0,
        "renders information collected by `stats' onto the screen"
    };

    bugle_register_filter_set(&stats_info);
    bugle_register_filter_set(&showstats_info);

    bugle_register_filter_set_renders("stats");
    bugle_register_filter_set_depends("stats", "trackcontext");
    bugle_register_filter_set_depends("stats", "trackextensions");
    bugle_register_filter_set_depends("stats", "trackdisplaylist");

    bugle_register_filter_set_depends("showstats", "stats");
    bugle_register_filter_set_renders("showstats");
}
