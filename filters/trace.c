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
#include "src/filters.h"
#include "src/utils.h"
#include "src/glutils.h"
#include "src/log.h"
#include "common/safemem.h"
#include "common/bool.h"
#include <stdio.h>

static bool trace_callback(function_call *call, const callback_data *data)
{
    GLenum error;
    FILE *f;

    if ((f = bugle_log_header("trace", "call")))
        budgie_dump_any_call(&call->generic, 0, f);
    if ((error = bugle_get_call_error(call)) && (f = bugle_log_header("trace", "error")))
    {
        bugle_dump_GLerror(error, f);
        fputs("\n", f);
    }
    return true;
}

static bool initialise_trace(filter_set *handle)
{
    filter *f;

    f = bugle_register_filter(handle, "trace");
    bugle_register_filter_depends("trace", "invoke");
    bugle_log_register_filter("trace");
    bugle_register_filter_catches_all(f, trace_callback);
    bugle_register_filter_post_renders("trace");
    bugle_register_filter_set_queries_error("trace");
    return true;
}

void bugle_initialise_filter_library(void)
{
    static const filter_set_info trace_info =
    {
        "trace",
        initialise_trace,
        NULL,
        NULL,
        0
    };
    bugle_register_filter_set(&trace_info);

    /* No direct rendering, but some of the length functions query state */
    bugle_register_filter_set_renders("trace");
    /* Some of the queries depend on extensions */
    bugle_register_filter_set_depends("trace", "trackextensions");
}
