/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2007  Bruce Merry
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
#include <stdbool.h>
#include <stdio.h>
#include <bugle/filters.h>
#include <bugle/glutils.h>
#include <bugle/log.h>
#include <budgie/reflect.h>
#include <budgie/addresses.h>

/* Callback passed to bugle_log */
static void trace_callback_call_callback(void *call, FILE *f)
{
    budgie_dump_any_call((generic_function_call *) call, 0, f);
}

static bool trace_callback(function_call *call, const callback_data *data)
{
    bugle_log_callback("trace", "call", BUGLE_LOG_INFO,
                       trace_callback_call_callback, &call->generic);
    return true;
}

static bool trace_initialise(filter_set *handle)
{
    filter *f;

    f = bugle_filter_new(handle, "trace");
    bugle_filter_order("invoke", "trace");
    bugle_filter_catches_all(f, false, trace_callback);
    bugle_filter_post_renders("trace");
    return true;
}

void bugle_initialise_filter_library(void)
{
    static const filter_set_info trace_info =
    {
        "trace",
        trace_initialise,
        NULL,
        NULL,
        NULL,
        NULL,
        "captures a text trace of all calls made"
    };
    bugle_filter_set_new(&trace_info);

    /* No direct rendering, but some of the length functions query state */
    bugle_filter_set_renders("trace");
    /* Some of the queries depend on extensions */
    bugle_filter_set_depends("trace", "trackbeginend");
    bugle_filter_set_depends("trace", "trackextensions");
}
