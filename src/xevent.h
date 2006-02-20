/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2006  Bruce Merry
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

#ifndef BUGLE_SRC_XEVENT_H
#define BUGLE_SRC_XEVENT_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <X11/Xlib.h>
#include <X11/keysym.h> /* Convenience for including files */

typedef struct
{
    KeySym keysym;
    unsigned int state;
} xevent_key;

/* Returns true if the key name was found, false otherwise. On false
 * return, the value of *key is unmodified.
 */
bool bugle_xevent_key_lookup(const char *name, xevent_key *key);

/* Convenience callback: sets a flag */
void bugle_xevent_key_callback_flag(const xevent_key *key, void *arg);

/* Registers a key trap. If non-NULL, the wanted function should return
 * true if the key should be trapped on this occasion. The wanted function
 * may be called multiple times for the same keypress, so it should be
 * stateless and have no side effects.
 */
void bugle_register_xevent_key(const xevent_key *key,
                               bool (*wanted)(const xevent_key *, void *arg),
                               void (*callback)(const xevent_key *, void *arg),
                               void *arg);

void initialise_xevent(void);

#endif /* !BUGLE_SRC_XEVENT_H */
