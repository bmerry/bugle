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
#define _POSIX_SOURCE /* for flockfile */
#include "src/filters.h"
#include "src/utils.h"
#include "src/glutils.h"
#include "common/safemem.h"
#include "common/bool.h"
#include <stdio.h>

static char *log_filename = NULL;
static bool log_flush = false;
static FILE *log_file;

static bool log_callback(function_call *call, const callback_data *data)
{
    GLenum error;

    flockfile(log_file);
    fputs("C: ", log_file);
    dump_any_call(&call->generic, 0, log_file);
    if ((error = get_call_error(call)))
    {
        fputs("E: ", log_file);
        dump_GLerror(error, log_file);
        fputs("\n", log_file);
    }
    if (log_flush) fflush(log_file);
    funlockfile(log_file);
    return true;
}

static bool initialise_log(filter_set *handle)
{
    if (log_filename)
        log_file = fopen(log_filename, "w");
    else
        log_file = stderr;
    if (!log_file)
    {
        if (log_filename)
            fprintf(stderr, "failed to open log file %s\n", log_filename);
        return false;
    }
    register_filter(handle, "log", log_callback);
    register_filter_depends("log", "invoke");
    /* No direct rendering, but some of the length functions query state */
    filter_set_renders("log");
    filter_post_renders("log");
    filter_set_queries_error("log", false);
    return true;
}

static void destroy_log(filter_set *handle)
{
    if (log_filename)
    {
        if (log_file) fclose(log_file);
        free(log_filename);
    }
}

static bool set_variable_log(filter_set *handle, const char *name, const char *value)
{
    if (strcmp(name, "filename") == 0)
    {
        if (log_filename) free(log_filename);
        log_filename = xstrdup(value);
    }
    else if (strcmp(name, "flush") == 0)
    {
        if (strcmp(value, "yes") == 0)
            log_flush = true;
        else if (strcmp(value, "no") == 0)
            log_flush = false;
        else
            fprintf(stderr, "illegal flush value '%s'\n", value);
    }
    else
        return false;
    return true;
}


void initialise_filter_library(void)
{
    const filter_set_info log_info =
    {
        "log",
        initialise_log,
        destroy_log,
        set_variable_log,
        0,
        0
    };
    register_filter_set(&log_info);
}
