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
#include <assert.h>
#include <bugle/bool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <bugle/glwin/glwin.h>
#include <bugle/glwin/trackcontext.h>
#include <bugle/gl/glutils.h>
#include <bugle/gl/glextensions.h>
#include <bugle/hashtable.h>
#include <bugle/filters.h>
#include <bugle/objects.h>
#include <bugle/apireflect.h>
#include <budgie/call.h>
#include "xalloc.h"

typedef struct
{
    bugle_bool *flags;
    hash_table names;
} context_extensions;

static object_view glextensions_view = 0;

/* Extracts all the words from str, and sets arbitrary non-NULL values in
 * the hash table for each token. It does NOT clear the hash table first.
 */
static void tokenise(const char *str, hash_table *tokens)
{
    const char *p, *q;
    char *tmp;

    tmp = xcharalloc(strlen(str) + 1);
    p = str;
    while (*p == ' ') p++;
    while (*p != '\0')
    {
        q = p;
        while (*q != ' ' && *q != '\0')
            q++;
        memcpy(tmp, p, q - p);
        tmp[q - p] = '\0';
        bugle_hash_set(tokens, tmp, tokens);
        p = q;
        while (*p == ' ') p++;
    }
    free(tmp);
}

static void context_init(const void *key, void *data)
{
    context_extensions *ce;
    const char *glver, *glexts, *glwinexts = NULL;
    int glwin_major = 0, glwin_minor = 0;
    bugle_api_extension i;
    glwin_display dpy;

    ce = (context_extensions *) data;
    ce->flags = XCALLOC(bugle_api_extension_count(), bugle_bool);
    bugle_hash_init(&ce->names, NULL);

    glexts = (const char *) CALL(glGetString)(GL_EXTENSIONS);
    glver = (const char *) CALL(glGetString)(GL_VERSION);
    dpy = bugle_glwin_get_current_display();
    if (dpy)
    {
        bugle_glwin_query_version(dpy, &glwin_major, &glwin_minor);
        glwinexts = bugle_glwin_query_extensions_string(dpy);
    }

    tokenise(glexts, &ce->names);
    if (glwinexts) tokenise(glwinexts, &ce->names);
    for (i = 0; i < bugle_api_extension_count(); i++)
    {
        const char *name, *ver;
        name = bugle_api_extension_name(i);
        ver = bugle_api_extension_version(i);
        if (ver)
        {
            if (bugle_api_extension_block(i) != BUGLE_API_EXTENSION_BLOCK_GLWIN)
                ce->flags[i] = strcmp(glver, ver) >= 0;
            else if (dpy)
            {
                int major = 0, minor = 0;
                sscanf(ver, "%d.%d", &major, &minor);
                ce->flags[i] = glwin_major > major
                    || (glwin_major == major && glwin_minor >= minor);
            }
            if (ce->flags[i])
                bugle_hash_set(&ce->names, name, ce); /* arbitrary non-NULL */
        }
        else
            ce->flags[i] = bugle_hash_count(&ce->names, name);
    }
}

static void context_clear(void *data)
{
    context_extensions *ce;

    ce = (context_extensions *) data;
    free(ce->flags);
    bugle_hash_clear(&ce->names);
}

static bugle_bool glextensions_filter_set_initialise(filter_set *handle)
{
    glextensions_view = bugle_object_view_new(bugle_context_class,
                                              context_init,
                                              context_clear,
                                              sizeof(context_extensions));
    return BUGLE_TRUE;
}

/* The output can be inverted by passing ~ext instead of ext (which basically
 * means "true if this extension is not present"). This is used in the
 * state tables.
 */
bugle_bool bugle_gl_has_extension(bugle_api_extension ext)
{
    const context_extensions *ce;

    /* bugle_api_extension_id returns -1 for unknown extensions - play it safe */
    if (ext == NULL_EXTENSION) return BUGLE_FALSE;
    if (ext < 0) return !bugle_gl_has_extension(~ext);
    assert(ext < bugle_api_extension_count());
    ce = (const context_extensions *) bugle_object_get_current_data(bugle_context_class, glextensions_view);
    if (!ce) return BUGLE_FALSE;
    else return ce->flags[ext];
}

bugle_bool bugle_gl_has_extension2(int ext, const char *name)
{
    const context_extensions *ce;

    assert(ext >= -1 && ext < bugle_api_extension_count());
    /* bugle_api_extension_id returns -1 for unknown extensions - play it safe */
    ce = (const context_extensions *) bugle_object_get_current_data(bugle_context_class, glextensions_view);
    if (!ce) return BUGLE_FALSE;
    if (ext >= 0)
        return ce->flags[ext];
    else
        return bugle_hash_count(&ce->names, name);
}

/* The output can be inverted by passing ~ext instead of ext (which basically
 * means "true if none of these extensions are present").
 */
bugle_bool bugle_gl_has_extension_group(bugle_api_extension ext)
{
    const context_extensions *ce;
    size_t i;
    const bugle_api_extension *exts;

    if (ext < 0) return !bugle_gl_has_extension_group(~ext);
    assert(ext < bugle_api_extension_count());
    ce = (const context_extensions *) bugle_object_get_current_data(bugle_context_class, glextensions_view);
    if (!ce) return BUGLE_FALSE;
    exts = bugle_api_extension_group_members(ext);

    for (i = 0; exts[i] != NULL_EXTENSION; i++)
        if (ce->flags[exts[i]]) return BUGLE_TRUE;
    return BUGLE_FALSE;
}

bugle_bool bugle_gl_has_extension_group2(bugle_api_extension ext, const char *name)
{
    return (ext == NULL_EXTENSION)
        ? bugle_gl_has_extension2(ext, name) : bugle_gl_has_extension_group(ext);
}

void glextensions_initialise(void)
{
    static const filter_set_info glextensions_info =
    {
        "glextensions",
        glextensions_filter_set_initialise,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL /* No documentation */
    };

    bugle_filter_set_new(&glextensions_info);

    bugle_gl_filter_set_renders("glextensions");
}
