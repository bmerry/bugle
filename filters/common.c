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
#include "src/types.h"
#include "src/safemem.h"
#include "common/bool.h"
#include <stdio.h>

/* Invoke filter-set */

static char *log_filename = NULL;
static bool log_flush = false;

static bool invoke_callback(function_call *call, void *data)
{
    invoke(call);
    return true;
}

static bool initialise_invoke(filter_set *handle)
{
    register_filter(handle, "invoke", invoke_callback);
    return true;
}

/* Log filter-set */

static FILE *log_file;

static bool log_callback(function_call *call, void *data)
{
    dump_any_call(&call->generic, 0, log_file);
    if (log_flush) fflush(log_file);
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
        log_filename = xstrdup(value);
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

/* General */

void initialise_filter_library(void)
{
    register_filter_set("invoke", initialise_invoke, NULL, NULL);
    register_filter_set("log", initialise_log, destroy_log, set_variable_log);
}
