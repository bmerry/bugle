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

    if ((f = log_header("trace", "call")))
        dump_any_call(&call->generic, 0, f);
    if ((error = get_call_error(call)) && (f = log_header("trace", "error")))
    {
        dump_GLerror(error, f);
        fputs("\n", f);
    }
    return true;
}

static bool initialise_trace(filter_set *handle)
{
    filter *f;

    f = register_filter(handle, "trace", trace_callback);
    register_filter_depends("trace", "invoke");
    log_register_filter("trace");
    register_filter_catches_all(f);
    /* No direct rendering, but some of the length functions query state */
    register_filter_set_renders("trace");
    register_filter_post_renders("trace");
    register_filter_set_queries_error("trace", false);
    return true;
}

void initialise_filter_library(void)
{
    const filter_set_info trace_info =
    {
        "trace",
        initialise_trace,
        NULL,
        NULL,
        0,
        0
    };
    register_filter_set(&trace_info);
}
