/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2006  Bruce Merry
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
#include "src/tracker.h"
#include "src/filters.h"
#include "src/objects.h"
#include "src/glexts.h"
#include "src/glutils.h"
#include "common/bool.h"
#include <string.h>
#include <assert.h>

static bugle_object_view trackextensions_view = 0;

static void context_initialise(const void *key, void *data)
{
    const char *glver, *glexts;
    const char *cur;
    bool *flags;
    int i;
    size_t len;

    flags = (bool *) data;
    memset(flags, 0, sizeof(bool) * BUGLE_EXT_COUNT);
    glexts = (const char *) CALL_glGetString(GL_EXTENSIONS);
    glver = (const char *) CALL_glGetString(GL_VERSION);
    for (i = 0; i < BUGLE_EXT_COUNT; i++)
        if (bugle_exts[i].gl_string)
            flags[i] = strcmp(glver, bugle_exts[i].gl_string) >= 0;
        else if (bugle_exts[i].glext_string)
        {
            cur = glexts;
            len = strlen(bugle_exts[i].glext_string);
            while ((cur = strstr(cur, bugle_exts[i].glext_string)) != NULL)
            {
                if ((cur == glexts || cur[-1] == ' ')
                    && (cur[len] == ' ' || cur[len] == '\0'))
                {
                    flags[i] = true;
                    break;
                }
                else
                    cur += len;
            }
        }
}

static bool initialise_trackextensions(filter_set *handle)
{
    trackextensions_view = bugle_object_class_register(&bugle_context_class,
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
    data = (const bool *) bugle_object_get_current_data(&bugle_context_class, trackextensions_view);
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
    data = (const bool *) bugle_object_get_current_data(&bugle_context_class, trackextensions_view);
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
        initialise_trackextensions,
        NULL,
        NULL,
        NULL,
        NULL,
        0,
        NULL /* No documentation */
    };

    bugle_register_filter_set(&trackextensions_info);

    bugle_register_filter_set_renders("trackextensions");
}
