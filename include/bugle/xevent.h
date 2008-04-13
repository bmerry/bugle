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

#ifndef BUGLE_SRC_XEVENT_H
#define BUGLE_SRC_XEVENT_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdbool.h>

#if BUGLE_WIN_X11
# include <X11/Xlib.h>
# include <X11/keysym.h> /* Convenience for including files */
#else
typedef int KeySym;
typedef void XEvent;
#define NoSymbol ((KeySym) 0)
#endif

typedef struct
{
    KeySym keysym;
    unsigned int state;
    bool press;
} xevent_key;

/* Returns true if the key name was found, false otherwise. On false
 * return, the value of *key is unmodified. The press field is always
 * set to true.
 */
bool bugle_xevent_key_lookup(const char *name, xevent_key *key);

/* Convenience callback: sets a flag */
void bugle_xevent_key_callback_flag(const xevent_key *key, void *arg, XEvent *event);

/* Registers a key trap. If non-NULL, the wanted function should return
 * true if the key should be trapped on this occasion. The wanted function
 * may be called multiple times for the same keypress, so it should be
 * stateless and have no side effects. It must also ignore the 'press'
 * field, otherwise the app will see press/release mismatches.
 */
void bugle_xevent_key_callback(const xevent_key *key,
                               bool (*wanted)(const xevent_key *, void *, XEvent *),
                               void (*callback)(const xevent_key *, void *, XEvent *),
                               void *arg);

void bugle_xevent_grab_pointer(bool dga, void (*callback)(int, int, XEvent *));
void bugle_xevent_release_pointer(void);

void xevent_initialise(void);

#endif /* !BUGLE_SRC_XEVENT_H */
