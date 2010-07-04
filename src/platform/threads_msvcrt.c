/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2009-2010  Bruce Merry
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
#ifndef WIN32_LEAN_AND_MEAN
# define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <process.h>
#include <bugle/bool.h>
#include <bugle/memory.h>
#include <assert.h>
#include "platform/threads.h"

#define RWLOCK_WRITER_BIT 0x40000000L

void bugle_thread_once(bugle_thread_once_t *once, void (*function)(void))
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

int bugle_thread_rwlock_init(bugle_thread_rwlock_t *rwlock)
{
    rwlock->num_readers = 0;
    InitializeCriticalSection(&rwlock->mutex);
    InitializeCriticalSection(&rwlock->num_readers_lock);
    rwlock->readers_done = CreateEvent(NULL, TRUE, TRUE, NULL);
    /* TODO: error handling */
    return 0;
}

int bugle_thread_rwlock_destroy(bugle_thread_rwlock_t *rwlock)
{
    DeleteCriticalSection(&rwlock->num_readers_lock);
    DeleteCriticalSection(&rwlock->mutex);
    CloseHandle(&rwlock->readers_done);
    /* TODO: error handling */
    return 0;
}

int bugle_thread_rwlock_rdlock(bugle_thread_rwlock_t *rwlock)
{
    /* Wait on any waiting or active writers */
    EnterCriticalSection(&rwlock->mutex);

    EnterCriticalSection(&rwlock->num_readers_lock);
    if (rwlock->num_readers == 0)
        ResetEvent(rwlock->readers_done);
    rwlock->num_readers++;
    LeaveCriticalSection(&rwlock->num_readers_lock);

    LeaveCriticalSection(&rwlock->mutex);
    /* TODO: error handling */
    return 0;
}

int bugle_thread_rwlock_wrlock(bugle_thread_rwlock_t *rwlock)
{
    /* Exclude new readers and other writers */
    EnterCriticalSection(&rwlock->mutex);

    EnterCriticalSection(&rwlock->num_readers_lock);
    rwlock->num_readers |= RWLOCK_WRITER_BIT;
    LeaveCriticalSection(&rwlock->num_readers_lock);

    WaitForSingleObject(rwlock->readers_done, INFINITE);

    /* TODO: error handling */
    return 0;
}

int bugle_thread_rwlock_unlock(bugle_thread_rwlock_t *rwlock)
{
    /* If there are any readers, we must be a reader. Otherwise, we
     * are a writer and RWLOCK_WRITER_BIT must be set.
     */
    EnterCriticalSection(&rwlock->num_readers_lock);
    if (rwlock->num_readers == RWLOCK_WRITER_BIT)
    {
        rwlock->num_readers &= ~RWLOCK_WRITER_BIT;
        LeaveCriticalSection(&rwlock->num_readers_lock);

        LeaveCriticalSection(&rwlock->mutex);
    }
    else
    {
        --rwlock->num_readers;
        if (rwlock->num_readers == 0 || rwlock->num_readers == RWLOCK_WRITER_BIT)
            SetEvent(rwlock->readers_done);
        LeaveCriticalSection(&rwlock->num_readers_lock);
    }

    /* TODO: error handling */
    return 0;
}

int bugle_thread_sem_init(bugle_thread_sem_t *sem, unsigned int value)
{
    HANDLE h = CreateSemaphore(NULL, value, 0x7FFFFFFF, NULL);
    if (h == NULL)
        return -1;
    else
    {
        *sem = h;
        return 0;
    }
}

int bugle_thread_sem_destroy(bugle_thread_sem_t *sem)
{
    return CloseHandle(*sem) ? 0 : -1;
}

int bugle_thread_sem_post(bugle_thread_sem_t *sem)
{
    return ReleaseSemaphore(*sem, 1, NULL) ? 0 : -1;
}

int bugle_thread_sem_wait(bugle_thread_sem_t *sem)
{
    DWORD status = WaitForSingleObject(*sem, INFINITE);

    switch (status)
    {
    case WAIT_TIMEOUT: return 1;
    case WAIT_OBJECT_0: return 0;
    default: return -1;
    }
}

int bugle_thread_sem_trywait(bugle_thread_sem_t *sem)
{
    DWORD status = WaitForSingleObject(*sem, 0);

    switch (status)
    {
    case WAIT_TIMEOUT: return 1;
    case WAIT_OBJECT_0: return 0;
    default: return -1;
    }
}

/* _beginthreadex takes a __stdcall function, but we don't want to impose that
 * on our API. bugle_thread_create creates one of these structures to pass
 * the real entry-point and its arg. bugle_thread_create_helper then unpacks
 * it.
 */
typedef struct
{
    unsigned int (*start)(void *);
    void *arg;
} bugle_thread_create_struct;

static unsigned int __stdcall bugle_thread_create_helper(void *arg)
{
    bugle_thread_create_struct *s = (bugle_thread_create_struct *) arg;
    unsigned int (*start)(void *) = s->start;
    void *arg2 = s->arg;

    bugle_free(s);
    return start(arg2);
}

int bugle_thread_create(bugle_thread_handle *thread, unsigned int (*start)(void *), void *arg)
{
    bugle_thread_create_struct *s = BUGLE_MALLOC(bugle_thread_create_struct);
    uintptr_t handle;

    s->start = start;
    s->arg = arg;
    handle = _beginthreadex(NULL, 0, bugle_thread_create_helper, s, 0, NULL);
    if (handle == (uintptr_t) -1)
    {
        bugle_free(s);
        return -1;
    }
    *thread = (bugle_thread_handle) handle;
    return 0;
}

int bugle_thread_join(bugle_thread_handle thread, unsigned int *retval)
{
    DWORD status = WaitForSingleObject(thread, INFINITE);
    DWORD exitcode = 0;

    if (status != WAIT_OBJECT_0)
        return -1;
    GetExitCodeThread(thread, &exitcode);
    CloseHandle(thread);
    if (retval != NULL)
        *retval = exitcode;
    return 0;
}
