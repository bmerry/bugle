/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2010, 2014  Bruce Merry
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
#include <bugle/bool.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <bugle/porting.h>
#include <bugle/linkedlist.h>
#include <bugle/filters.h>
#include <bugle/input.h>
#include <bugle/log.h>
#include <bugle/memory.h>
#include "platform/threads.h"

#define STATE_MASK (BUGLE_INPUT_SHIFT_BIT | BUGLE_INPUT_CONTROL_BIT | BUGLE_INPUT_ALT_BIT)

typedef struct
{
    bugle_input_key key;
#if BUGLE_WINSYS_X11
    /* Event predicates only receive keycodes, so we have to cache those
     * here.
     */
    KeyCode keycode;
#endif
    void *arg;
    bugle_bool (*wanted)(const bugle_input_key *, void *, bugle_input_event *);
    void (*callback)(const bugle_input_key *, void *, bugle_input_event *);
} handler;

static bugle_bool active = BUGLE_FALSE;
static linked_list handlers;

static bugle_bool mouse_active = BUGLE_FALSE;
static bugle_bool mouse_first = BUGLE_TRUE;
static bugle_bool mouse_dga = BUGLE_FALSE;
static bugle_input_window mouse_window;
static int mouse_x, mouse_y;
static void (*mouse_callback)(int, int, bugle_input_event *) = NULL;

void bugle_input_key_callback(const bugle_input_key *key,
                               bugle_bool (*wanted)(const bugle_input_key *, void *, bugle_input_event *),
                               void (*callback)(const bugle_input_key *, void *, bugle_input_event *),
                               void *arg)
{
    handler *h;

    if (key->keysym == BUGLE_INPUT_NOSYMBOL) return;
    h = BUGLE_MALLOC(handler);
    h->key = *key;
    h->wanted = wanted;
    h->callback = callback;
    h->arg = arg;

    bugle_list_append(&handlers, h);
    active = BUGLE_TRUE;
}

void bugle_input_grab_pointer(bugle_bool dga, void (*callback)(int, int, bugle_input_event *))
{
    mouse_dga = dga;
    mouse_callback = callback;
    mouse_active = BUGLE_TRUE;
    mouse_first = BUGLE_TRUE;
    bugle_log("input", "mouse", BUGLE_LOG_DEBUG, "grabbed");
}

void bugle_input_release_pointer(void)
{
    mouse_active = BUGLE_FALSE;
    bugle_log("input", "mouse", BUGLE_LOG_DEBUG, "released");
}

void bugle_input_key_callback_flag(const bugle_input_key *key, void *arg, bugle_input_event *event)
{
    *(bugle_bool *) arg = BUGLE_TRUE;
}

#if BUGLE_HAVE_ATTRIBUTE_CONSTRUCTOR && !DEBUG_CONSTRUCTOR && BUGLE_BINFMT_CONSTRUCTOR_DL
# define bugle_initialise_all() ((void) 0)
#else
extern void bugle_initialise_all(void);
#endif

#if BUGLE_WINSYS_X11

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include "platform/dl.h"

/* Events we want to get, even if the app doesn't ask for them */
#define EVENT_MASK (KeyPressMask | KeyReleaseMask | PointerMotionMask)

/* Events we don't want to know about */
#define EVENT_UNMASK (PointerMotionHintMask)

/* Pointers to the real versions in libX11 */
static int (*real_XNextEvent)(Display *, XEvent *) = NULL;
static int (*real_XPeekEvent)(Display *, XEvent *) = NULL;
static int (*real_XWindowEvent)(Display *, Window, long, XEvent *) = NULL;
static Bool (*real_XCheckWindowEvent)(Display *, Window, long, XEvent *) = NULL;
static int (*real_XMaskEvent)(Display *, long, XEvent *) = NULL;
static Bool (*real_XCheckMaskEvent)(Display *, long, XEvent *) = NULL;
static Bool (*real_XCheckTypedEvent)(Display *, int, XEvent *) = NULL;
static Bool (*real_XCheckTypedWindowEvent)(Display *, Window, int, XEvent *) = NULL;

static int (*real_XIfEvent)(Display *, XEvent *, Bool (*)(Display *, XEvent *, XPointer), XPointer) = NULL;
static Bool (*real_XCheckIfEvent)(Display *, XEvent *, Bool (*)(Display *, XEvent *, XPointer), XPointer) = NULL;
static int (*real_XPeekIfEvent)(Display *, XEvent *, Bool (*)(Display *, XEvent *, XPointer), XPointer) = NULL;

static int (*real_XEventsQueued)(Display *, int) = NULL;
static int (*real_XPending)(Display *) = NULL;

static Window (*real_XCreateWindow)(Display *, Window, int, int, unsigned int, unsigned int, unsigned int, int, unsigned int, Visual *, unsigned long, XSetWindowAttributes *) = NULL;
static Window (*real_XCreateSimpleWindow)(Display *, Window, int, int, unsigned int, unsigned int, unsigned int, unsigned long, unsigned long) = NULL;
static int (*real_XSelectInput)(Display *, Window, long) = NULL;

/* Declare our versions for DLL export */
BUGLE_EXPORT_PRE int XNextEvent(Display *, XEvent *) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE int XPeekEvent(Display *, XEvent *) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE int XWindowEvent(Display *, Window, long, XEvent *) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE Bool XCheckWindowEvent(Display *, Window, long, XEvent *) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE int XMaskEvent(Display *, long, XEvent *) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE Bool XCheckMaskEvent(Display *, long, XEvent *) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE Bool XCheckTypedEvent(Display *, int, XEvent *) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE Bool XCheckTypedWindowEvent(Display *, Window, int, XEvent *) BUGLE_EXPORT_POST;

BUGLE_EXPORT_PRE int XIfEvent(Display *, XEvent *, Bool (*)(Display *, XEvent *, XPointer), XPointer) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE Bool XCheckIfEvent(Display *, XEvent *, Bool (*)(Display *, XEvent *, XPointer), XPointer) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE int XPeekIfEvent(Display *, XEvent *, Bool (*)(Display *, XEvent *, XPointer), XPointer) BUGLE_EXPORT_POST;

BUGLE_EXPORT_PRE int XEventsQueued(Display *, int) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE int XPending(Display *) BUGLE_EXPORT_POST;

BUGLE_EXPORT_PRE Window XCreateWindow(Display *, Window, int, int, unsigned int, unsigned int, unsigned int, int, unsigned int, Visual *, unsigned long, XSetWindowAttributes *) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE Window XCreateSimpleWindow(Display *, Window, int, int, unsigned int, unsigned int, unsigned int, unsigned long, unsigned long) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE int XSelectInput(Display *, Window, long) BUGLE_EXPORT_POST;

static bugle_thread_lock_t keycodes_lock;

/* Determines whether bugle wants to intercept an event */
static Bool event_predicate(Display *dpy, XEvent *event, XPointer arg)
{
    bugle_input_key key;
    linked_list_node *i;
    handler *h;

    if (mouse_active && event->type == MotionNotify) return True;
    if (event->type != KeyPress && event->type != KeyRelease) return False;
    key.state = event->xkey.state & STATE_MASK;
    key.press = event->type == KeyPress;
    for (i = bugle_list_head(&handlers); i; i = bugle_list_next(i))
    {
        h = (handler *) bugle_list_data(i);
        if (h->keycode == event->xkey.keycode
            && h->key.state == key.state
            && (!h->wanted || (*h->wanted)(&key, h->arg, event)))
            return True;
    }
    return False;
}

/* Note: assumes that event_predicate is True */
static void handle_event(Display *dpy, XEvent *event)
{
    bugle_input_key key;
    linked_list_node *i;
    handler *h;

    if (mouse_active && event->type == MotionNotify)
    {
        if (mouse_dga)
        {
            bugle_log_printf("input", "mouse", BUGLE_LOG_DEBUG,
                             "DGA mouse moved by (%d, %d)",
                             event->xmotion.x, event->xmotion.y);
            (*mouse_callback)(event->xmotion.x, event->xmotion.y, event);
        }
        else if (mouse_first)
        {
            XWindowAttributes attr;
            XGetWindowAttributes(dpy, event->xmotion.window, &attr);

            mouse_window = event->xmotion.window;
            mouse_x = attr.width / 2;
            mouse_y = attr.height / 2;
            XWarpPointer(dpy, event->xmotion.window, event->xmotion.window,
                         0, 0, 0, 0, mouse_x, mouse_y);
            XFlush(dpy); /* try to ensure that XWarpPointer gets to server before next motion */
            mouse_first = BUGLE_FALSE;
        }
        else if (event->xmotion.window != mouse_window)
            XWarpPointer(dpy, None, mouse_window, 0, 0, 0, 0, mouse_x, mouse_y);
        else if (mouse_x != event->xmotion.x || mouse_y != event->xmotion.y)
        {
            bugle_log_printf("input", "mouse", BUGLE_LOG_DEBUG,
                             "mouse moved by (%d, %d) ref = (%d, %d)",
                             event->xmotion.x - mouse_x, event->xmotion.y - mouse_y,
                             mouse_x, mouse_y);
            (*mouse_callback)(event->xmotion.x - mouse_x, event->xmotion.y - mouse_y, event);
            XWarpPointer(dpy, None, event->xmotion.window, 0, 0, 0, 0, mouse_x, mouse_y);
            XFlush(dpy); /* try to ensure that XWarpPointer gets to server before next motion */
        }
        return;
    }
    /* event_predicate returns True on release as well, so that the
     * release event does not appear to the app without the press event.
     * We only conditionally pass releases to the filterset.
     */
    if (event->type != KeyPress && event->type != KeyRelease) return;

    key.keysym = XkbKeycodeToKeysym(dpy, event->xkey.keycode, 0, 1);
    key.state = event->xkey.state & STATE_MASK;
    key.press = (event->type == KeyPress);
    for (i = bugle_list_head(&handlers); i; i = bugle_list_next(i))
    {
        h = (handler *) bugle_list_data(i);
        if (h->key.keysym == key.keysym && h->key.state == key.state
            && h->key.press == key.press
            && (!h->wanted || (*h->wanted)(&key, h->arg, event)))
            (*h->callback)(&key, h->arg, event);
    }
}

static void initialise_keycodes(Display *dpy)
{
    /* We have to generate keycodes outside of event_predicate, because
     * event_predicate may not call X functions.
     * FIXME: this will go wrong if two different threads connected to
     * different X servers try to use this at the same time.
     */
    static Display *dpy_cached = NULL;
    bugle_thread_lock_lock(&keycodes_lock);
    if (dpy_cached != dpy)
    {
        linked_list_node *i;
        handler *h;
        for (i = bugle_list_head(&handlers); i; i = bugle_list_next(i))
        {
            h = (handler *) bugle_list_data(i);
            h->keycode = XKeysymToKeycode(dpy, h->key.keysym);
        }
        dpy_cached = dpy;
    }
    bugle_thread_lock_unlock(&keycodes_lock);
}

static bugle_bool extract_events(Display *dpy)
{
    XEvent event;
    bugle_bool any = BUGLE_FALSE;

    initialise_keycodes(dpy);

    while ((*real_XCheckIfEvent)(dpy, &event, event_predicate, NULL))
    {
        handle_event(dpy, &event);
        any = BUGLE_TRUE;
    }
    return any;
}

int XNextEvent(Display *dpy, XEvent *event)
{
    int ret;

    bugle_initialise_all();
    if (!active) return (*real_XNextEvent)(dpy, event);
    bugle_log("input", "XNextEvent", BUGLE_LOG_DEBUG, "enter");
    extract_events(dpy);
    while ((ret = (*real_XNextEvent)(dpy, event)) != 0)
    {
        if (!event_predicate(dpy, event, NULL))
        {
            bugle_log("input", "XNextEvent", BUGLE_LOG_DEBUG, "exit");
            return ret;
        }
        handle_event(dpy, event);
    }
    bugle_log("input", "XNextEvent", BUGLE_LOG_DEBUG, "exit");
    return ret;
}

int XPeekEvent(Display *dpy, XEvent *event)
{
    int ret;

    bugle_initialise_all();
    if (!active) return (*real_XPeekEvent)(dpy, event);
    bugle_log("input", "XPeekEvent", BUGLE_LOG_DEBUG, "enter");
    extract_events(dpy);
    while ((ret = (*real_XPeekEvent)(dpy, event)) != 0)
    {
        if (!event_predicate(dpy, event, NULL))
        {
            bugle_log("input", "XPeekEvent", BUGLE_LOG_DEBUG, "exit");
            return ret;
        }
        /* Peek doesn't remove the event, so do another extract run to
         * clear it */
        extract_events(dpy);
    }
    bugle_log("input", "XPeekEvent", BUGLE_LOG_DEBUG, "exit");
    return ret;
}

/* Note: for the blocking condition event extractions, we cannot simply
 * call the real function, because interception events may occur before
 * the real function ever returns. On the other hand, we need to call
 * some blocking method to avoid hammering the CPU. Instead, we use
 * XIfEvent to wait until either an interception event or a user-requested
 * event arrives. If the former, we process it and start again, if the
 * latter we return the event.
 */

typedef struct
{
    Window w;
    long event_mask;
    Bool (*predicate)(Display *, XEvent *, XPointer);
    XPointer arg;
    int use_w;
    int use_event_mask;
    int use_predicate;
} if_block_data;

static Bool matches_mask(XEvent *event, unsigned long mask)
{
    /* Loosely based on evtomask.c and ChkMaskEv.c from XOrg 6.8.2 */
    switch (event->type)
    {
    case KeyPress: return (mask & KeyPressMask) ? True : False;
    case KeyRelease: return (mask & KeyReleaseMask) ? True : False;
    case ButtonPress: return (mask & ButtonPressMask) ? True : False;
    case ButtonRelease: return (mask & ButtonReleaseMask) ? True : False;
    case MotionNotify:
        if (mask & (PointerMotionMask | PointerMotionHintMask | ButtonMotionMask))
            return True;
        if (mask & (Button1MotionMask | Button2MotionMask | Button3MotionMask | Button4MotionMask | Button5MotionMask) & event->xmotion.state)
            return True;
        return False;
    case EnterNotify: return (mask & EnterWindowMask) ? True : False;
    case LeaveNotify: return (mask & LeaveWindowMask) ? True : False;
    case FocusIn:
    case FocusOut: return (mask & FocusChangeMask) ? True : False;
    case KeymapStateMask: return (mask & KeymapNotify) ? True : False;
    case Expose:
    case GraphicsExpose:
    case NoExpose: return (mask & ExposureMask) ? True : False;
    case VisibilityNotify: return (mask & VisibilityChangeMask) ? True : False;
    case CreateNotify: return (mask & SubstructureNotifyMask) ? True : False;
    case DestroyNotify:
    case UnmapNotify:
    case MapNotify:
    case ReparentNotify:
    case ConfigureNotify:
    case GravityNotify:
    case CirculateNotify: return (mask & (SubstructureNotifyMask | StructureNotifyMask)) ? True : False;
    case MapRequest:
    case ConfigureRequest:
    case CirculateRequest: return (mask & SubstructureRedirectMask) ? True : False;
    case ResizeRequest: return (mask & ResizeRedirectMask) ? True : False;
    case PropertyNotify: return (mask & PropertyChangeMask) ? True : False;
    case ColormapNotify: return (mask & ColormapChangeMask) ? True : False;
    case SelectionClear:
    case SelectionRequest:
    case SelectionNotify:
    case ClientMessage:
    case MappingNotify: return False;
    default:
        /* Unknown event! This is probably related to an extension. We
         * return True for now to be safe. That means that BuGLe might
         * wait longer than expected for an event, but it will never
         * cause to wait forever for one.
         */
        return True;
    }
}

static Bool if_block(Display *dpy, XEvent *event, XPointer arg)
{
    const if_block_data *data;

    data = (if_block_data *) arg;
    if (data->use_w && event->xany.window != data->w) return False;
    if (data->use_event_mask && !matches_mask(event, data->event_mask)) return False;
    if (data->use_predicate && !(*data->predicate)(dpy, event, data->arg)) return False;
    return True;
}

static Bool if_block_intercept(Display *dpy, XEvent *event, XPointer arg)
{
    return event_predicate(dpy, event, NULL) || if_block(dpy, event, arg);
}

int XWindowEvent(Display *dpy, Window w, long event_mask, XEvent *event)
{
    int ret;
    if_block_data data;

    bugle_initialise_all();
    if (!active) return (*real_XWindowEvent)(dpy, w, event_mask, event);
    bugle_log("input", "XWindowEvent", BUGLE_LOG_DEBUG, "enter");
    initialise_keycodes(dpy);
    extract_events(dpy);
    data.use_w = 1;
    data.use_event_mask = 1;
    data.use_predicate = 0;
    data.w = w;
    data.event_mask = event_mask;
    while ((ret = (*real_XIfEvent)(dpy, event, if_block_intercept, (XPointer) &data)) != 0)
    {
        if (!event_predicate(dpy, event, NULL))
        {
            bugle_log("input", "XWindowEvent", BUGLE_LOG_DEBUG, "exit");
            return ret;
        }
        handle_event(dpy, event);
    }
    bugle_log("input", "XWindowEvent", BUGLE_LOG_DEBUG, "exit");
    return ret;
}

Bool XCheckWindowEvent(Display *dpy, Window w, long event_mask, XEvent *event)
{
    Bool ret;

    bugle_initialise_all();
    if (!active) return (*real_XCheckWindowEvent)(dpy, w, event_mask, event);
    bugle_log("input", "XCheckWindowEvent", BUGLE_LOG_DEBUG, "enter");
    extract_events(dpy);
    while ((ret = (*real_XCheckWindowEvent)(dpy, w, event_mask, event)) == True)
    {
        if (!event_predicate(dpy, event, NULL))
        {
            bugle_log("input", "XCheckWindowEvent", BUGLE_LOG_DEBUG, "exit");
            return ret;
        }
        handle_event(dpy, event);
    }
    bugle_log("input", "XCheckWindowEvent", BUGLE_LOG_DEBUG, "exit");
    return ret;
}

int XMaskEvent(Display *dpy, long event_mask, XEvent *event)
{
    int ret;
    if_block_data data;

    bugle_initialise_all();
    if (!active) return (*real_XMaskEvent)(dpy, event_mask, event);
    bugle_log("input", "XMaskEvent", BUGLE_LOG_DEBUG, "enter");
    extract_events(dpy);
    data.use_w = 0;
    data.use_event_mask = 1;
    data.use_predicate = 0;
    data.event_mask = event_mask;
    while ((ret = (*real_XIfEvent)(dpy, event, if_block_intercept, (XPointer) &data)) != 0)
    {
        if (!event_predicate(dpy, event, NULL))
        {
            bugle_log("input", "XMaskEvent", BUGLE_LOG_DEBUG, "exit");
            return ret;
        }
        handle_event(dpy, event);
    }
    bugle_log("input", "XMaskEvent", BUGLE_LOG_DEBUG, "exit");
    return ret;
}

Bool XCheckMaskEvent(Display *dpy, long event_mask, XEvent *event)
{
    Bool ret;

    bugle_initialise_all();
    if (!active) return (*real_XCheckMaskEvent)(dpy, event_mask, event);
    bugle_log("input", "XCheckMaskEvent", BUGLE_LOG_DEBUG, "enter");
    extract_events(dpy);
    while ((ret = (*real_XCheckMaskEvent)(dpy, event_mask, event)) == True)
    {
        if (!event_predicate(dpy, event, NULL))
        {
            bugle_log("input", "XCheckMaskEvent", BUGLE_LOG_DEBUG, "exit");
            return ret;
        }
        handle_event(dpy, event);
    }
    bugle_log("input", "XCheckMaskEvent", BUGLE_LOG_DEBUG, "exit");
    return ret;
}

Bool XCheckTypedEvent(Display *dpy, int type, XEvent *event)
{
    Bool ret;

    bugle_initialise_all();
    if (!active) return (*real_XCheckTypedEvent)(dpy, type, event);
    bugle_log("input", "XCheckTypedEvent", BUGLE_LOG_DEBUG, "enter");
    extract_events(dpy);
    while ((ret = (*real_XCheckTypedEvent)(dpy, type, event)) == True)
    {
        if (!event_predicate(dpy, event, NULL))
        {
            bugle_log("input", "XCheckTypedEvent", BUGLE_LOG_DEBUG, "exit");
            return ret;
        }
        handle_event(dpy, event);
    }
    bugle_log("input", "XCheckTypedEvent", BUGLE_LOG_DEBUG, "exit");
    return ret;
}

Bool XCheckTypedWindowEvent(Display *dpy, Window w, int type, XEvent *event)
{
    Bool ret;

    bugle_initialise_all();
    if (!active) return (*real_XCheckTypedWindowEvent)(dpy, w, type, event);
    bugle_log("input", "XCheckTypedWindowEvent", BUGLE_LOG_DEBUG, "enter");
    extract_events(dpy);
    while ((ret = (*real_XCheckTypedWindowEvent)(dpy, w, type, event)) == True)
    {
        if (!event_predicate(dpy, event, NULL))
        {
            bugle_log("input", "XCheckTypedWindowEvent", BUGLE_LOG_DEBUG, "exit");
            return ret;
        }
        handle_event(dpy, event);
    }
    bugle_log("input", "XCheckTypedWindowEvent", BUGLE_LOG_DEBUG, "exit");
    return ret;
}

int XIfEvent(Display *dpy, XEvent *event, Bool (*predicate)(Display *, XEvent *, XPointer), XPointer arg)
{
    int ret;
    if_block_data data;

    bugle_initialise_all();
    if (!active) return (*real_XIfEvent)(dpy, event, predicate, arg);
    bugle_log("input", "XIfEvent", BUGLE_LOG_DEBUG, "enter");
    extract_events(dpy);
    data.use_w = 0;
    data.use_event_mask = 0;
    data.use_predicate = 1;
    data.predicate = predicate;
    data.arg = arg;
    while ((ret = (*real_XIfEvent)(dpy, event, if_block_intercept, (XPointer) &data)) != 0)
    {
        if (!event_predicate(dpy, event, NULL))
        {
            bugle_log("input", "XIfEvent", BUGLE_LOG_DEBUG, "exit");
            return ret;
        }
        handle_event(dpy, event);
    }
    bugle_log("input", "XIfEvent", BUGLE_LOG_DEBUG, "exit");
    return ret;
}

Bool XCheckIfEvent(Display *dpy, XEvent *event, Bool (*predicate)(Display *, XEvent *, XPointer), XPointer arg)
{
    Bool ret;

    bugle_initialise_all();
    if (!active) return (*real_XCheckIfEvent)(dpy, event, predicate, arg);
    bugle_log("input", "XCheckIfEvent", BUGLE_LOG_DEBUG, "enter");
    extract_events(dpy);
    while ((ret = (*real_XCheckIfEvent)(dpy, event, predicate, arg)) == True)
    {
        if (!event_predicate(dpy, event, NULL))
        {
            bugle_log("input", "XCheckIfEvent", BUGLE_LOG_DEBUG, "exit");
            return ret;
        }
        handle_event(dpy, event);
    }
    bugle_log("input", "XCheckIfEvent", BUGLE_LOG_DEBUG, "exit");
    return ret;
}

int XPeekIfEvent(Display *dpy, XEvent *event, Bool (*predicate)(Display *, XEvent *, XPointer), XPointer arg)
{
    int ret;
    if_block_data data;

    bugle_initialise_all();
    if (!active) return (*real_XPeekIfEvent)(dpy, event, predicate, arg);
    bugle_log("input", "XPeekIfEvent", BUGLE_LOG_DEBUG, "enter");
    extract_events(dpy);
    data.use_w = 0;
    data.use_event_mask = 0;
    data.use_predicate = 1;
    data.predicate = predicate;
    data.arg = arg;
    while ((ret = (*real_XPeekIfEvent)(dpy, event, if_block_intercept, (XPointer) &data)) != 0)
    {
        if (!event_predicate(dpy, event, NULL))
        {
            bugle_log("input", "XPeekIfEvent", BUGLE_LOG_DEBUG, "exit");
            return ret;
        }
        /* Peek doesn't remove the event, so do another extract run to
         * clear it */
        extract_events(dpy);
    }
    bugle_log("input", "XPeekIfEvent", BUGLE_LOG_DEBUG, "exit");
    return ret;
}

int XEventsQueued(Display *dpy, int mode)
{
    int events;

    bugle_initialise_all();
    if (!active) return (*real_XEventsQueued)(dpy, mode);
    bugle_log("input", "XEventsQueued", BUGLE_LOG_DEBUG, "enter");

    do
    {
        events = (*real_XEventsQueued)(dpy, mode);
    } while (events > 0 && extract_events(dpy));
    bugle_log("input", "XEventsQueued", BUGLE_LOG_DEBUG, "exit");
    return events;
}

int XPending(Display *dpy)
{
    int events;

    bugle_initialise_all();
    if (!active) return (*real_XPending)(dpy);
    bugle_log("input", "XPending", BUGLE_LOG_DEBUG, "enter");

    do
    {
        events = (*real_XPending)(dpy);
    } while (events > 0 && extract_events(dpy));
    bugle_log("input", "XPending", BUGLE_LOG_DEBUG, "exit");
    return events;
}

static void adjust_event_mask(Display *dpy, Window w)
{
    XWindowAttributes attributes;
    unsigned long mask;

    XGetWindowAttributes(dpy, w, &attributes);
    mask = attributes.your_event_mask;
    if ((~mask & EVENT_MASK)
        || (mask & EVENT_UNMASK))
    {
        mask |= EVENT_MASK;
        mask &= ~EVENT_UNMASK;
        (*real_XSelectInput)(dpy, w, mask);
    }
}

Window XCreateWindow(Display *dpy, Window parent, int x, int y,
                     unsigned int width, unsigned int height,
                     unsigned int border_width, int depth,
                     unsigned int c_class, Visual *visual,
                     unsigned long valuemask,
                     XSetWindowAttributes *attributes)
{
    Window ret;

    bugle_initialise_all();
    if (!active)
        return (*real_XCreateWindow)(dpy, parent, x, y, width, height,
                                     border_width, depth, c_class, visual,
                                     valuemask, attributes);
    bugle_log("input", "XCreateWindow", BUGLE_LOG_DEBUG, "enter");
    ret = (*real_XCreateWindow)(dpy, parent, x, y, width, height,
                                border_width, depth, c_class, visual,
                                valuemask, attributes);
    if (ret != None)
        adjust_event_mask(dpy, ret);
    bugle_log("input", "XCreateWindow", BUGLE_LOG_DEBUG, "exit");
    return ret;
}

Window XCreateSimpleWindow(Display *dpy, Window parent, int x, int y,
                           unsigned int width, unsigned int height,
                           unsigned int border_width, unsigned long border,
                           unsigned long background)
{
    Window ret;

    bugle_initialise_all();
    if (!active)
        return (*real_XCreateSimpleWindow)(dpy, parent, x, y, width, height,
                                           border_width, border, background);
    bugle_log("input", "XCreateSimpleWindow", BUGLE_LOG_DEBUG, "enter");
    ret = (*real_XCreateSimpleWindow)(dpy, parent, x, y, width, height,
                                      border_width, border, background);
    if (ret != None)
        adjust_event_mask(dpy, ret);
    bugle_log("input", "XCreateSimpleWindow", BUGLE_LOG_DEBUG, "exit");
    return ret;
}

/* FIXME: capture XChangeWindowAttributes */

int XSelectInput(Display *dpy, Window w, long event_mask)
{
    int ret;

    bugle_initialise_all();
    if (!active) return (*real_XSelectInput)(dpy, w, event_mask);
    bugle_log("input", "XSelectInput", BUGLE_LOG_DEBUG, "enter");
    event_mask |= EVENT_MASK;
    event_mask &= ~EVENT_UNMASK;
    ret = (*real_XSelectInput)(dpy, w, event_mask);
    bugle_log("input", "XSelectInput", BUGLE_LOG_DEBUG, "exit");
    return ret;
}

void bugle_input_invalidate_window(bugle_input_event *event)
{
    glwin_display dpy;
    XEvent dirty;
    Window w;
    Window root;
    int x, y;
    unsigned int width, height, border_width, depth;

    dpy = event->xany.display;
    w = event->xany.window != None ? event->xany.window : PointerWindow;
    dirty.type = Expose;
    dirty.xexpose.display = dpy;
    dirty.xexpose.window = event->xany.window;
    dirty.xexpose.x = 0;
    dirty.xexpose.y = 0;
    dirty.xexpose.width = 1;
    dirty.xexpose.height = 1;
    dirty.xexpose.count = 0;
    if (event->xany.window != None
        && XGetGeometry(dpy, w, &root, &x, &y, &width, &height, &border_width, &depth))
    {
        dirty.xexpose.width = width;
        dirty.xexpose.height = height;
    }
    XSendEvent(dpy, PointerWindow, True, ExposureMask, &dirty);
}

void input_initialise(void)
{
    bugle_dl_module handle;

    handle = bugle_dl_open("libX11", 0);
    if (handle == NULL)
    {
        /* The first attempt ought to be the most portable, but also fails
         * when trying to run with 32-bit apps on a 64-bit system. This is
         * a fallback.
         */
        handle = bugle_dl_open("libX11", BUGLE_DL_FLAG_SUFFIX);
    }
    if (handle == NULL)
    {
        fputs("ERROR: cannot locate libX11. There is something unusual about the linkage\n"
              "of your application. You will need to pass --disable-input to configure\n"
              "when configuring bugle, and will you lose the key and mouse interception\n"
              "features. Please contact the author to help him resolve this issue.\n", stderr);
        exit(1);
    }

    real_XNextEvent = (int (*)(Display *, XEvent *)) bugle_dl_sym_function(handle, "XNextEvent");
    real_XPeekEvent = (int (*)(Display *, XEvent *)) bugle_dl_sym_function(handle, "XPeekEvent");
    real_XWindowEvent = (int (*)(Display *, Window, long, XEvent *)) bugle_dl_sym_function(handle, "XWindowEvent");
    real_XCheckWindowEvent = (Bool (*)(Display *, Window, long, XEvent *)) bugle_dl_sym_function(handle, "XCheckWindowEvent");
    real_XMaskEvent = (int (*)(Display *, long, XEvent *)) bugle_dl_sym_function(handle, "XMaskEvent");
    real_XCheckMaskEvent = (Bool (*)(Display *, long, XEvent *)) bugle_dl_sym_function(handle, "XCheckMaskEvent");
    real_XCheckTypedEvent = (Bool (*)(Display *, int, XEvent *)) bugle_dl_sym_function(handle, "XCheckTypedEvent");
    real_XCheckTypedWindowEvent = (Bool (*)(Display *, Window, int, XEvent *)) bugle_dl_sym_function(handle, "XCheckTypedWindowEvent");

    real_XIfEvent = (int (*)(Display *, XEvent *, Bool (*)(Display *, XEvent *, XPointer), XPointer)) bugle_dl_sym_function(handle, "XIfEvent");
    real_XCheckIfEvent = (Bool (*)(Display *, XEvent *, Bool (*)(Display *, XEvent *, XPointer), XPointer)) bugle_dl_sym_function(handle, "XCheckIfEvent");
    real_XPeekIfEvent = (int (*)(Display *, XEvent *, Bool (*)(Display *, XEvent *, XPointer), XPointer)) bugle_dl_sym_function(handle, "XPeekIfEvent");

    real_XEventsQueued = (int (*)(Display *, int)) bugle_dl_sym_function(handle, "XEventsQueued");
    real_XPending = (int (*)(Display *)) bugle_dl_sym_function(handle, "XPending");

    real_XCreateWindow = (Window (*)(Display *, Window, int, int, unsigned int, unsigned int, unsigned int, int, unsigned int, Visual *, unsigned long, XSetWindowAttributes *)) bugle_dl_sym_function(handle, "XCreateWindow");
    real_XCreateSimpleWindow = (Window (*)(Display *, Window, int, int, unsigned int, unsigned int, unsigned int, unsigned long, unsigned long)) bugle_dl_sym_function(handle, "XCreateSimpleWindow");
    real_XSelectInput = (int (*)(Display *, Window, long)) bugle_dl_sym_function(handle, "XSelectInput");

    if (!real_XNextEvent
        || !real_XPeekEvent
        || !real_XWindowEvent
        || !real_XCheckWindowEvent
        || !real_XMaskEvent
        || !real_XCheckMaskEvent
        || !real_XCheckTypedEvent
        || !real_XCheckTypedWindowEvent
        || !real_XIfEvent
        || !real_XCheckIfEvent
        || !real_XPeekIfEvent
        || !real_XEventsQueued
        || !real_XPending
        || !real_XCreateWindow
        || !real_XSelectInput)
    {
        fputs("ERROR: cannot load X symbols. There is something unusual about the linkage\n"
              "of your application. You will need to pass --disable-input to configure\n"
              "when configuring bugle, and will you lose the key and mouse interception\n"
              "features. Please contact the author to help him resolve this issue.\n", stderr);
        exit(1);
    }
    bugle_list_init(&handlers, bugle_free);
    bugle_thread_lock_init(&keycodes_lock);
}

static bugle_bool input_key_lookup(const char *name, bugle_input_key *key)
{
    bugle_input_keysym keysym;

    keysym = XStringToKeysym((char *) name);
    if (keysym != BUGLE_INPUT_NOSYMBOL)
    {
        key->keysym = keysym;
        return BUGLE_TRUE;
    }
    else
        return BUGLE_FALSE;
}

#endif /* BUGLE_WINSYS_X11 */

#if BUGLE_WINSYS_WINDOWS

LRESULT CALLBACK input_key_hook(int code, WPARAM wParam, LPARAM lParam)
{
    bugle_input_key key;
    linked_list_node *i;
    handler *h;

    /* Refer to MSDN documentation for what this hook function can and
     * should do */
    if (code == HC_ACTION)
    {
        key.keysym = wParam;
        key.state = 0;
        if (GetKeyState(VK_SHIFT) & 0x8000)
            key.state |= BUGLE_INPUT_SHIFT_BIT;
        if (GetKeyState(VK_CONTROL) & 0x8000)
            key.state |= BUGLE_INPUT_CONTROL_BIT;
        if (GetKeyState(VK_MENU) & 0x8000)
            key.state |= BUGLE_INPUT_ALT_BIT;
        key.press = (lParam & 0x80000000U) == 0;
        for (i = bugle_list_head(&handlers); i; i = bugle_list_next(i))
        {
            h = (handler *) bugle_list_data(i);
            if (h->key.keysym == key.keysym && h->key.state == key.state
                && h->key.press == key.press
                && (!h->wanted || (*h->wanted)(&key, h->arg, NULL)))
                (*h->callback)(&key, h->arg, NULL);
        }
        bugle_log_printf("input", "input_key_hook", BUGLE_LOG_DEBUG, "wParam = %x lParam = %08x state = %04x",
                         (unsigned int) wParam, (unsigned int) lParam, (unsigned int) key.state);
    }
    return CallNextHookEx(NULL, code, wParam, lParam);
}

LRESULT CALLBACK input_mouse_hook(int code, WPARAM wParam, LPARAM lParam)
{
    if (code == HC_ACTION)
    {
        if (mouse_active && (wParam == WM_NCMOUSEMOVE || wParam == WM_MOUSEMOVE))
        {
            MOUSEHOOKSTRUCT *info;
            RECT area;
            info = (MOUSEHOOKSTRUCT *) lParam;
            if (mouse_first)
            {
                mouse_window = info->hwnd;
                GetWindowRect(mouse_window, &area);
                mouse_x = (area.left + area.right) / 2;
                mouse_y = (area.top + area.bottom) / 2;
                SetCursorPos(mouse_x, mouse_y);
                mouse_first = BUGLE_FALSE;
            }
            else if (mouse_x != info->pt.x || mouse_y != info->pt.y)
            {
                bugle_log_printf("input", "mouse", BUGLE_LOG_DEBUG,
                                 "mouse moved by (%d, %d) ref = (%d, %d)",
                                 (int) info->pt.x - mouse_x, (int) info->pt.y - mouse_y,
                                 mouse_x, mouse_y);
                (*mouse_callback)((int) info->pt.x - mouse_x, (int) info->pt.y - mouse_y, NULL);
                SetCursorPos(mouse_x, mouse_y);
            }
        }
    }
    return CallNextHookEx(NULL, code, wParam, lParam);
}

void bugle_input_invalidate_window(bugle_input_event *event)
{
    /* FIXME: implement? */
}

void input_initialise(void)
{
    SetWindowsHookEx(WH_KEYBOARD, input_key_hook, (HINSTANCE) NULL, GetCurrentThreadId());
    SetWindowsHookEx(WH_MOUSE, input_mouse_hook, (HINSTANCE) NULL, GetCurrentThreadId());
}

static bugle_bool input_key_lookup(const char *name, bugle_input_key *key)
{
    if (name[0] >= 'A' && name[0] <= 'Z' && name[1] == '\0')
    {
        key->keysym = name[0];
        return BUGLE_TRUE;
    }
    else
        return BUGLE_FALSE;
}

#endif /* BUGLE_WINSYS_WINDOWS */

#if BUGLE_WINSYS_NONE

void bugle_input_invalidate_window(bugle_input_event *event)
{
}

void input_initialise(void)
{
}

static bugle_bool input_key_lookup(const char *name, bugle_input_key *key)
{
    /* make all input keys appear valid, so that we don't reject config
     * files just because they have keys in them.
     */
    key->keysym = BUGLE_INPUT_NOSYMBOL;
    return BUGLE_TRUE;
}

#endif /* BUGLE_WINSYS_NONE */

bugle_bool bugle_input_key_lookup(const char *name, bugle_input_key *key)
{
    unsigned int state = 0;

    key->press = BUGLE_TRUE;
    while (BUGLE_TRUE)
    {
        if (name[0] == 'C' && name[1] == '-')
        {
            state |= BUGLE_INPUT_CONTROL_BIT;
            name += 2;
        }
        else if (name[0] == 'S' && name[1] == '-')
        {
            state |= BUGLE_INPUT_SHIFT_BIT;
            name += 2;
        }
        else if (name[0] == 'A' && name[1] == '-')
        {
            state |= BUGLE_INPUT_ALT_BIT;
            name += 2;
        }
        else
        {
            if (input_key_lookup(name, key))
            {
                key->state = state;
                return BUGLE_TRUE;
            }
            else
                return BUGLE_FALSE;
        }
    }
}
