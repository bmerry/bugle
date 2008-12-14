/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2008  Bruce Merry
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <budgie/reflect.h>
#include <budgie/addresses.h>
#include <bugle/glwin/glwin.h>
#include <bugle/apireflect.h>
#include <bugle/filters.h>
#include <bugle/log.h>
#include "xalloc.h"

static FILE *out;
static int frame = 0;
static bool outside = true;
static char *exe_filename = NULL;

#define MAX_ARGS 32 /* Up this if necessary, but I hope not... */

/* Outputs a definition for the data in the pointer, and returns its number.
 * Also updated defn_pool to avoid name collisions. The type is the base
 * type, not the type of the pointer
 */
static int follow_pointer(budgie_type type, int length, const void *ptr,
                          int *defn_pool)
{
    int i;
    size_t size;
    int *arg_ids;

    size = budgie_type_size(type);
    arg_ids = XNMALLOC(length, int);
    /* First follow any sub-pointers */
    for (i = 0; i < length; i++)
    {
        const void *cur;
        budgie_type cur_type, base;

        cur = (const void *) (i * budgie_type_size(type) + (const char *) ptr);
        cur_type = budgie_type_type(type, cur);
        base = budgie_type_pointer_base(cur_type);
        if (base != NULL_TYPE)
        {
            int cur_length;
            const void * const * cur_ptr;

            cur_ptr = (const void * const *) cur;
            cur_length = abs(budgie_type_length(base, cur_ptr));
            arg_ids[i] = follow_pointer(base, cur_length, cur_ptr, defn_pool);
        }
        else
            arg_ids[i] = -1;
    }
    /* Now output ourself
     * Note: the __typeof is because not all definitions can be formed
     * simply as "<type> <identifier>" e.g. function pointers.
     */
    fprintf(out, "        const __typeof(%s) defn%d[] = { ",
            budgie_type_name_nomangle(type), *defn_pool);
    for (i = 0; i < length; i++)
    {
        const void *cur;
        budgie_type cur_type;

        if (i) fprintf(out, ", ");
        cur = (const void *) (i * budgie_type_size(type) + (const char *) ptr);
        cur_type = budgie_type_type(type, cur);
        if (arg_ids[i] != -1)
            fprintf(out, "defn%d", arg_ids[i]);
        else
        {
            char *buffer, *buffer_ptr;
            size_t buffer_size;
            /* Obtain size */
            buffer = NULL;
            buffer_ptr = buffer;
            buffer_size = 0;
            budgie_dump_any_type(cur_type, cur, -1, &buffer_ptr, &buffer_size);
            buffer_size = buffer_ptr - buffer + 1;
            /* Fill in */
            buffer_ptr = buffer = xcharalloc(buffer_size);
            budgie_dump_any_type(cur_type, cur, -1, &buffer_ptr, &buffer_size);
            fputs(buffer, out);
            free(buffer);
        }
    }
    free(arg_ids);
    fprintf(out, " };\n");
    return *defn_pool++;
}

static bool exe_glwin_swap_buffers(function_call *call, const callback_data *data)
{
    if (!outside)
    {
        fprintf(out, "}\n");
        outside = true;
    }
    return true;
}

static bool exe_callback(function_call *call, const callback_data *data)
{
    int i, defn_pool = 0;
    int arg_ids[MAX_ARGS];
    bool block = false;

    if (bugle_api_extension_block(bugle_api_function_extension(call->generic.id)) == BUGLE_API_EXTENSION_BLOCK_GLWIN)
        return true;

    if (outside)
    {
        fprintf(out, "static void frame%d(void)\n{\n", frame);
        frame++;
        outside = false;
    }
    /* Generate any pointer data first */
    for (i = 0; i < call->generic.num_args; i++)
    {
        budgie_type type, base;
        int length;
        type = budgie_call_parameter_type(&call->generic, i);
        base = budgie_type_pointer_base(type);
        if (base != NULL_TYPE)
        {
            if (!block)
            {
                fprintf(out, "    {\n");
                block = true;
            }
            length = abs(budgie_call_parameter_length(&call->generic, i));
            arg_ids[i] = follow_pointer(base, length, *(void **) call->generic.args[i], &defn_pool);
        }
        else
            arg_ids[i] = -1;
    }
    /* Generate the function call */
    if (block) fputs("    ", out);
    fprintf(out, "    %s(", budgie_function_name(call->generic.id));
    for (i = 0; i < call->generic.num_args; i++)
    {
        if (i) fprintf(out, ", ");
        if (arg_ids[i] != -1)
            fprintf(out, "defn%d", arg_ids[i]);
        else
        {
            char *buffer, *buffer_ptr;
            size_t buffer_size;
            /* Obtain size */
            buffer = NULL;
            buffer_ptr = buffer;
            buffer_size = 0;
            budgie_call_parameter_dump(&call->generic, i, &buffer_ptr, &buffer_size);
            buffer_size = buffer_ptr - buffer + 1;
            /* Fill in */
            buffer_ptr = buffer = xcharalloc(buffer_size);
            budgie_call_parameter_dump(&call->generic, i, &buffer_ptr, &buffer_size);
            fputs(buffer, out);
            free(buffer);
        }
    }
    fputs(");\n", out);
    if (block) fputs("    }\n", out);
    return true;
}

static bool exe_initialise(filter_set *handle)
{
    filter *f;

    f = bugle_filter_new(handle, "exe");
    bugle_filter_catches_all(f, false, exe_callback);
    bugle_glwin_filter_catches_swap_buffers(f, false, exe_glwin_swap_buffers);
    bugle_filter_order("invoke", "exe");

    out = fopen(exe_filename, "w");
    if (!out)
    {
        bugle_log_printf("exe", "initialise", BUGLE_LOG_ERROR,
                         "cannot open %s for writing: %s", exe_filename, strerror(errno));
        return false;
    }
    fputs("#include <stdlib.h>\n"
          "#include <string.h>\n"
          "#include <GL/glew.h>\n"
          "#include <GL/glut.h>\n"
          "\n",
          out);
    return true;
}

static void exe_shutdown(filter_set *handle)
{
    int i;

    if (!outside)
        fprintf(out, "}\n");
    fprintf(out, "\nstatic void (*frames[]) =\n{\n");
    for (i = 0; i < frame; i++)
        fprintf(out, "    frame%d,\n", i);
    fprintf(out, "};\n");
    fclose(out);
    free(exe_filename);
}

void bugle_initialise_filter_library(void)
{
    static const filter_set_variable_info exe_variables[] =
    {
        { "filename", "filename of the C file to write [exetrace.c]", FILTER_SET_VARIABLE_STRING, &exe_filename, NULL },
        { NULL, NULL, 0, NULL, NULL }
    };

    static const filter_set_info info =
    {
        "exe",
        exe_initialise,
        exe_shutdown,
        NULL,
        NULL,
        exe_variables,
        "generates code to reproduce the original command stream"
    };

    bugle_filter_set_new(&info);
    exe_filename = xstrdup("exetrace.c");
}
