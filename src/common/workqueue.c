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

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <assert.h>
#include <bugle/bool.h>
#include <bugle/memory.h>
#include "platform/threads.h"
#include "common/workqueue.h"

#define BUGLE_WORKQUEUE_SIZE 4

struct bugle_workqueue
{
    bugle_thread_sem_t data_sem;
    bugle_thread_sem_t space_sem;
    /* The main thread examines the tail to implement gldb_queue_has_data,
     * so access to it must be thread-safe.
     */
    bugle_thread_lock_t tail_lock;
    bugle_thread_handle worker_thread;

    bugle_workqueue_consume consume;
    void *consume_data;
    bugle_workqueue_wakeup wakeup;
    void *wakeup_data;

    bugle_bool running;
    unsigned int head, tail;
    void *items[BUGLE_WORKQUEUE_SIZE];
};

static unsigned int bugle_workqueue_thread(void *arg)
{
    bugle_workqueue *queue = (bugle_workqueue *) arg;
    void *item;
    bugle_bool more;

    assert(queue != NULL);

    do
    {
        more = queue->consume(queue, queue->consume_data, &item);
        bugle_thread_sem_wait(&queue->space_sem);
        bugle_thread_lock_lock(&queue->tail_lock);
        queue->items[queue->tail] = item;
        if (++queue->tail == BUGLE_WORKQUEUE_SIZE)
            queue->tail = 0;
        bugle_thread_lock_unlock(&queue->tail_lock);
        bugle_thread_sem_post(&queue->data_sem);

        if (queue->wakeup != NULL)
            queue->wakeup(queue, queue->wakeup_data);
    } while (more);
    return 0;
}

bugle_workqueue *bugle_workqueue_new(bugle_workqueue_consume consume,
                                     void *user_data)
{
    bugle_workqueue *queue = BUGLE_MALLOC(bugle_workqueue);
    if (bugle_thread_sem_init(&queue->data_sem, 0) != 0)
        goto cleanup_data_sem;
    /* Note: we avoid allowing the queue to completely fill, so that
     * head == tail can be used to test for empty.
     */
    if (bugle_thread_sem_init(&queue->space_sem, BUGLE_WORKQUEUE_SIZE - 1) != 0)
        goto cleanup_space_sem;
    if (bugle_thread_lock_init(&queue->tail_lock) != 0)
        goto cleanup_tail_lock;

    queue->consume = consume;
    queue->consume_data = user_data;
    queue->wakeup = NULL;
    queue->wakeup_data = NULL;
    queue->running = BUGLE_FALSE;
    queue->head = 0;
    queue->tail = 0;
    return queue;

cleanup_tail_lock:
    bugle_thread_sem_destroy(&queue->space_sem);
cleanup_space_sem:
    bugle_thread_sem_destroy(&queue->data_sem);
cleanup_data_sem:
    bugle_free(queue);
    return NULL;
}

void bugle_workqueue_set_wakeup(bugle_workqueue *queue,
                                bugle_workqueue_wakeup wakeup,
                                void *user_data)
{
    assert(queue != NULL);
    assert(!queue->running);
    queue->wakeup = wakeup;
    queue->wakeup_data = user_data;
}

void bugle_workqueue_start(bugle_workqueue *queue)
{
    assert(queue != NULL);
    assert(!queue->running);
    bugle_thread_create(&queue->worker_thread, bugle_workqueue_thread, queue);
    queue->running = BUGLE_TRUE;
}

void bugle_workqueue_stop(bugle_workqueue *queue)
{
    assert(queue != NULL);
    assert(queue->running);
    bugle_thread_join(queue->worker_thread, NULL);
    queue->running = BUGLE_FALSE;
}

void bugle_workqueue_free(bugle_workqueue *queue)
{
    if (queue == NULL)
        return;

    assert(!queue->running);
    bugle_thread_lock_destroy(&queue->tail_lock);
    bugle_thread_sem_destroy(&queue->data_sem);
    bugle_thread_sem_destroy(&queue->space_sem);
    bugle_free(queue);
}

bugle_bool bugle_workqueue_has_data(bugle_workqueue *queue)
{
    bugle_bool ret;

    assert(queue != NULL);
    bugle_thread_lock_lock(&queue->tail_lock);
    ret = (queue->head != queue->tail);
    bugle_thread_lock_unlock(&queue->tail_lock);
    return ret;
}

void *bugle_workqueue_get_item(bugle_workqueue *queue)
{
    void *ret;
    assert(queue != NULL);
    bugle_thread_sem_wait(&queue->data_sem);
    ret = queue->items[queue->head];
    if (++queue->head == BUGLE_WORKQUEUE_SIZE)
        queue->head = 0;
    bugle_thread_sem_post(&queue->space_sem);
    return ret;
}
