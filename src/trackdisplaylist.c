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
#include "src/canon.h"
#include "src/objects.h"
#include "src/tracker.h"
#include "src/glutils.h"
#include "common/bool.h"
#include "common/hashtable.h"
#include <stddef.h>
#include <GL/gl.h>
#if HAVE_PTHREAD_H
# include <pthread.h>
#endif

object_class bugle_displaylist_class;
static bugle_hashptr_table displaylist_objects;
static pthread_mutex_t displaylist_lock = PTHREAD_MUTEX_INITIALIZER;
static size_t displaylist_offset;

typedef struct
{
    GLuint list;
    GLenum mode;
} displaylist_struct;

static void initialise_displaylist_struct(const void *key, void *data)
{
    memcpy(data, key, sizeof(displaylist_struct));
}

GLenum bugle_displaylist_mode(void)
{
    displaylist_struct *info;

    info = bugle_object_get_current_data(&bugle_displaylist_class, displaylist_offset);
    if (!info) return GL_NONE;
    else return info->mode;
}

GLuint bugle_displaylist_list(void)
{
    displaylist_struct *info;

    info = bugle_object_get_current_data(&bugle_displaylist_class, displaylist_offset);
    if (!info) return 0;
    else return info->list;
}

void *bugle_displaylist_get(GLuint list)
{
    void *ans;

    pthread_mutex_lock(&displaylist_lock);
    ans = bugle_hashptr_get(&displaylist_objects, (void *) (size_t) list);
    pthread_mutex_unlock(&displaylist_lock);
    return ans;
}

static bool trackdisplaylist_callback(function_call *call, const callback_data *data)
{
    void *obj;
    displaylist_struct info, *info_ptr;
    GLint value;

    switch (bugle_canonical_call(call))
    {
    case FUNC_glNewList:
        if (bugle_displaylist_list()) break; /* Nested call */
        if (bugle_begin_internal_render())
        {
            CALL_glGetIntegerv(GL_LIST_INDEX, &value);
            info.list = value;
            CALL_glGetIntegerv(GL_LIST_MODE, &value);
            info.mode = value;
            if (info.list == 0)
                break;
            obj = bugle_object_new(&bugle_displaylist_class, &info, true);
            bugle_end_internal_render("trackdisplaylist_callback", true);
        }
        break;
    case FUNC_glEndList:
        obj = bugle_object_get_current(&bugle_displaylist_class);
        info_ptr = bugle_object_get_data(&bugle_displaylist_class, obj, displaylist_offset);
        /* Note: we update the hash table when we end the list, since this is
         * when the list is ended, since this is when OpenGL says the new
         * name comes into effect.
         */
        pthread_mutex_lock(&displaylist_lock);
        bugle_hashptr_set(&displaylist_objects, (void *) (size_t) info_ptr->list, obj);
        pthread_mutex_unlock(&displaylist_lock);
        bugle_object_set_current(&bugle_displaylist_class, NULL);
        break;
    }
    return true;
}

static bool initialise_trackdisplaylist(filter_set *handle)
{
    filter *f;

    f = bugle_register_filter(handle, "trackdisplaylist", trackdisplaylist_callback);
    bugle_register_filter_depends("trackdisplaylist", "invoke");
    bugle_register_filter_catches(f, FUNC_glNewList);
    bugle_register_filter_catches(f, FUNC_glEndList);

    displaylist_offset = bugle_object_class_register(&bugle_displaylist_class,
                                                     initialise_displaylist_struct,
                                                     NULL,
                                                     sizeof(displaylist_struct));
    return true;
}

void trackdisplaylist_initialise(void)
{
    const filter_set_info trackdisplaylist_info =
    {
        "trackdisplaylist",
        initialise_trackdisplaylist,
        NULL,
        NULL,
        0
    };

    bugle_object_class_init(&bugle_displaylist_class, &bugle_context_class);
    bugle_hashptr_init(&displaylist_objects);
    bugle_register_filter_set(&trackdisplaylist_info);

    bugle_register_filter_set_depends("trackdisplaylist", "trackcontext");
}
