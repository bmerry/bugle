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
#include <bugle/tracker.h>
#include <bugle/filters.h>
#include <bugle/objects.h>
#include <bugle/glutils.h>
#include <budgie/call.h>
#include "src/glexts.h"
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <GL/glx.h>

static object_view trackextensions_view = 0;

static bool string_contains_extension(const char *exts, const char *ext)
{
    const char *cur;
    size_t len;

    cur = exts;
    len = strlen(ext);
    while ((cur = strstr(cur, ext)) != NULL)
    {
        if ((cur == exts || cur[-1] == ' ')
            && (cur[len] == ' ' || cur[len] == '\0'))
            return true;
        else
            cur += len;
    }
    return false;
}

static void context_initialise(const void *key, void *data)
{
    const char *glver, *glexts, *glxexts = NULL;
    int glx_major = 0, glx_minor = 0;
    bool *flags;
    int i;
    Display *dpy;

    flags = (bool *) data;
    memset(flags, 0, sizeof(bool) * BUGLE_EXT_COUNT);
    glexts = (const char *) CALL_glGetString(GL_EXTENSIONS);
    glver = (const char *) CALL_glGetString(GL_VERSION);
    /* Don't lock, because we're already inside a lock */
    dpy = bugle_get_current_display_internal(false);
    if (dpy)
    {
        CALL_glXQueryVersion(dpy, &glx_major, &glx_minor);
        glxexts = CALL_glXQueryExtensionsString(dpy, 0); /* FIXME: get screen number */
    }
    for (i = 0; i < BUGLE_EXT_COUNT; i++)
        if (bugle_exts[i].glx)
        {
            if (!dpy)
                continue;
            if (bugle_exts[i].gl_string)
            {
                int major = 0, minor = 0;
                sscanf(bugle_exts[i].gl_string, "%d.%d", &major, &minor);
                flags[i] = glx_major > major
                    || (glx_major == major && glx_minor >= minor);
            }
            else if (bugle_exts[i].glext_string)
                flags[i] = string_contains_extension(glxexts, bugle_exts[i].glext_string);
        }
        else
        {
            if (bugle_exts[i].gl_string)
                flags[i] = strcmp(glver, bugle_exts[i].gl_string) >= 0;
            else if (bugle_exts[i].glext_string)
                flags[i] = string_contains_extension(glexts, bugle_exts[i].glext_string);
        }
}

static bool trackextensions_filter_set_initialise(filter_set *handle)
{
    trackextensions_view = bugle_object_view_new(bugle_context_class,
                                                 context_initialise,
                                                 NULL,
                                                 sizeof(bool) * BUGLE_EXT_COUNT);
    return true;
}

/* The output can be inverted by passing ~ext instead of ext (which basically
 * means "true if this extension is not present"). This is used in the
 * state tables.
 */
bool bugle_gl_has_extension(int ext)
{
    const bool *data;

    if (ext < 0) return !bugle_gl_has_extension(~ext);
    assert(ext < BUGLE_EXT_COUNT);
    data = (const bool *) bugle_object_get_current_data(bugle_context_class, trackextensions_view);
    if (!data) return false;
    else return data[ext];
}

/* The output can be inverted by passing ~ext instead of ext (which basically
 * means "true if none of these extensions are present").
 */
bool bugle_gl_has_extension_group(int ext)
{
    size_t i;
    const bool *data;
    const int *exts;

    if (ext < 0) return !bugle_gl_has_extension_group(~ext);
    assert(ext <= BUGLE_EXT_COUNT);
    data = (const bool *) bugle_object_get_current_data(bugle_context_class, trackextensions_view);
    if (!data) return false;
    exts = bugle_extgroups[ext];

    for (i = 0; exts[i] != -1; i++)
        if (data[exts[i]]) return true;
    return false;
}

void trackextensions_initialise(void)
{
    static const filter_set_info trackextensions_info =
    {
        "trackextensions",
        trackextensions_filter_set_initialise,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL /* No documentation */
    };

    bugle_filter_set_new(&trackextensions_info);

    bugle_filter_set_renders("trackextensions");
}
