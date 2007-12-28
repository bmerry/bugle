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
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <bugle/hashtable.h>
#include <bugle/glreflect.h>
#include "src/glexts.h"
#include "lock.h"

static hash_table ext_map;
gl_once_define(static, ext_map_once);

static void bugle_gl_extension_clear(void)
{
    bugle_hash_clear(&ext_map);
}

static void bugle_gl_extension_init(void)
{
    bugle_gl_extension i;
    bugle_hash_init(&ext_map, false);
    for (i = 0; i < BUGLE_EXT_COUNT; i++)
        bugle_hash_set(&ext_map, bugle_exts[i].name, (void *) (size_t) (i + 1));
    atexit(bugle_gl_extension_clear);
}

int bugle_gl_extension_count(void)
{
    return BUGLE_EXT_COUNT;
}

const char *bugle_gl_extension_name(bugle_gl_extension ext)
{
    assert(ext >= 0 && ext < BUGLE_EXT_COUNT);
    return bugle_exts[ext].name;
}

const char *bugle_gl_extension_version(bugle_gl_extension ext)
{
    assert(ext >= 0 && ext < BUGLE_EXT_COUNT);
    return bugle_exts[ext].version;
}

bool bugle_gl_extension_is_glx(bugle_gl_extension ext)
{
    assert(ext >= 0 && ext < BUGLE_EXT_COUNT);
    return strncmp(bugle_exts[ext].name, "GLX_", 4) == 0;
}

bugle_gl_extension bugle_gl_extension_id(const char *name)
{
    gl_once(ext_map_once, bugle_gl_extension_init);
    return (bugle_gl_extension) (size_t) bugle_hash_get(&ext_map, name) - 1;
}

bugle_gl_extension *bugle_gl_extension_group_members(bugle_gl_extension ext)
{
    assert(ext >= 0 && ext < BUGLE_EXT_COUNT);
    return bugle_extgroups[ext];
}
