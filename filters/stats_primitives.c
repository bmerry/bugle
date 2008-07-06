/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2008  Bruce Merry
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
#include <bugle/gl/glheaders.h>
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <bugle/glwin/trackcontext.h>
#include <bugle/gl/glbeginend.h>
#include <bugle/gl/gldisplaylist.h>
#include <bugle/gl/glutils.h>
#include <bugle/stats.h>
#include <bugle/filters.h>
#include <bugle/objects.h>
#include <bugle/log.h>
#include <budgie/types.h>

static object_view stats_primitives_view;  /* begin/end counting */
static object_view stats_primitives_displaylist_view;
static stats_signal *stats_primitives_batches, *stats_primitives_triangles;

typedef struct
{
    GLenum begin_mode;        /* For counting triangles in immediate mode */
    GLsizei begin_count;      /* FIXME: use per-begin/end object */
} stats_primitives_struct;

typedef struct
{
    GLsizei triangles;
    GLsizei batches;
} stats_primitives_displaylist_struct;

/* Increments the triangle and batch count according to mode */
static void stats_primitives_update(GLenum mode, GLsizei count)
{
    size_t t = 0;
#ifndef GL_ES_VERSION_2_0
    stats_primitives_displaylist_struct *displaylist;
#endif

    switch (mode)
    {
    case GL_TRIANGLES:
        t = count / 3;
        break;
#ifndef GL_ES_VERSION_2_0
    case GL_QUADS:
        t = count / 4 * 2;
        break;
    case GL_QUAD_STRIP:
    case GL_POLYGON:
#endif
    case GL_TRIANGLE_STRIP:
    case GL_TRIANGLE_FAN:
        if (count >= 3)
            t = count - 2;
        break;
#ifdef GL_EXT_geometry_shader4
    case GL_TRIANGLES_ADJACENCY_EXT:
        t = count / 6;
        break;
    case GL_TRIANGLE_STRIP_ADJACENCY_EXT:
        if (count >= 6)
            t = count / 2 - 2;
        break;
#endif
    }

    switch (bugle_displaylist_mode())
    {
    case GL_NONE:
        bugle_stats_signal_add(stats_primitives_triangles, t);
        bugle_stats_signal_add(stats_primitives_batches, 1);
        break;
#ifndef GL_ES_VERSION_2_0
    case GL_COMPILE_AND_EXECUTE:
        bugle_stats_signal_add(stats_primitives_triangles, t);
        bugle_stats_signal_add(stats_primitives_batches, 1);
        /* Fall through */
    case GL_COMPILE:
        displaylist = bugle_object_get_current_data(bugle_displaylist_class, stats_primitives_displaylist_view);
        assert(displaylist);
        displaylist->triangles += t;
        displaylist->batches++;
        break;
#endif
    default:
        abort();
    }
}

#ifndef GL_ES_VERSION_2_0
static bool stats_primitives_immediate(function_call *call, const callback_data *data)
{
    stats_primitives_struct *s;

    if (bugle_gl_in_begin_end())
    {
        s = bugle_object_get_current_data(bugle_context_class, stats_primitives_view);
        s->begin_count++;
    }
    return true;
}

static bool stats_primitives_glBegin(function_call *call, const callback_data *data)
{
    stats_primitives_struct *s;

    s = bugle_object_get_current_data(bugle_context_class, stats_primitives_view);
    s->begin_mode = *call->glBegin.arg0;
    s->begin_count = 0;
    return true;
}

static bool stats_primitives_glEnd(function_call *call, const callback_data *data)
{
    stats_primitives_struct *s;

    s = bugle_object_get_current_data(bugle_context_class, stats_primitives_view);
    stats_primitives_update(s->begin_mode, s->begin_count);
    s->begin_mode = GL_NONE;
    s->begin_count = 0;
    return true;
}
#endif /* !GL_ES_VERSION_2_0 */

static bool stats_primitives_glDrawArrays(function_call *call, const callback_data *data)
{
    stats_primitives_update(*call->glDrawArrays.arg0, *call->glDrawArrays.arg2);
    return true;
}

static bool stats_primitives_glDrawElements(function_call *call, const callback_data *data)
{
    stats_primitives_update(*call->glDrawElements.arg0, *call->glDrawElements.arg1);
    return true;
}

#ifndef GL_ES_VERSION_2_0
static bool stats_primitives_glDrawRangeElements(function_call *call, const callback_data *data)
{
    stats_primitives_update(*call->glDrawRangeElements.arg0, *call->glDrawRangeElementsEXT.arg3);
    return true;
}

static bool stats_primitives_glMultiDrawArrays(function_call *call, const callback_data *data)
{
    GLsizei i, primcount;

    primcount = *call->glMultiDrawArrays.arg3;
    for (i = 0; i < primcount; i++)
        stats_primitives_update(*call->glMultiDrawArrays.arg0,
                                (*call->glMultiDrawArrays.arg2)[i]);
    return true;
}

static bool stats_primitives_glMultiDrawElements(function_call *call, const callback_data *data)
{
    GLsizei i, primcount;

    primcount = *call->glMultiDrawElements.arg4;
    for (i = 0; i < primcount; i++)
        stats_primitives_update(*call->glMultiDrawElements.arg0,
                                (*call->glMultiDrawElements.arg1)[i]);
    return true;
}

static bool stats_primitives_glCallList(function_call *call, const callback_data *data)
{
    stats_primitives_struct *s;
    stats_primitives_displaylist_struct *counts;

    s = bugle_object_get_current_data(bugle_context_class, stats_primitives_view);
    counts = bugle_object_get_data(bugle_displaylist_get(*call->glCallList.arg0),
                                   stats_primitives_displaylist_view);
    if (counts)
    {
        bugle_stats_signal_add(stats_primitives_triangles, counts->triangles);
        bugle_stats_signal_add(stats_primitives_batches, counts->batches);
    }
    return true;
}

static bool stats_primitives_glCallLists(function_call *call, const callback_data *data)
{
    bugle_log("stats_primitives", "glCallLists", BUGLE_LOG_WARNING,
              "triangle counting in glCallLists is not implemented!");
    return true;
}
#endif /* !GL_ES_VERSION_2_0 */

static bool stats_primitives_initialise(filter_set *handle)
{
    filter *f;

    stats_primitives_view = bugle_object_view_new(bugle_context_class,
                                                  NULL,
                                                  NULL,
                                                  sizeof(stats_primitives_struct));
    stats_primitives_displaylist_view = bugle_object_view_new(bugle_displaylist_class,
                                                              NULL,
                                                              NULL,
                                                              sizeof(stats_primitives_struct));

    f = bugle_filter_new(handle, "stats_primitives");
    bugle_filter_catches(f, "glDrawElements", false, stats_primitives_glDrawElements);
    bugle_filter_catches(f, "glDrawArrays", false, stats_primitives_glDrawArrays);
#ifndef GL_ES_VERSION_2_0
    bugle_filter_catches(f, "glDrawRangeElements", false, stats_primitives_glDrawRangeElements);
    bugle_filter_catches(f, "glMultiDrawElements", false, stats_primitives_glMultiDrawElements);
    bugle_filter_catches(f, "glMultiDrawArrays", false, stats_primitives_glMultiDrawArrays);
    bugle_gl_filter_catches_drawing_immediate(f, false, stats_primitives_immediate);
    bugle_filter_catches(f, "glBegin", false, stats_primitives_glBegin);
    bugle_filter_catches(f, "glEnd", false, stats_primitives_glEnd);
    bugle_filter_catches(f, "glCallList", false, stats_primitives_glCallList);
    bugle_filter_catches(f, "glCallLists", false, stats_primitives_glCallLists);
#endif
    bugle_filter_order("stats_primitives", "invoke");
    bugle_filter_order("stats_primitives", "stats");

    stats_primitives_batches = bugle_stats_signal_new("batches", NULL, NULL);
    stats_primitives_triangles = bugle_stats_signal_new("triangles", NULL, NULL);

    return true;
}

void bugle_initialise_filter_library(void)
{
    static const filter_set_info stats_primitives_info =
    {
        "stats_primitives",
        stats_primitives_initialise,
        NULL,
        NULL,
        NULL,
        NULL,
        "stats module: triangles and batches"
    };

    bugle_filter_set_new(&stats_primitives_info);
    bugle_filter_set_depends("stats_primitives", "stats_basic");
    bugle_filter_set_depends("stats_primitives", "trackcontext");
    bugle_filter_set_depends("stats_primitives", "gldisplaylist");
    bugle_filter_set_depends("stats_primitives", "glbeginend");
    bugle_filter_set_stats_generator("stats_primitives");
}
