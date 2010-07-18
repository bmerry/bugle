/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2010  Bruce Merry
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

/* Utility functions to consume data in one thread and pass it to another.
 */

#ifndef BUGLE_COMMON_WORKQUEUE_H
#define BUGLE_COMMON_WORKQUEUE_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <bugle/bool.h>

typedef struct bugle_workqueue bugle_workqueue;

/* Obtains data from the source, and fills in *item with a value that is
 * passed back to the calling thread.
 *
 * Returns BUGLE_FALSE if the source is exhausted - the returned item is still
 * added (the expected use is that it will be NULL in this case, which would
 * signal to the main thread that the source is exhausted).
 */
typedef bugle_bool (*bugle_workqueue_consume)(bugle_workqueue *queue, void *arg, void **item);

/* Callback function that is called after data is received. This should signal
 * the main thread (e.g. by posting a semaphore) that data is available.
 *
 * This callback may be NULL to indicate that no wakeup is required. Note
 * that no wakeup is required to unblock bugle_workqueue_get_item - it is
 * provided to allow a GUI event loop to be woken.
 */
typedef void (*bugle_workqueue_wakeup)(bugle_workqueue *queue, void *arg);

/* Creates a new queue, but does not start it running. Returns NULL
 * on failure. The user_data is passed to the consume callback.
 */
BUGLE_EXPORT_PRE bugle_workqueue *bugle_workqueue_new(bugle_workqueue_consume consume,
                                                      void *user_data) BUGLE_EXPORT_POST;

/* Set the wakeup callback. This must only be called when not running. */
BUGLE_EXPORT_PRE void bugle_workqueue_set_wakeup(bugle_workqueue *queue,
                                                 bugle_workqueue_wakeup wakeup,
                                                 void *user_data) BUGLE_EXPORT_POST;

/* Spawns the worker thread to process this queue */
BUGLE_EXPORT_PRE void bugle_workqueue_start(bugle_workqueue *queue) BUGLE_EXPORT_POST;

/* Blocks until the worker thread is finished. */
BUGLE_EXPORT_PRE void bugle_workqueue_stop(bugle_workqueue *queue) BUGLE_EXPORT_POST;

/* Destroys the queue (should only be done after bugle_workqueue_stop) */
BUGLE_EXPORT_PRE void bugle_workqueue_free(bugle_workqueue *queue) BUGLE_EXPORT_POST;

/* Determines whether there is data ready to be read. */
BUGLE_EXPORT_PRE bugle_bool bugle_workqueue_has_data(bugle_workqueue *queue) BUGLE_EXPORT_POST;

/* Obtains an item from the queue (blocks until data is available) */
BUGLE_EXPORT_PRE void *bugle_workqueue_get_item(bugle_workqueue *queue) BUGLE_EXPORT_POST;

#endif /* !BUGLE_COMMON_WORKQUEUE_H */
