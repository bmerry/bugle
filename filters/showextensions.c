/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2008  Bruce Merry
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
#define _XOPEN_SOURCE 500
#define GL_GLEXT_PROTOTYPES
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <math.h>
#include <sys/time.h>
#include <bugle/gl/glheaders.h>
#include <bugle/glwin/glwin.h>
#include <bugle/gl/glutils.h>
#include <bugle/hashtable.h>
#include <bugle/filters.h>
#include <bugle/apireflect.h>
#include <bugle/log.h>
#include <budgie/addresses.h>
#include <budgie/reflect.h>
#include "xalloc.h"

/* The values are set to &seen_extensions to indicate presence, then
 * lazily deleted by setting to NULL.
 *
 * FIXME-GLES: needs to be updated to support GL ES
 * FIXME: needs to be updated to support WGL
 */
static bool *seen_functions;
static hashptr_table seen_enums;
static const char *gl_version = "1.1";
static const char *glx_version = "1.2";

static bool showextensions_callback(function_call *call, const callback_data *data)
{
    int i;

    seen_functions[call->generic.id] = true;
    for (i = 0; i < budgie_group_parameter_count(call->generic.group); i++)
    {
        if (budgie_group_parameter_type(call->generic.group, i)
            == BUDGIE_TYPE_ID(6GLenum))
        {
            GLenum e;

            e = *(const GLenum *) call->generic.args[i];
            /* Set to arbitrary non-NULL value */
            bugle_hashptr_set(&seen_enums, (void *) (size_t) e, &seen_enums);
        }
    }
    return true;
}

static bool showextensions_initialise(filter_set *handle)
{
    filter *f;

    f = bugle_filter_new(handle, "showextensions");
    bugle_filter_catches_all(f, false, showextensions_callback);
    /* The order mainly doesn't matter, but making it a pre-filter
     * reduces the risk of another filter aborting the call.
     */
    bugle_filter_order("showextensions", "invoke");

    seen_functions = XCALLOC(budgie_function_count(), bool);
    bugle_hashptr_init(&seen_enums, NULL);
    return true;
}

static bool has_extension(const bugle_api_extension *list, bugle_api_extension test)
{
    size_t i;
    for (i = 0; list[i] != NULL_EXTENSION; i++)
        if (list[i] == test)
            return true;
    return false;
}

/* Clears the seen flags for everything covered by this extension, so that
 * we don't try to find a different extension to cover it.
 */
static void mark_extension(bugle_api_extension ext, bool *marked_extensions)
{
    budgie_function f;
    const hashptr_table_entry *e;
    const bugle_api_extension *exts;

    if (marked_extensions[ext]) return;
    marked_extensions[ext] = true;

    for (f = 0; f < budgie_function_count(); f++)
        if (seen_functions[f])
        {
            if (bugle_api_function_extension(f) == ext)
                seen_functions[f] = false;
        }
    for (e = bugle_hashptr_begin(&seen_enums); e; e = bugle_hashptr_next(&seen_enums, e))
        if (e->value)
        {
            exts = bugle_api_enum_extensions((GLenum) (size_t) e->key);
            if (has_extension(exts, ext))
                bugle_hashptr_set(&seen_enums, e->key, NULL);
        }
    /* GL and GLX versions automatically include all previous versions of
     * the same extension, and the extension numbering puts them all in order.
     * We recursively mark off all prior extensions.
     */
    if (ext > 0
        && bugle_api_extension_version(ext) 
        && bugle_api_extension_version(ext - 1)
        && bugle_api_extension_block(ext) == bugle_api_extension_block(ext - 1))
        mark_extension(ext - 1, marked_extensions);
}

static void showextensions_print(void *marked, FILE *logf)
{
    bool *marked_extensions;
    bugle_api_extension i;

    fputs("Required extensions:", logf);
    marked_extensions = (bool *) marked;
    for (i = 0; i < bugle_api_extension_count(); i++)
        if (marked_extensions[i] && !bugle_api_extension_version(i))
        {
            fputc(' ', logf);
            fputs(bugle_api_extension_name(i), logf);
        }
}

static void showextensions_shutdown(filter_set *handle)
{
    int i;
    budgie_function f;
    const hashptr_table_entry *e;
    bool *marked_extensions;
    const bugle_api_extension *exts;
    bugle_api_extension best;

    marked_extensions = XCALLOC(bugle_api_extension_count(), bool);
    /* We assume GL 1.1 and GLX 1.2 */
    mark_extension(BUGLE_API_EXTENSION_ID(GL_VERSION_1_1), marked_extensions);
    mark_extension(BUGLE_API_EXTENSION_ID(GLX_VERSION_1_2), marked_extensions);
    /* Functions generally tell us about specific extensions, so do them
     * first.
     */
    for (f = 0; f < budgie_function_count(); f++)
        if (seen_functions[f])
            mark_extension(bugle_api_function_extension(f), marked_extensions);
    /* For enums, we try to list the highest-numbered extension first, since
     * that is the oldest and hence least-demanding version
     */
    for (e = bugle_hashptr_begin(&seen_enums); e; e = bugle_hashptr_next(&seen_enums, e))
        if (e->value)
        {
            exts = bugle_api_enum_extensions((GLenum) (size_t) e->key);
            best = NULL_EXTENSION;
            for (i = 0; exts[i] != NULL_EXTENSION; i++)
                if (exts[i] > best)
                    best = exts[i];
            mark_extension(best, marked_extensions);
        }

    /* Now identify the versions of GL and GLX. */
    for (best = 0; best < bugle_api_extension_count(); best++)
        if (marked_extensions[best])
        {
            const char *ver;
            ver = bugle_api_extension_version(best);
            if (ver)
            {
                /* FIXME: should be named glwin_version */
                if (bugle_api_extension_block(best) == BUGLE_API_EXTENSION_BLOCK_GLWIN)
                    glx_version = ver;
                else
                    gl_version = ver;
            }
        }

    bugle_log_printf("showextensions", "gl", BUGLE_LOG_INFO,
                     "Min GL version: %s", gl_version);
    bugle_log_printf("showextensions", "glx", BUGLE_LOG_INFO,
                     "Min GLX version: %s", glx_version);
    bugle_log_callback("showextensions", "ext", BUGLE_LOG_INFO,
                       showextensions_print, marked_extensions);

    free(marked_extensions);
    free(seen_functions);
    bugle_hashptr_clear(&seen_enums);
}

void bugle_initialise_filter_library(void)
{
    static const filter_set_info showextensions_info =
    {
        "showextensions",
        showextensions_initialise,
        showextensions_shutdown,
        NULL,
        NULL,
        NULL,
        "reports extensions used at program termination"
    };

    bugle_filter_set_new(&showextensions_info);
}
