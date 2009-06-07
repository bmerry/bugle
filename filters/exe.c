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
#include <bugle/bool.h>
#include <bugle/memory.h>
#include <bugle/string.h>
#include <bugle/io.h>
#include <budgie/reflect.h>
#include <budgie/addresses.h>
#include <bugle/glwin/glwin.h>
#include <bugle/apireflect.h>
#include <bugle/filters.h>
#include <bugle/log.h>

static bugle_io_writer *out;
static int frame = 0;
static bugle_bool outside = BUGLE_TRUE;
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
    arg_ids = BUGLE_NMALLOC(length, int);
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
    bugle_io_printf(out, "        __typeof(%s) defn%d[] = { ",
                    budgie_type_name_nomangle(type), *defn_pool);
    for (i = 0; i < length; i++)
    {
        const void *cur;
        budgie_type cur_type;

        if (i) bugle_io_puts(", ", out);
        cur = (const void *) (i * budgie_type_size(type) + (const char *) ptr);
        cur_type = budgie_type_type(type, cur);
        if (arg_ids[i] != -1)
            bugle_io_printf(out, "defn%d", arg_ids[i]);
        else
            budgie_dump_any_type(cur_type, cur, -1, out);
    }
    free(arg_ids);
    bugle_io_puts(" };\n", out);
    return *defn_pool++;
}

static bugle_bool exe_glwin_swap_buffers(function_call *call, const callback_data *data)
{
    if (!outside)
    {
        bugle_io_puts("}\n", out);
        outside = BUGLE_TRUE;
    }
    return BUGLE_TRUE;
}

static bugle_bool exe_callback(function_call *call, const callback_data *data)
{
    int i, defn_pool = 0;
    int arg_ids[MAX_ARGS];
    bugle_bool block = BUGLE_FALSE;

    if (bugle_api_extension_block(bugle_api_function_extension(call->generic.id)) == BUGLE_API_EXTENSION_BLOCK_GLWIN)
        return BUGLE_TRUE;

    if (outside)
    {
        bugle_io_printf(out, "static void frame%d(void)\n{\n", frame);
        frame++;
        outside = BUGLE_FALSE;
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
                bugle_io_puts("    {\n", out);
                block = BUGLE_TRUE;
            }
            length = abs(budgie_call_parameter_length(&call->generic, i));
            arg_ids[i] = follow_pointer(base, length, *(void **) call->generic.args[i], &defn_pool);
        }
        else
            arg_ids[i] = -1;
    }
    /* Generate the function call */
    if (block) bugle_io_puts("    ", out);
    bugle_io_printf(out, "    %s(", budgie_function_name(call->generic.id));
    for (i = 0; i < call->generic.num_args; i++)
    {
        if (i) bugle_io_puts(", ", out);
        if (arg_ids[i] != -1)
            bugle_io_printf(out, "defn%d", arg_ids[i]);
        else
            budgie_call_parameter_dump(&call->generic, i, out);
    }
    bugle_io_puts(");\n", out);
    if (block) bugle_io_puts("    }\n", out);
    return BUGLE_TRUE;
}

static bugle_bool exe_initialise(filter_set *handle)
{
    filter *f;
    FILE *out_file;

    f = bugle_filter_new(handle, "exe");
    bugle_filter_catches_all(f, BUGLE_FALSE, exe_callback);
    bugle_glwin_filter_catches_swap_buffers(f, BUGLE_FALSE, exe_glwin_swap_buffers);
    bugle_filter_order("invoke", "exe");

    out_file = fopen(exe_filename, "w");
    if (!out_file)
    {
        bugle_log_printf("exe", "initialise", BUGLE_LOG_ERROR,
                         "cannot open %s for writing: %s", exe_filename, strerror(errno));
        return BUGLE_FALSE;
    }
    out = bugle_io_writer_file_new(out_file);
    bugle_io_puts("#include <stdlib.h>\n"
                  "#include <string.h>\n"
#if BUGLE_GLTYPE_GL
                  "#include <GL/glew.h>\n"
                  "#include <GL/glut.h>\n"
#endif
#if BUGLE_GLTYPE_GLES1CM
                  "#include <GLES/gl.h>\n"
                  "#include <GLES/glext.h>\n"
#endif
#if BUGLE_GLTYPE_GLES2
                  "#include <GLES2/gl2.h>\n"
                  "#include <GLES2/gl2ext.h>\n"
#endif
                  "\n",
                  out);
    return BUGLE_TRUE;
}

static void exe_shutdown(filter_set *handle)
{
    int i;

    if (!outside)
        bugle_io_puts("}\n", out);
    bugle_io_printf(out, "\nint frame_count = %d;\n", frame);
    bugle_io_printf(out, "void (*frames[])(void) =\n{\n");
    for (i = 0; i < frame; i++)
        bugle_io_printf(out, "    frame%d,\n", i);
    bugle_io_printf(out, "};\n");
    bugle_io_writer_close(out);
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
    exe_filename = bugle_strdup("exetrace.c");
}
