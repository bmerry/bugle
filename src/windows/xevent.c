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
#include <stdbool.h>
#include <ltdl.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <bugle/linkedlist.h>
#include <bugle/filters.h>
#include <bugle/xevent.h>
#include <bugle/log.h>
#include "xalloc.h"

void bugle_xevent_key_callback(const xevent_key *key,
                               bool (*wanted)(const xevent_key *, void *, XEvent *),
                               void (*callback)(const xevent_key *, void *, XEvent *),
                               void *arg)
{
}

void bugle_xevent_grab_pointer(bool dga, void (*callback)(int, int, XEvent *))
{
}

void bugle_xevent_release_pointer(void)
{
}

void bugle_xevent_invalidate_window(XEvent *event)
{
}

void xevent_initialise(void)
{
}

bool bugle_xevent_key_lookup(const char *name, xevent_key *key)
{
    return false;
}

void bugle_xevent_key_callback_flag(const xevent_key *key, void *arg, XEvent *event)
{
    *(bool *) arg = true;
}
