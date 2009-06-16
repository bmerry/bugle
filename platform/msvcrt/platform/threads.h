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

typedef struct
{
    volatile LONG value;
    volatile LONG complete;
} bugle_thread_once_t;
#define bugle_thread_once_define(storage, name) \
    storage bugle_thread_once_t name = { 0, 0 };
#define bugle_thread_once(name, function) \
    _bugle_thread_once(&(name), function)
static inline void _bugle_thread_once(bugle_thread_once_t *once, void (*function)(void))
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

typedef CRITICAL_SECTION bugle_thread_lock_t;
#define bugle_thread_lock_define(storage, name) \
    storage bugle_thread_lock_t name;
#define bugle_thread_lock_init(name) (InitializeCriticalSection(&(name)))
#define bugle_thread_lock_destroy(name) (DeleteCriticalSection(&(name)))
#define bugle_thread_lock_lock(name) (EnterCriticalSection(&(name)))
#define bugle_thread_lock_unlock(name) (LeaveCriticalSection(&(name)))

/* Devolve rwlocks to regular locks for now */
typedef bugle_thread_lock_t bugle_thread_rwlock_t;
# define bugle_thread_rwlock_define(storage, name) \
    bugle_thread_lock_define(storage, name)
# define bugle_thread_rwlock_init(name) bugle_thread_lock_init(name)
# define bugle_thread_rwlock_destroy(name) bugle_thread_lock_destroy(name)
# define bugle_thread_rwlock_rdlock(name) bugle_thread_lock_lock(name)
# define bugle_thread_rwlock_wrlock(name) bugle_thread_lock_lock(name)
# define bugle_thread_rwlock_unlock(name) bugle_thread_lock_unlock(name)

typedef DWORD bugle_thread_key_t;
#define bugle_thread_key_create(name, destructor) \
   do { \
      if (TLS_OUT_OF_INDEXES == ((name) = TlsAlloc())) abort(); \
   } while (0)
#define bugle_thread_getspecific(key) (TlsGetValue(key))
#define bugle_thread_setspecific(key, value) \
    do { \
        if (!TlsSetValue(key, value)) abort(); \
    } while (0)
#define bugle_thread_key_delete(key) (TlsFree(key))

typedef DWORD bugle_thread_t;
#define bugle_thread_self() (GetCurrentThreadId())
#define bugle_thread_raise(sig) ((void) 0)

#define bugle_flockfile(f) ((void) 0)
#define bugle_funlockfile(f) ((void) 0)

#define BUGLE_CONSTRUCTOR(fn) bugle_thread_once_define(static, fn ## _once)
#define BUGLE_RUN_CONSTRUCTOR(fn) bugle_thread_once(fn ## _once, (fn))

#endif /* !BUGLE_PLATFORM_THREADS_H */
