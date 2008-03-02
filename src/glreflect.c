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
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <bugle/hashtable.h>
#include <bugle/glreflect.h>
#include <budgie/reflect.h>
#include "src/gltables.h"
#include "lock.h"

static hash_table ext_map;
gl_once_define(static, ext_map_once)

static void bugle_gl_extension_clear(void)
{
    bugle_hash_clear(&ext_map);
}

static void bugle_gl_extension_init(void)
{
    bugle_gl_extension i;
    bugle_hash_init(&ext_map, false);
    for (i = 0; i < BUGLE_GL_EXTENSION_COUNT; i++)
        bugle_hash_set(&ext_map, _bugle_gl_extension_table[i].name, (void *) (size_t) (i + 1));
    atexit(bugle_gl_extension_clear);
}

int bugle_gl_extension_count(void)
{
    return BUGLE_GL_EXTENSION_COUNT;
}

const char *bugle_gl_extension_name(bugle_gl_extension ext)
{
    assert(ext >= 0 && ext < BUGLE_GL_EXTENSION_COUNT);
    return _bugle_gl_extension_table[ext].name;
}

const char *bugle_gl_extension_version(bugle_gl_extension ext)
{
    assert(ext >= 0 && ext < BUGLE_GL_EXTENSION_COUNT);
    return _bugle_gl_extension_table[ext].version;
}

bool bugle_gl_extension_is_glx(bugle_gl_extension ext)
{
    assert(ext >= 0 && ext < BUGLE_GL_EXTENSION_COUNT);
    return strncmp(_bugle_gl_extension_table[ext].name, "GLX_", 4) == 0;
}

bugle_gl_extension bugle_gl_extension_id(const char *name)
{
    gl_once(ext_map_once, bugle_gl_extension_init);
    return (bugle_gl_extension) (size_t) bugle_hash_get(&ext_map, name) - 1;
}

const bugle_gl_extension *bugle_gl_extension_group_members(bugle_gl_extension ext)
{
    assert(ext >= 0 && ext < BUGLE_GL_EXTENSION_COUNT);
    return _bugle_gl_extension_table[ext].group;
}

/*** Tokens ***/

static const bugle_gl_enum_data *bugle_gl_enum_get(GLenum e)
{
    int l, r, m;

    l = 0;
    r = BUGLE_GL_ENUM_COUNT;
    while (l + 1 < r)
    {
        m = (l + r) / 2;
        if (e < _bugle_gl_enum_table[m].value) r = m;
        else l = m;
    }
    if (_bugle_gl_enum_table[l].value != e)
        return NULL;
    else
        return &_bugle_gl_enum_table[l];
}

const char *bugle_gl_enum_name(GLenum e)
{
    const bugle_gl_enum_data *t;

    t = bugle_gl_enum_get(e);
    if (t) return t->name; else return NULL;
}

const bugle_gl_extension *bugle_gl_enum_extensions(GLenum e)
{
    const bugle_gl_enum_data *t;
    static const bugle_gl_extension dummy[1] = {NULL_EXTENSION};

    t = bugle_gl_enum_get(e);
    if (t) return t->extensions;
    else return dummy;
}

/*** Functions ***/

bugle_gl_extension bugle_gl_function_extension(budgie_function id)
{
    assert(id >= 0 && id < budgie_function_count());
    return _bugle_gl_function_table[id].extension;
}
