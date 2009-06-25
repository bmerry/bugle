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
#include <bugle/bool.h>
#include <stdio.h>
#include <stdlib.h>
#include <bugle/gl/glutils.h>
#include <bugle/filters.h>
#include <bugle/memory.h>
#include <bugle/log.h>
#include <budgie/reflect.h>
#include <budgie/addresses.h>

static bugle_bool trace_callback(function_call *call, const callback_data *data)
{
    /* TODO modify bugle_log_callback to pass a bugle_io_writer
     * and use that instead of constructing in memory.
     */
    bugle_io_writer *writer;

    writer = bugle_io_writer_mem_new(256);
    budgie_dump_any_call(&call->generic, 0, writer);
    bugle_log("trace", "call", BUGLE_LOG_INFO, bugle_io_writer_mem_get(writer));
    bugle_io_writer_mem_release(writer);
    bugle_io_writer_close(writer);
    return BUGLE_TRUE;
}

static bugle_bool trace_initialise(filter_set *handle)
{
    filter *f;

    f = bugle_filter_new(handle, "trace");
    bugle_filter_order("invoke", "trace");
    bugle_filter_catches_all(f, BUGLE_FALSE, trace_callback);
    bugle_gl_filter_post_renders("trace");
    return BUGLE_TRUE;
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
    bugle_gl_filter_set_renders("trace");
    /* Some of the queries depend on extensions */
    bugle_filter_set_depends("trace", "glbeginend");
    bugle_filter_set_depends("trace", "glextensions");
}
