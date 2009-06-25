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
#include <stdlib.h>
#include <bugle/bool.h>
#include <bugle/memory.h>
#include <string.h>
#include <bugle/glwin/glwin.h>
#include <bugle/glwin/trackcontext.h>
#include <bugle/gl/glheaders.h>
#include <bugle/gl/glutils.h>
#include <bugle/gl/glextensions.h>
#include <bugle/gl/gldisplaylist.h>
#include <bugle/hashtable.h>
#include <bugle/filters.h>
#include <bugle/objects.h>
#include <bugle/log.h>
#include <bugle/apireflect.h>
#include <budgie/reflect.h>
#include <budgie/addresses.h>

/*** extoverride extension ***/

/* The design is that extensions that we have never heard of may be
 * suppressed; hence we use the two hash-tables rather than a boolean array.
 */
static bugle_bool extoverride_disable_all = BUGLE_FALSE;
static char *extoverride_max_version = NULL;
static hash_table extoverride_disabled;
static hash_table extoverride_enabled;
static object_view extoverride_view;

typedef struct
{
    const GLubyte *version; /* Filtered version of glGetString(GL_VERSION) */
    GLubyte *extensions;    /* Filtered version of glGetString(GL_EXTENSIONS) */
} extoverride_context;

static bugle_bool extoverride_suppressed(const char *ext)
{
    if (extoverride_disable_all)
        return !bugle_hash_count(&extoverride_enabled, ext);
    else
        return bugle_hash_count(&extoverride_disabled, ext);
}

static void extoverride_context_init(const void *key, void *data)
{
    const char *real_version, *real_exts, *p, *q;
    char *exts, *ext, *exts_ptr;
    extoverride_context *self;

    real_version = (const char *) CALL(glGetString)(GL_VERSION);
    real_exts = (const char *) CALL(glGetString)(GL_EXTENSIONS);
    self = (extoverride_context *) data;

    if (extoverride_max_version && strcmp(real_version, extoverride_max_version) > 0)
        self->version = (const GLubyte *) extoverride_max_version;
    else
        self->version = (const GLubyte *) real_version;

    exts_ptr = exts = BUGLE_NMALLOC(strlen(real_exts) + 1, char);
    ext = BUGLE_NMALLOC(strlen(real_exts) + 1, char); /* Current extension */
    p = real_exts;
    while (*p == ' ') p++;
    while (*p)
    {
        q = p;
        while (*q != ' ' && *q != '\0') q++;
        memcpy(ext, p, q - p);
        ext[q - p] = '\0';
        if (!extoverride_suppressed(ext))
        {
            memcpy(exts_ptr, ext, q - p);
            exts_ptr += q - p;
            *exts_ptr++ = ' ';
        }
        p = q;
        while (*p == ' ') p++;
    }

    /* Back up over the trailing space */
    if (exts_ptr != exts) exts_ptr--;
    *exts_ptr = '\0';
    self->extensions = (GLubyte *) exts;
    free(ext);
}

static void extoverride_context_clear(void *data)
{
    extoverride_context *self;

    self = (extoverride_context *) data;
    /* self->version is always a shallow copy of another string */
    free(self->extensions);
}

static bugle_bool extoverride_glGetString(function_call *call, const callback_data *data)
{
    extoverride_context *ctx;

    ctx = (extoverride_context *) bugle_object_get_current_data(bugle_context_class, extoverride_view);
    if (!ctx) return BUGLE_TRUE; /* Nothing can be overridden */
    switch (*call->glGetString.arg0)
    {
    case GL_VERSION:
        *call->glGetString.retn = ctx->version;
        break;
    case GL_EXTENSIONS:
        *call->glGetString.retn = ctx->extensions;
        break;
    }
    return BUGLE_TRUE;
}

static bugle_bool extoverride_warn(function_call *call, const callback_data *data)
{
    bugle_log_printf("extoverride", "warn", BUGLE_LOG_NOTICE,
                     "%s was called, although the corresponding extension was suppressed",
                     budgie_function_name(call->generic.id));
    return BUGLE_TRUE;
}

static bugle_bool extoverride_initialise(filter_set *handle)
{
    filter *f;
    budgie_function i;

    f = bugle_filter_new(handle, "extoverride_get");
    bugle_filter_order("invoke", "extoverride_get");
    bugle_filter_order("extoverride_get", "trace");
    bugle_filter_catches(f, "glGetString", BUGLE_FALSE, extoverride_glGetString);

    f = bugle_filter_new(handle, "extoverride_warn");
    bugle_filter_order("extoverride_warn", "invoke");
    for (i = 0; i < budgie_function_count(); i++)
    {
        bugle_api_extension ext;
        const char *name, *version;

        ext = bugle_api_function_extension(i);
        version = bugle_api_extension_version(ext);
        name = bugle_api_extension_name(ext);
        if (!version
            && extoverride_suppressed(name))
            bugle_filter_catches_function_id(f, i, BUGLE_FALSE, extoverride_warn);
        else if (extoverride_max_version
                 && version
                 && bugle_api_extension_block(ext) == BUGLE_API_EXTENSION_BLOCK_GL
                 && strcmp(version, extoverride_max_version) > 1)
        {
            /* FIXME: the strcmp above will break if there is ever an OpenGL
             * version with multiple digits.
             */
            bugle_filter_catches_function_id(f, i, BUGLE_FALSE, extoverride_warn);
        }
    }

    extoverride_view = bugle_object_view_new(bugle_context_class,
                                             extoverride_context_init,
                                             extoverride_context_clear,
                                             sizeof(extoverride_context));
    return BUGLE_TRUE;
}

static void extoverride_shutdown(filter_set *handle)
{
    bugle_hash_clear(&extoverride_disabled);
    bugle_hash_clear(&extoverride_enabled);
    free(extoverride_max_version);
}

static bugle_bool extoverride_variable_disable(const struct filter_set_variable_info_s *var,
                                         const char *text, const void *value)
{
    bugle_api_extension i;

    if (strcmp(text, "all") == 0)
    {
        extoverride_disable_all = BUGLE_TRUE;
        return BUGLE_TRUE;
    }

    bugle_hash_set(&extoverride_disabled, text, NULL);
    i = bugle_api_extension_id(text);
    if (i == -1 || bugle_api_extension_version(i) != NULL)
        bugle_log_printf("extoverride", "disable", BUGLE_LOG_WARNING,
                         "Extension %s is unknown (typo?)", text);
    return BUGLE_TRUE;
}

static bugle_bool extoverride_variable_enable(const struct filter_set_variable_info_s *var,
                                        const char *text, const void *value)
{
    bugle_api_extension i;

    if (strcmp(text, "all") == 0)
    {
        extoverride_disable_all = BUGLE_FALSE;
        return BUGLE_TRUE;
    }

    bugle_hash_set(&extoverride_enabled, text, NULL);
    i = bugle_api_extension_id(text);
    if (i == -1 || bugle_api_extension_version(i) != NULL)
        bugle_log_printf("extoverride", "enable", BUGLE_LOG_WARNING,
                         "Extension %s is unknown (typo?)", text);
    return BUGLE_TRUE;
}

void bugle_initialise_filter_library(void)
{
    static const filter_set_variable_info extoverride_variables[] =
    {
        { "disable", "name of an extension to suppress, or 'all'", FILTER_SET_VARIABLE_CUSTOM, NULL, extoverride_variable_disable },
        { "enable", "name of an extension to enable, or 'all'", FILTER_SET_VARIABLE_CUSTOM, NULL, extoverride_variable_enable },
        { "version", "maximum OpenGL version to expose", FILTER_SET_VARIABLE_STRING, &extoverride_max_version, NULL },
        { NULL, NULL, 0, NULL, NULL }
    };

    static const filter_set_info extoverride_info =
    {
        "extoverride",
        extoverride_initialise,
        extoverride_shutdown,
        NULL,
        NULL,
        extoverride_variables,
        "suppresses extensions or OpenGL versions"
    };

    bugle_filter_set_new(&extoverride_info);

    bugle_filter_set_depends("extoverride", "glextensions");
}
