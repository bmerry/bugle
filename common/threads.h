/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2007  Bruce Merry
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

/* Most of the thread management is handled by gnulib now. Note that
 * bugle never creates or destroys threads, it just needs some thread-safe
 * primitives like mutexes.
 *
 * What remains here are oddities not covered by gnulib:
 * - thread ID
 * - pthread_kill wrapper
 * - macros for emulating constructors
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
# include <pthread.h>
#endif
#include <signal.h>
#include <stdlib.h>

/* Functions */
static inline unsigned long bugle_thread_self(void)
{
#if BUGLE_PTHREAD_T_INTEGRAL && THREADS == BUGLE_THREADS_PTHREADS
    return (unsigned long) pthread_self();
#else
    return (unsigned long) 0;
#endif
}

/* Not a pthread function, but a thread-safe wrapper around raise(3) */
static inline int bugle_thread_raise(int sig)
{
#if HAVE_PTHREAD_KILL && THREADS == BUGLE_THREADS_PTHREADS
    return pthread_kill(pthread_self(), sig);
#else
    return raise(sig);
#endif
}

/*** Higher-level stuff that doesn't depend on the threading implementation ***/

/* This initialisation mechanism is only valid for static constructor functions.
 * Any code that depends on the initialisation being complete must call
 * BUGLE_RUN_CONSTRUCTOR on the constructor first.
 */
#if BUGLE_GCC_HAVE_CONSTRUCTOR_ATTRIBUTE && !DEBUG_CONSTRUCTOR
# define BUGLE_CONSTRUCTOR(fn) static void fn(void) BUGLE_GCC_CONSTRUCTOR_ATTRIBUTE
# define BUGLE_RUN_CONSTRUCTOR(fn) ((void) 0)
#else
# define BUGLE_CONSTRUCTOR(fn) static bugle_thread_once_t fn ## _once = BUGLE_THREAD_ONCE_INIT
# define BUGLE_RUN_CONSTRUCTOR(fn) (bugle_thread_once(&(fn ## _once), (fn)))
#endif

#endif /* !BUGLE_COMMON_THREADS_H */
