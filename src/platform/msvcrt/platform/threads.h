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

/* In general, the function names match either pthread or gnulib conventions.
 *
 * Some caveats:
 * - read-write locks may degrade to simple mutexes if rwlock functionality is
 *   not available. rwlocks should only be used to gain efficiency from
 *   not serialising multiple readers.
 */

#ifndef BUGLE_PLATFORM_THREADS_H
#define BUGLE_PLATFORM_THREADS_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#ifndef WIN32_LEAN_AND_MEAN
# define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stddef.h>
#include <signal.h>
#include <bugle/export.h>

typedef struct
{
    volatile LONG value;
    volatile LONG complete;
} bugle_thread_once_t;

#define BUGLE_THREAD_ONCE_INIT { 0, 0 }
BUGLE_EXPORT_PRE void bugle_thread_once(bugle_thread_once_t *once, void (*function)(void)) BUGLE_EXPORT_POST;

typedef CRITICAL_SECTION bugle_thread_lock_t;
#define bugle_thread_lock_init(lock) (InitializeCriticalSection(lock))
#define bugle_thread_lock_destroy(lock) (DeleteCriticalSection(lock))
#define bugle_thread_lock_lock(lock) (EnterCriticalSection(lock))
#define bugle_thread_lock_unlock(lock) (LeaveCriticalSection(lock))

/* RW lock implementation that prevents writer starvation.
 */
typedef struct
{
    /* Writers hold this mutex over their lifetime. Readers take it momentarily
     * on entry. This ensures that readers are blocked while a writer is
     * active or waiting.
     */
    CRITICAL_SECTION mutex;

    /* Number of active readers (i.e. those that have successfully taken the
     * lock). Increments are done with mutex held, so that a thread observing
     * no readers and with mutex held can be certain that a reader won't
     * appear until the lock is released.
     *
     * If there is a writer waiting or active, RWLOCK_WRITER_BIT is set.
     */
    volatile LONG num_readers;
    CRITICAL_SECTION num_readers_lock;

    /* This is an auto-reset event that is used by the last exiting reader
     * to signal a writer that has the lock but is waiting for readers to
     * finish. It is unsignalled except during this transfer.
     */
    HANDLE readers_done;
} bugle_thread_rwlock_t;

BUGLE_EXPORT_PRE void bugle_thread_rwlock_init(bugle_thread_rwlock_t *rwlock) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void bugle_thread_rwlock_destroy(bugle_thread_rwlock_t *rwlock) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void bugle_thread_rwlock_rdlock(bugle_thread_rwlock_t *rwlock) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void bugle_thread_rwlock_wrlock(bugle_thread_rwlock_t *rwlock) BUGLE_EXPORT_POST;
BUGLE_EXPORT_PRE void bugle_thread_rwlock_unlock(bugle_thread_rwlock_t *rwlock) BUGLE_EXPORT_POST;

typedef DWORD bugle_thread_key_t;
#define bugle_thread_key_create(name, destructor) \
   do { \
      if (TLS_OUT_OF_INDEXES == (*(name) = TlsAlloc())) abort(); \
   } while (0)
#define bugle_thread_getspecific(key) (TlsGetValue(key))
#define bugle_thread_setspecific(key, value) \
    do { \
        if (!TlsSetValue(key, value)) abort(); \
    } while (0)
#define bugle_thread_key_delete(key) (TlsFree(key))

typedef DWORD bugle_thread_t;
#define bugle_thread_self() (GetCurrentThreadId())
#define bugle_thread_raise(sig) (raise(sig))

#define bugle_flockfile(f) ((void) 0)
#define bugle_funlockfile(f) ((void) 0)

#define BUGLE_CONSTRUCTOR(fn) static bugle_thread_once_t fn ## _once = BUGLE_THREAD_ONCE_INIT;
#define BUGLE_RUN_CONSTRUCTOR(fn) bugle_thread_once(&(fn ## _once), (fn))

#endif /* !BUGLE_PLATFORM_THREADS_H */
