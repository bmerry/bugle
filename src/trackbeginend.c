/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2006  Bruce Merry
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

/* Track whether we are in glBegin/glEnd. This is trickier than it sounds,
 * because glBegin can fail if given an illegal primitive, and we can't
 * check for the error because glGetError is illegal inside glBegin/glEnd.
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "src/filters.h"
#include "src/utils.h"
#include "src/tracker.h"
#include "src/objects.h"
#include <stdbool.h>
#include <stddef.h>
#include <GL/gl.h>

static object_view trackbeginend_view;

/* Note: we can't use glGetError to determine whether an error occurred,
 * because if the call was successful then doing so is itself an error. So
 * we are forced to do the validation ourselves.
 */
static bool trackbeginend_glBegin(function_call *call, const callback_data *data)
{
    bool *begin_end;

    switch (*call->typed.glBegin.arg0)
    {
    case GL_POINTS:
    case GL_LINES:
    case GL_LINE_STRIP:
    case GL_LINE_LOOP:
    case GL_TRIANGLES:
    case GL_TRIANGLE_STRIP:
    case GL_TRIANGLE_FAN:
    case GL_QUADS:
    case GL_QUAD_STRIP:
    case GL_POLYGON:
#ifdef GL_EXT_geometry_shader4
    case GL_LINES_ADJACENCY_EXT:
    case GL_LINE_STRIP_ADJACENCY_EXT:
    case GL_TRIANGLES_ADJACENCY_EXT:
    case GL_TRIANGLE_STRIP_ADJACENCY_EXT:
#endif
        begin_end = (bool *) bugle_object_get_current_data(&bugle_context_class, trackbeginend_view);
        if (begin_end != NULL) *begin_end = true;
    default: /* Avoids compiler warning if GLenum is a C enum */ ;
    }
    return true;
}

static bool trackbeginend_glEnd(function_call *call, const callback_data *data)
{
    bool *begin_end;

    /* glEnd can only fail if we weren't in begin/end anyway. */
    begin_end = (bool *) bugle_object_get_current_data(&bugle_context_class, trackbeginend_view);
    if (begin_end != NULL) *begin_end = false;
    return true;
}

bool bugle_in_begin_end(void)
{
    bool *begin_end;

    begin_end = (bool *) bugle_object_get_current_data(&bugle_context_class, trackbeginend_view);
    return !begin_end || *begin_end;
}

void bugle_filter_post_queries_begin_end(const char *name)
{
    bugle_filter_order("trackbeginend", name);
}

static bool trackbeginend_filter_set_initialise(filter_set *handle)
{
    filter *f;

    f = bugle_filter_register(handle, "trackbeginend");
    bugle_filter_order("invoke", "trackbeginend");
    bugle_filter_catches(f, GROUP_glBegin, true, trackbeginend_glBegin);
    bugle_filter_catches(f, GROUP_glEnd, true, trackbeginend_glEnd);

    trackbeginend_view = bugle_object_view_register(&bugle_context_class,
                                                     NULL,
                                                     NULL,
                                                     sizeof(bool));
    return true;
}

void trackbeginend_initialise(void)
{
    static const filter_set_info trackbeginend_info =
    {
        "trackbeginend",
        trackbeginend_filter_set_initialise,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL /* no documentation */
    };

    bugle_filter_set_register(&trackbeginend_info);

    bugle_filter_set_depends("trackbeginend", "trackcontext");
}
