/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2009  Bruce Merry
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
#include "platform/threads.h"

#define RWLOCK_WRITER_BIT 0x40000000L

void _bugle_thread_once(bugle_thread_once_t *once, void (*function)(void))
{
    if (InterlockedIncrement(&once->value) == 1)
    {
        /* We got here first. */
       (*function)();
       once->complete = 1; /* Mark completion */
    }
    else
    {
        /* Unlike with pthreads, there are no sync primitives that
         * can be statically initialised, so we have to spin. In the
         * bugle use cases, the chance of contention is low and the
         * initialiser functions should not be particularly long-running,
         * so we keep it simple and spin until full completion rather than
         * waiting for critical section to be created as in gnulib.
         */
        while (!once->complete)
        {
            Sleep(0);
        }
    }
}

void _bugle_thread_rwlock_init(bugle_thread_rwlock_t *rwlock)
{
    rwlock->num_readers = 0;
    InitializeCriticalSection(&rwlock->mutex);
    rwlock->readers_done = CreateEvent(NULL, TRUE, FALSE, NULL);
    /* TODO: error handling */
}

void _bugle_thread_rwlock_destroy(bugle_thread_rwlock_t *rwlock)
{
    DestroyCriticalSection(&rwlock->mutex);
    CloseHandle(&rwlock->readers_done);
}

void _bugle_thread_rwlock_rdlock(bugle_thread_rwlock_t *rwlock)
{
    /* Wait on any waiting or active writers */
    EnterCriticalSection(&rwlock->mutex);

    InterlockedAdd(&rwlock->num_readers, 1);

    LeaveCriticalSection(&rwlock->mutex);
}

void _bugle_thread_rwlock_wrlock(bugle_thread_rwlock_t *rwlock)
{
    LONG num_readers;

    /* Exclude new readers and other writers */
    EnterCriticalSection(&rwlock->mutex);

    num_readers = InterlockedAdd(&rwlock->num_readers, RWLOCK_WRITER_BIT) - RWLOCK_WRITER_BIT;

    if (num_readers != 0)
        WaitForSingleEvent(rwlock->readers_done, INFINITE);
}

void _bugle_thread_rwlock_unlock(bugle_thread_rwlock_t *rwlock)
{
    bugle_bool is_writer;

    /* If there are any readers, we must be a reader. Otherwise, we
     * are a writer and RWLOCK_WRITER_BIT must be set.
     */
    if (rwlock->num_readers == RWLOCK_WRITER_BIT)
    {
        InterlockedAdd(&rwlock->num_readers, -RWLOCK_WRITER_BIT);
        LeaveCriticalSection(&rwlock->mutex);
    }
    else
    {
        LONG num_readers;
        num_readers = InterlockedAdd(&rwlock->num_readers, -1);
        if (num_readers == RWLOCK_WRITER_BIT)
        {
            /* We were the last reader, and a writer is waiting for us */
            SetEvent(&rwlock->readers_done);
        }
    }
}
