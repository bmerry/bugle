/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2007, 2009  Bruce Merry
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

/* Refer to the DocBook manual for documentation on these functions. */

#ifndef BUGLE_PLATFORM_THREADS_H
#define BUGLE_PLATFORM_THREADS_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "platform_config.h"
#include <signal.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <bugle/porting.h>
#include <pthread.h>

typedef pthread_once_t bugle_thread_once_t;

#define BUGLE_THREAD_ONCE_INIT PTHREAD_ONCE_INIT
#define bugle_thread_once(once, function) pthread_once(once, function)

typedef pthread_mutex_t bugle_thread_lock_t;
#define bugle_thread_lock_init(lock) pthread_mutex_init(lock, NULL)
#define bugle_thread_lock_destroy(lock) pthread_mutex_destroy(lock)
#define bugle_thread_lock_lock(lock) pthread_mutex_lock(lock)
#define bugle_thread_lock_unlock(lock) pthread_mutex_unlock(lock)

typedef pthread_rwlock_t bugle_thread_rwlock_t;
# define bugle_thread_rwlock_init(rwlock) pthread_rwlock_init(rwlock, NULL)
# define bugle_thread_rwlock_destroy(rwlock) pthread_rwlock_destroy(rwlock)
# define bugle_thread_rwlock_rdlock(rwlock) pthread_rwlock_rdlock(rwlock)
# define bugle_thread_rwlock_wrlock(rwlock) pthread_rwlock_wrlock(rwlock)
# define bugle_thread_rwlock_unlock(rwlock) pthread_rwlock_unlock(rwlock)

typedef pthread_key_t bugle_thread_key_t;
#define bugle_thread_key_create(key, destructor) \
    pthread_key_create(key, destructor)
#define bugle_thread_getspecific(key) pthread_getspecific(key)
#define bugle_thread_setspecific(key, value) pthread_setspecific(key, value)
#define bugle_thread_key_delete(key) pthread_key_delete(key)

typedef pthread_t bugle_thread_t;
#define bugle_thread_self() pthread_self()
#define bugle_thread_raise(sig) pthread_kill(pthread_self(), (sig))

#if _POSIX_THREAD_SAFE_FUNCTIONS > 0
# define bugle_flockfile(f) flockfile(f)
# define bugle_funlockfile(f) funlockfile(f)
#else
# define bugle_flockfile(f) ((void) 0)
# define bugle_funlockfile(f) ((void) 0)
#endif

/*** Higher-level stuff that doesn't depend on the threading implementation ***/
/* TODO move it out of here and into a shared header
 * TODO remove reference to LTDL
 */

/* This initialisation mechanism is only valid for static constructor functions.
 * Any code that depends on the initialisation being complete must call
 * BUGLE_RUN_CONSTRUCTOR on the constructor first.
 *
 * This is disabled on WIN32 because it does DLL initialisation before it is
 * safe to call LoadLibrary.
 */
#if BUGLE_HAVE_ATTRIBUTE_CONSTRUCTOR && !DEBUG_CONSTRUCTOR && BUGLE_BINFMT_CONSTRUCTOR_LTDL
# define BUGLE_CONSTRUCTOR(fn) static void fn(void) __attribute__((constructor))
# define BUGLE_RUN_CONSTRUCTOR(fn) ((void) 0)
#else
# define BUGLE_CONSTRUCTOR(fn) static bugle_thread_once_t fn ## _once = BUGLE_THREAD_ONCE_INIT
# define BUGLE_RUN_CONSTRUCTOR(fn) bugle_thread_once(&(fn ## _once), (fn))
#endif

#endif /* !BUGLE_PLATFORM_THREADS_H */
