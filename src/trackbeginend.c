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

/* Track whether we are in glBegin/glEnd. This is trickier than it sounds,
 * because glBegin can fail if given an illegal primitive, and we can't
 * check for the error because glGetError is illegal inside glBegin/glEnd.
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "src/filters.h"
#include "src/utils.h"
#include "src/canon.h"
#include "src/tracker.h"
#include "src/objects.h"
#include "common/bool.h"
#include <stddef.h>
#include <GL/gl.h>

static bugle_object_view trackbeginend_view;

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
        begin_end = (bool *) bugle_object_get_current_data(&bugle_context_class, trackbeginend_view);
        if (begin_end != NULL) *begin_end = true;
    default: /* Avoids compiler warning if GLenum is a C enum */ ;
    }
    return true;
}

static bool trackbeginend_glEnd(function_call *call, const callback_data *data)
{
    bool *begin_end;

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

static bool initialise_trackbeginend(filter_set *handle)
{
    filter *f;

    f = bugle_register_filter(handle, "trackbeginend");
    bugle_register_filter_depends("trackbeginend", "invoke");
    bugle_register_filter_catches(f, FUNC_glBegin, trackbeginend_glBegin);
    bugle_register_filter_catches(f, FUNC_glEnd, trackbeginend_glEnd);

    trackbeginend_view = bugle_object_class_register(&bugle_context_class,
                                                     NULL,
                                                     NULL,
                                                     sizeof(bool));
    return true;
}

void trackbeginend_initialise(void)
{
    const filter_set_info trackbeginend_info =
    {
        "trackbeginend",
        initialise_trackbeginend,
        NULL,
        NULL,
        0
    };

    bugle_register_filter_set(&trackbeginend_info);

    bugle_register_filter_set_depends("trackbeginend", "trackcontext");
}
