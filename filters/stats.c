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

#include "src/filters.h"
#include "src/utils.h"
#include "src/canon.h"
#include "src/glutils.h"
#include "src/tracker.h"
#include "src/log.h"
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

static size_t stats_offset;
static size_t showstats_offset;
static bool count_fragments = false;
static bool count_triangles = false;
static size_t displaylist_offset;

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
    /* FIXME: check for the extension, not the function */
    if (count_fragments && budgie_function_table[FUNC_glGenQueriesARB].real
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

    displaylist_count = bugle_object_get_current_data(&bugle_displaylist_class, displaylist_offset);
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

static bool stats_callback(function_call *call, const callback_data *data)
{
    stats_struct *s;
    struct timeval now;
    float elapsed;
    FILE *f;
    GLsizei i, primcount;
    budgie_function canon;
    size_t *count;

    s = bugle_object_get_current_data(&bugle_context_class, stats_offset);
    canon = bugle_canonical_call(call);
    switch (canon)
    {
    case CFUNC_glXSwapBuffers:
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
    if (count_fragments)
        switch (canon)
        {
        case CFUNC_glBeginQueryARB:
        case CFUNC_glEndQueryARB:
            if (s->query)
            {
                fputs("App is using occlusion queries, disabling fragment counting\n", stderr);
                s->query = 0;
                s->fragments = 0;
            }
            break;
        }
#endif

    if (count_triangles)
        switch (canon)
        {
#ifdef GL_ARB_vertex_program
        case CFUNC_glVertexAttrib1sARB:
        case CFUNC_glVertexAttrib1fARB:
        case CFUNC_glVertexAttrib1dARB:
        case CFUNC_glVertexAttrib2sARB:
        case CFUNC_glVertexAttrib2fARB:
        case CFUNC_glVertexAttrib2dARB:
        case CFUNC_glVertexAttrib3sARB:
        case CFUNC_glVertexAttrib3fARB:
        case CFUNC_glVertexAttrib3dARB:
        case CFUNC_glVertexAttrib4sARB:
        case CFUNC_glVertexAttrib4fARB:
        case CFUNC_glVertexAttrib4dARB:
        case CFUNC_glVertexAttrib4NubARB:
        case CFUNC_glVertexAttrib1svARB:
        case CFUNC_glVertexAttrib1fvARB:
        case CFUNC_glVertexAttrib1dvARB:
        case CFUNC_glVertexAttrib2svARB:
        case CFUNC_glVertexAttrib2fvARB:
        case CFUNC_glVertexAttrib2dvARB:
        case CFUNC_glVertexAttrib3svARB:
        case CFUNC_glVertexAttrib3fvARB:
        case CFUNC_glVertexAttrib3dvARB:
        case CFUNC_glVertexAttrib4bvARB:
        case CFUNC_glVertexAttrib4svARB:
        case CFUNC_glVertexAttrib4ivARB:
        case CFUNC_glVertexAttrib4ubvARB:
        case CFUNC_glVertexAttrib4usvARB:
        case CFUNC_glVertexAttrib4uivARB:
        case CFUNC_glVertexAttrib4fvARB:
        case CFUNC_glVertexAttrib4dvARB:
        case CFUNC_glVertexAttrib4NbvARB:
        case CFUNC_glVertexAttrib4NsvARB:
        case CFUNC_glVertexAttrib4NivARB:
        case CFUNC_glVertexAttrib4NubvARB:
        case CFUNC_glVertexAttrib4NusvARB:
        case CFUNC_glVertexAttrib4NuivARB:
            if (*(GLuint *) call->generic.args[0] != 0) break;
            /* fall through */
#endif
        case CFUNC_glVertex2d:
        case CFUNC_glVertex2dv:
        case CFUNC_glVertex2f:
        case CFUNC_glVertex2fv:
        case CFUNC_glVertex2i:
        case CFUNC_glVertex2iv:
        case CFUNC_glVertex2s:
        case CFUNC_glVertex2sv:
        case CFUNC_glVertex3d:
        case CFUNC_glVertex3dv:
        case CFUNC_glVertex3f:
        case CFUNC_glVertex3fv:
        case CFUNC_glVertex3i:
        case CFUNC_glVertex3iv:
        case CFUNC_glVertex3s:
        case CFUNC_glVertex3sv:
        case CFUNC_glVertex4d:
        case CFUNC_glVertex4dv:
        case CFUNC_glVertex4f:
        case CFUNC_glVertex4fv:
        case CFUNC_glVertex4i:
        case CFUNC_glVertex4iv:
        case CFUNC_glVertex4s:
        case CFUNC_glVertex4sv:
        case CFUNC_glArrayElement:
            if (bugle_in_begin_end()) s->begin_count++;
            break;
        case CFUNC_glBegin:
            s->begin_mode = *call->typed.glBegin.arg0;
            s->begin_count = 0;
            break;
        case CFUNC_glEnd:
            update_triangles(s, s->begin_mode, s->begin_count);
            s->begin_mode = 0;
            s->begin_count = 0;
            break;
        case CFUNC_glDrawArrays:
            update_triangles(s, *call->typed.glDrawArrays.arg0, *call->typed.glDrawArrays.arg2);
            break;
        case CFUNC_glDrawElements:
            update_triangles(s, *call->typed.glDrawElements.arg0, *call->typed.glDrawElements.arg1);
            break;
#ifdef GL_EXT_draw_range_elements
        case CFUNC_glDrawRangeElementsEXT:
            update_triangles(s, *call->typed.glDrawRangeElementsEXT.arg0, *call->typed.glDrawRangeElementsEXT.arg3);
            break;
#endif
#ifdef GL_EXT_multi_draw_arrays
        case CFUNC_glMultiDrawArraysEXT:
            primcount = *call->typed.glMultiDrawArrays.arg3;
            for (i = 0; i < primcount; i++)
                update_triangles(s, *call->typed.glMultiDrawArrays.arg0,
                                 (*call->typed.glMultiDrawArrays.arg2)[i]);
            break;
        case CFUNC_glMultiDrawElementsEXT:
            primcount = *call->typed.glMultiDrawElements.arg4;
            for (i = 0; i < primcount; i++)
                update_triangles(s, *call->typed.glMultiDrawElements.arg0,
                                 (*call->typed.glMultiDrawElements.arg1)[i]);
            break;
#endif
        case CFUNC_glCallList:
            count = bugle_object_get_data(&bugle_displaylist_class,
                                          bugle_displaylist_get(*call->typed.glCallList.arg0),
                                          displaylist_offset);
            if (count) s->triangles += *count;
            break;
        case CFUNC_glCallLists:
            fputs("FIXME: triangle counting in glCallLists not implemented!\n", stderr);
            break;
        }
    return true;
}

static bool stats_post_callback(function_call *call, const callback_data *data)
{
    stats_struct *s;

    switch (bugle_canonical_call(call))
    {
    case CFUNC_glXSwapBuffers:
        s = bugle_object_get_current_data(&bugle_context_class, stats_offset);
#ifdef GL_ARB_occlusion_query
        if (s->query && bugle_begin_internal_render())
        {
            CALL_glBeginQueryARB(GL_SAMPLES_PASSED, s->query);
            bugle_end_internal_render("stats_post_callback", true);
        }
#endif
        s->triangles = 0;
        break;
    }
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

    switch (bugle_canonical_call(call))
    {
    case FUNC_glXSwapBuffers:
        aux = bugle_get_aux_context();
        if (aux && bugle_begin_internal_render())
        {
            real = CALL_glXGetCurrentContext();
            old_write = CALL_glXGetCurrentDrawable();
            old_read = CALL_glXGetCurrentReadDrawable();
            dpy = CALL_glXGetCurrentDisplay();
            CALL_glXMakeContextCurrent(dpy, old_write, old_write, aux);
            s = bugle_object_get_current_data(&bugle_context_class, stats_offset);
            ss = bugle_object_get_current_data(&bugle_context_class, showstats_offset);

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
        break;
    }
    return true;
}

static bool command_stats(filter_set *handle, const char *name, const char *value)
{
    if (strcmp(name, "fragments") == 0)
    {
        if (strcmp(value, "yes") == 0)
            count_fragments = true;
        else if (strcmp(value, "no") == 0)
            count_fragments = false;
        else
            fprintf(stderr, "illegal fragments value '%s' (should be yes or no)\n", value);
    }
    else if (strcmp(name, "triangles") == 0)
    {
        if (strcmp(value, "yes") == 0)
            count_triangles = true;
        else if (strcmp(value, "no") == 0)
            count_triangles = false;
        else
            fprintf(stderr, "illegal triangles value '%s' (should be yes or no)\n", value);
    }
    else
        return false;
    return true;
}

static bool initialise_stats(filter_set *handle)
{
    filter *f;

    f = bugle_register_filter(handle, "stats", stats_callback);
    bugle_register_filter_catches(f, CFUNC_glXSwapBuffers);
    if (count_triangles)
    {
#ifdef GL_ARB_vertex_program
        bugle_register_filter_catches(f, CFUNC_glVertexAttrib1sARB);
        bugle_register_filter_catches(f, CFUNC_glVertexAttrib1fARB);
        bugle_register_filter_catches(f, CFUNC_glVertexAttrib1dARB);
        bugle_register_filter_catches(f, CFUNC_glVertexAttrib2sARB);
        bugle_register_filter_catches(f, CFUNC_glVertexAttrib2fARB);
        bugle_register_filter_catches(f, CFUNC_glVertexAttrib2dARB);
        bugle_register_filter_catches(f, CFUNC_glVertexAttrib3sARB);
        bugle_register_filter_catches(f, CFUNC_glVertexAttrib3fARB);
        bugle_register_filter_catches(f, CFUNC_glVertexAttrib3dARB);
        bugle_register_filter_catches(f, CFUNC_glVertexAttrib4sARB);
        bugle_register_filter_catches(f, CFUNC_glVertexAttrib4fARB);
        bugle_register_filter_catches(f, CFUNC_glVertexAttrib4dARB);
        bugle_register_filter_catches(f, CFUNC_glVertexAttrib4NubARB);
        bugle_register_filter_catches(f, CFUNC_glVertexAttrib1svARB);
        bugle_register_filter_catches(f, CFUNC_glVertexAttrib1fvARB);
        bugle_register_filter_catches(f, CFUNC_glVertexAttrib1dvARB);
        bugle_register_filter_catches(f, CFUNC_glVertexAttrib2svARB);
        bugle_register_filter_catches(f, CFUNC_glVertexAttrib2fvARB);
        bugle_register_filter_catches(f, CFUNC_glVertexAttrib2dvARB);
        bugle_register_filter_catches(f, CFUNC_glVertexAttrib3svARB);
        bugle_register_filter_catches(f, CFUNC_glVertexAttrib3fvARB);
        bugle_register_filter_catches(f, CFUNC_glVertexAttrib3dvARB);
        bugle_register_filter_catches(f, CFUNC_glVertexAttrib4bvARB);
        bugle_register_filter_catches(f, CFUNC_glVertexAttrib4svARB);
        bugle_register_filter_catches(f, CFUNC_glVertexAttrib4ivARB);
        bugle_register_filter_catches(f, CFUNC_glVertexAttrib4ubvARB);
        bugle_register_filter_catches(f, CFUNC_glVertexAttrib4usvARB);
        bugle_register_filter_catches(f, CFUNC_glVertexAttrib4uivARB);
        bugle_register_filter_catches(f, CFUNC_glVertexAttrib4fvARB);
        bugle_register_filter_catches(f, CFUNC_glVertexAttrib4dvARB);
        bugle_register_filter_catches(f, CFUNC_glVertexAttrib4NbvARB);
        bugle_register_filter_catches(f, CFUNC_glVertexAttrib4NsvARB);
        bugle_register_filter_catches(f, CFUNC_glVertexAttrib4NivARB);
        bugle_register_filter_catches(f, CFUNC_glVertexAttrib4NubvARB);
        bugle_register_filter_catches(f, CFUNC_glVertexAttrib4NusvARB);
        bugle_register_filter_catches(f, CFUNC_glVertexAttrib4NuivARB);
#endif
        bugle_register_filter_catches(f, CFUNC_glVertex2d);
        bugle_register_filter_catches(f, CFUNC_glVertex2dv);
        bugle_register_filter_catches(f, CFUNC_glVertex2f);
        bugle_register_filter_catches(f, CFUNC_glVertex2fv);
        bugle_register_filter_catches(f, CFUNC_glVertex2i);
        bugle_register_filter_catches(f, CFUNC_glVertex2iv);
        bugle_register_filter_catches(f, CFUNC_glVertex2s);
        bugle_register_filter_catches(f, CFUNC_glVertex2sv);
        bugle_register_filter_catches(f, CFUNC_glVertex3d);
        bugle_register_filter_catches(f, CFUNC_glVertex3dv);
        bugle_register_filter_catches(f, CFUNC_glVertex3f);
        bugle_register_filter_catches(f, CFUNC_glVertex3fv);
        bugle_register_filter_catches(f, CFUNC_glVertex3i);
        bugle_register_filter_catches(f, CFUNC_glVertex3iv);
        bugle_register_filter_catches(f, CFUNC_glVertex3s);
        bugle_register_filter_catches(f, CFUNC_glVertex3sv);
        bugle_register_filter_catches(f, CFUNC_glVertex4d);
        bugle_register_filter_catches(f, CFUNC_glVertex4dv);
        bugle_register_filter_catches(f, CFUNC_glVertex4f);
        bugle_register_filter_catches(f, CFUNC_glVertex4fv);
        bugle_register_filter_catches(f, CFUNC_glVertex4i);
        bugle_register_filter_catches(f, CFUNC_glVertex4iv);
        bugle_register_filter_catches(f, CFUNC_glVertex4s);
        bugle_register_filter_catches(f, CFUNC_glVertex4sv);
        bugle_register_filter_catches(f, CFUNC_glArrayElement);
        bugle_register_filter_catches(f, CFUNC_glBegin);
        bugle_register_filter_catches(f, CFUNC_glEnd);
        bugle_register_filter_catches(f, CFUNC_glDrawElements);
        bugle_register_filter_catches(f, CFUNC_glDrawArrays);
#ifdef GL_EXT_draw_range_elements
        bugle_register_filter_catches(f, CFUNC_glDrawRangeElementsEXT);
#endif
#ifdef GL_EXT_multi_draw_arrays
        bugle_register_filter_catches(f, CFUNC_glMultiDrawElementsEXT);
        bugle_register_filter_catches(f, CFUNC_glMultiDrawArraysEXT);
#endif
        bugle_register_filter_catches(f, CFUNC_glCallList);
        bugle_register_filter_catches(f, CFUNC_glCallLists);
    }
    bugle_register_filter_depends("invoke", "stats");

    if (count_triangles || count_fragments)
    {
        f = bugle_register_filter(handle, "stats_post", stats_post_callback);
        if (count_fragments || count_triangles)
            bugle_register_filter_catches(f, CFUNC_glXSwapBuffers);
        bugle_register_filter_post_renders("stats_post");
        bugle_register_filter_depends("stats_post", "invoke");
    }
    bugle_log_register_filter("stats");
    bugle_register_filter_set_renders("stats");
    bugle_register_filter_set_depends("stats", "trackcontext");
    if (count_triangles)
        bugle_register_filter_set_depends("stats", "trackdisplaylist");
    stats_offset = bugle_object_class_register(&bugle_context_class,
                                               initialise_stats_struct,
                                               NULL,
                                               sizeof(stats_struct));
    if (count_triangles)
        displaylist_offset = bugle_object_class_register(&bugle_displaylist_class,
                                                         NULL,
                                                         NULL,
                                                         sizeof(size_t));
    return true;
}

static bool initialise_showstats(filter_set *handle)
{
    filter *f;

    f = bugle_register_filter(handle, "showstats", showstats_callback);
    bugle_register_filter_depends("showstats", "stats");
    bugle_register_filter_depends("invoke", "showstats");
    bugle_register_filter_depends("screenshot", "showstats");
    /* make sure that screenshots capture the stats */
    bugle_register_filter_depends("debugger", "showstats");
    bugle_register_filter_depends("screenshot", "showstats");
    bugle_register_filter_catches(f, CFUNC_glXSwapBuffers);
    bugle_register_filter_set_depends("showstats", "stats");
    bugle_register_filter_set_renders("showstats");
    showstats_offset = bugle_object_class_register(&bugle_context_class,
                                                   initialise_showstats_struct,
                                                   NULL,
                                                   sizeof(showstats_struct));
    return true;
}

void bugle_initialise_filter_library(void)
{
    const filter_set_info stats_info =
    {
        "stats",
        initialise_stats,
        NULL,
        command_stats,
        0,
    };
    const filter_set_info showstats_info =
    {
        "showstats",
        initialise_showstats,
        NULL,
        NULL,
        0
    };
    bugle_register_filter_set(&stats_info);
    bugle_register_filter_set(&showstats_info);
}
