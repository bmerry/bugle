/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2006, 2008  Bruce Merry
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
#include <stdbool.h>
#include <stddef.h>
#include <bugle/glwin/trackcontext.h>
#include <bugle/gl/glheaders.h>
#include <bugle/gl/glbeginend.h>
#include <bugle/gl/globjects.h>
#include <bugle/filters.h>
#include <bugle/objects.h>
#include <budgie/call.h>
#include <budgie/types.h>

#if BUGLE_GLTYPE_GL
static object_view glbeginend_view;

/* Note: we can't use glGetError to determine whether an error occurred,
 * because if the call was successful then doing so is itself an error. So
 * we are forced to do the validation ourselves.
 */
static bool glbeginend_glBegin(function_call *call, const callback_data *data)
{
    bool *begin_end;

    switch (*call->glBegin.arg0)
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
        begin_end = (bool *) bugle_object_get_current_data(bugle_context_class, glbeginend_view);
        if (begin_end != NULL) *begin_end = true;
    default: /* Avoids compiler warning if GLenum is a C enum */ ;
    }
    return true;
}

static bool glbeginend_glEnd(function_call *call, const callback_data *data)
{
    bool *begin_end;

    /* glEnd can only fail if we weren't in begin/end anyway. */
    begin_end = (bool *) bugle_object_get_current_data(bugle_context_class, glbeginend_view);
    if (begin_end != NULL) *begin_end = false;
    return true;
}

bool bugle_gl_in_begin_end(void)
{
    bool *begin_end;

    begin_end = (bool *) bugle_object_get_current_data(bugle_context_class, glbeginend_view);
    return !begin_end || *begin_end;
}
#else
bool bugle_gl_in_begin_end(void)
{
    return bugle_object_get_current(bugle_context_class) == NULL;
}
#endif

void bugle_gl_filter_post_queries_begin_end(const char *name)
{
    bugle_filter_order("glbeginend", name);
}

static bool glbeginend_filter_set_initialise(filter_set *handle)
{
#if BUGLE_GLTYPE_GL
    filter *f;

    f = bugle_filter_new(handle, "glbeginend");
    bugle_filter_order("invoke", "glbeginend");
    bugle_filter_catches(f, "glBegin", true, glbeginend_glBegin);
    bugle_filter_catches(f, "glEnd", true, glbeginend_glEnd);

    glbeginend_view = bugle_object_view_new(bugle_context_class,
                                               NULL,
                                               NULL,
                                               sizeof(bool));
#endif
    return true;
}

void glbeginend_initialise(void)
{
    static const filter_set_info glbeginend_info =
    {
        "glbeginend",
        glbeginend_filter_set_initialise,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL /* no documentation */
    };

    bugle_filter_set_new(&glbeginend_info);

    bugle_filter_set_depends("glbeginend", "trackcontext");
}
