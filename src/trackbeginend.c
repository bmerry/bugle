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
#if HAVE_PTHREAD_H
# include <pthread.h>
#endif

static size_t trackbeginend_offset;

static bool trackbeginend_callback(function_call *call, const callback_data *data)
{
    bool *begin_end;

    switch (canonical_call(call))
    {
    case CFUNC_glBegin:
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
            begin_end = (bool *) object_get_current_data(&context_class, trackbeginend_offset);
            if (begin_end != NULL) *begin_end = true;
        default: ;
        }
        break;
    case CFUNC_glEnd:
        begin_end = (bool *) object_get_current_data(&context_class, trackbeginend_offset);
        if (begin_end != NULL) *begin_end = false;
        break;
    }
    return true;
}

bool in_begin_end(void)
{
    bool *begin_end;

    begin_end = (bool *) object_get_current_data(&context_class, trackbeginend_offset);
    return !begin_end || *begin_end;
}

static bool initialise_trackbeginend(filter_set *handle)
{
    filter *f;

    f = register_filter(handle, "trackbeginend", trackbeginend_callback);
    register_filter_depends("trackbeginend", "invoke");
    register_filter_catches(f, FUNC_glBegin);
    register_filter_catches(f, FUNC_glEnd);
    register_filter_set_depends("trackbeginend", "trackcontext");
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

    /* This ought to be in the initialise routines, but it is vital that
     * it runs early and we currently have no other way to determine the
     * ordering.
     */
    trackbeginend_offset = object_class_register(&context_class,
                                                 NULL,
                                                 NULL,
                                                 sizeof(bool));
    register_filter_set(&trackbeginend_info);
}
