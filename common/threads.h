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

/* This file implements a basic threading abstraction layer. It is not
 * a full threading layer with support for creating and managing threads,
 * just features like mutexes to make it possible to write re-entrant code.
 * The wrapper function names are modelled on pthreads, with "pthread"
 * replaced by "bugle_thread".
 *
 * At present it supports
 * - mutexes (lock and unlock only, no trylock or timedlock
 * - thread-specific data
 * - run-onces
 */

#ifndef BUGLE_COMMON_THREADS_H
#define BUGLE_COMMON_THREADS_H
#if HAVE_CONFIG_H
# include <config.h>
#endif

/* These values don't matter as long as they are distinct, since the
 * preprocessor only does == on integers.
 */
#define BUGLE_THREADS_SINGLE 0
#define BUGLE_THREADS_PTHREADS 1

#if (THREADS == BUGLE_THREADS_PTHREADS)

#include <pthread.h>

/* Types */
typedef pthread_mutex_t bugle_thread_mutex_t;
typedef pthread_key_t bugle_thread_key_t;
typedef pthread_once_t bugle_thread_once_t;

/* Constants */
#define BUGLE_THREAD_ONCE_INIT PTHREAD_ONCE_INIT
#define BUGLE_THREAD_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER

/* Functions */
static inline int bugle_thread_mutex_init(bugle_thread_mutex_t *mutex, const void *mutexattr)
{ return pthread_mutex_init(mutex, mutexattr); }
static inline int bugle_thread_mutex_lock(bugle_thread_mutex_t *mutex)
{ return pthread_mutex_lock(mutex); }
static inline int bugle_thread_mutex_unlock(bugle_thread_mutex_t *mutex)
{ return pthread_mutex_unlock(mutex); }
static inline int bugle_thread_mutex_destroy(bugle_thread_mutex_t *mutex)
{ return pthread_mutex_destroy(mutex); }

static inline int bugle_thread_key_create(bugle_thread_key_t *key,
                                          void (*destr_function)(void *))
{ return pthread_key_create(key, destr_function); }
static inline int bugle_thread_key_delete(bugle_thread_key_t key)
{ return pthread_key_delete(key); }
static inline int bugle_thread_setspecific(bugle_thread_key_t key, const void *ptr)
{ return pthread_setspecific(key, ptr); }
static inline void *bugle_thread_getspecific(bugle_thread_key_t key)
{ return pthread_getspecific(key); }

static inline int bugle_thread_once(bugle_thread_once_t *once_control, void (*init_routine)(void))
{ return pthread_once(once_control, init_routine); }

/* Not a pthread function, but a thread-safe wrapper around raise(3) */
static inline int bugle_thread_raise(int sig)
{
    return pthread_kill(pthread_self(), sig);
}

#endif /* THREADS == BUGLE_THREADS_PTHREADS */

#if (THREADS == BUGLE_THREADS_SINGLE)

#include <stdlib.h>
#include <signal.h>
#include "common/safemem.h"

/* Types */
typedef int bugle_thread_mutex_t;
typedef int bugle_thread_once_t;
typedef struct bugle_thread_key_s
{
    void *value;
} *bugle_thread_key_t;

/* Constants */
#define BUGLE_THREAD_ONCE_INIT 0
#define BUGLE_THREAD_MUTEX_INITIALIZER 0

/* Functions */
static inline int bugle_thread_mutex_init(bugle_thread_mutex_t *mutex, const void *mutexattr)
{ return 0; }
static inline int bugle_thread_mutex_lock(bugle_thread_mutex_t *mutex)
{ return 0; }
static inline int bugle_thread_mutex_unlock(bugle_thread_mutex_t *mutex)
{ return 0; }
static inline int bugle_thread_mutex_destroy(bugle_thread_mutex_t *mutex)
{ return 0; }

static inline int bugle_thread_key_create(bugle_thread_key_t *key,
                                          void (*destr_function)(void *))
{
    /* Note: pthreads defines the destructor function to be called on
     * pthread_exit or thread cancellation, neither of which can happen
     * in a single-threaded model. So we ignore the destructor.
     */
    *key = (bugle_thread_key_t) bugle_malloc(sizeof(struct bugle_thread_key_s));
    (*key)->value = NULL;
    return 0;
}

static inline int bugle_thread_key_delete(bugle_thread_key_t key)
{
    free(key);
    return 0;
}

static inline int bugle_thread_setspecific(bugle_thread_key_t key,
                                           const void *pointer)
{
    key->value = (void *) pointer;
    return 0;
}

static inline void *bugle_thread_getspecific(bugle_thread_key_t key)
{
    return key->value;
}

static inline int bugle_thread_once(bugle_thread_once_t *once_control,
                                    void (*init_routine)(void))
{
    if (!*once_control)
    {
        *once_control = 1;
        (*init_routine)();
    }
    return 0;
}

static inline int bugle_thread_raise(int sig)
{
    return raise(sig);
}

#endif /* THREADS == BUGLE_THREADS_SINGLE */

#endif /* !BUGLE_COMMON_THREADS_H */
