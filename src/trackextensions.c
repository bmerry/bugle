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
    const char *exts;
    const char *cur;
    bool *flags;
    int i;
    size_t len;

    flags = (bool *) data;
    memset(flags, 0, sizeof(bool) * BUGLE_GLEXT_COUNT);
    exts = CALL_glGetString(GL_EXTENSIONS);
    for (i = 0; i < BUGLE_GLEXT_COUNT; i++)
    {
        cur = exts;
        len = strlen(bugle_glext_names[i]);
        while ((cur = strstr(cur, bugle_glext_names[i])) != NULL)
        {
            if ((cur == exts || cur[-1] == ' ')
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
                                                       sizeof(bool) * BUGLE_GLEXT_COUNT);
    return true;
}

bool bugle_gl_has_extension(int ext)
{
    bool *data;

    assert(0 <= ext && ext < BUGLE_GLEXT_COUNT);
    data = (bool *) bugle_object_get_current_data(&bugle_context_class, trackextensions_view);
    if (!data) return false;
    else return data[ext];
}

void trackextensions_initialise(void)
{
    const filter_set_info trackextensions_info =
    {
        "trackextensions",
        initialise_trackextensions,
        NULL,
        NULL,
        0
    };

    bugle_register_filter_set(&trackextensions_info);

    bugle_register_filter_set_renders("trackextensions");
}
