/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2006, 2008-2009  Bruce Merry
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

#ifndef BUGLE_SRC_INPUT_H
#define BUGLE_SRC_INPUT_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdbool.h>
#include <bugle/porting.h>
#include <bugle/export.h>

/* These are chosen to match the X definitions for convenience */
#define BUGLE_INPUT_SHIFT_BIT   1
#define BUGLE_INPUT_CONTROL_BIT 4
#define BUGLE_INPUT_ALT_BIT     8

#if BUGLE_WINSYS_X11

#include <X11/Xlib.h>
#include <X11/keysym.h> /* Convenience for including files */

typedef KeySym bugle_input_keysym;
typedef XEvent bugle_input_event;
typedef Window bugle_input_window;

#define BUGLE_INPUT_NOSYMBOL NoSymbol

#elif BUGLE_WINSYS_WINDOWS

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
typedef SHORT bugle_input_keysym;
typedef MSG bugle_input_event;
typedef HWND bugle_input_window;
#define BUGLE_INPUT_NOSYMBOL ((bugle_input_keysym) 0)

#elif BUGLE_WINSYS_NONE

typedef int bugle_input_keysym;
typedef int bugle_input_event;
typedef int bugle_input_window;

#define BUGLE_INPUT_NOSYMBOL ((bugle_input_keysym) 0)

#else
#error "Unknown window system - please set one of the BUGLE_WINSYS_* macros"
#endif /* BUGLE_WINSYS_? */

typedef struct
{
    bugle_input_keysym keysym;
    unsigned char state;
    bool press;
} bugle_input_key;

/* Returns true if the key name was found, false otherwise. On false
 * return, the value of *key is unmodified. The press field is always
 * set to true.
 */
BUGLE_EXPORT_PRE bool bugle_input_key_lookup(const char *name, bugle_input_key *key) BUGLE_EXPORT_POST;

/* Convenience callback: sets a flag */
BUGLE_EXPORT_PRE void bugle_input_key_callback_flag(const bugle_input_key *key, void *arg, bugle_input_event *event) BUGLE_EXPORT_POST;

/* Registers a key trap. If non-NULL, the wanted function should return
 * true if the key should be trapped on this occasion. The wanted function
 * may be called multiple times for the same keypress, so it should be
 * stateless and have no side effects. It must also ignore the 'press'
 * field, otherwise the app will see press/release mismatches.
 */
BUGLE_EXPORT_PRE void bugle_input_key_callback(const bugle_input_key *key,
                                               bool (*wanted)(const bugle_input_key *, void *, bugle_input_event *),
                                               void (*callback)(const bugle_input_key *, void *, bugle_input_event *),
                                               void *arg) BUGLE_EXPORT_POST;

BUGLE_EXPORT_PRE void bugle_input_grab_pointer(bool dga, void (*callback)(int, int, bugle_input_event *)) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void bugle_input_release_pointer(void) BUGLE_EXPORT_POST;

/* Sends an invalidation event to the window referenced by the event */
BUGLE_EXPORT_PRE void bugle_input_invalidate_window(bugle_input_event *event) BUGLE_EXPORT_POST;

void input_initialise(void);

#endif /* !BUGLE_SRC_INPUT_H */
