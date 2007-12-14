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
#include <signal.h>
#include <stdlib.h>

#if USE_POSIX_THREADS

#include <pthread.h>
#include <config.h>

#if PTHREAD_IN_USE_DETECTION_HARD
#define pthread_in_use() glthread_in_use()
/* The implementation is in lock.c */
extern int glthread_in_use(void);
#endif

/* Based on the setup of lock.h from gnulib */
#if USE_POSIX_THREADS_WEAK

#pragma weak pthread_kill
#ifndef pthread_self
# pragma weak pthread_self
#endif

#if !PTHREAD_IN_USE_DETECTION_HARD
# pragma weak pthread_cancel
# define pthread_in_use() (pthread_cancel != NULL)
#endif

#else /* USE_POSIX_THREADS_WEAK */

#if !PTHREAD_IN_USE_DETECTION_HARD
# define pthread_in_use() (1)
#endif

#endif /* !USE_POSIX_THREADS_WEAK */

#if BUGLE_PTHREAD_T_INTEGRAL
# define bugle_thread_self() (pthread_in_use() ? (unsigned long) pthread_self() : 0UL)
#endif
#define bugle_pthread_raise(sig) (pthread_in_use() ? pthread_kill(pthread_self(), (sig)) : raise((sig)))

#endif /* !USE_POSIX_THREADS */

#if USE_PTH_THREADS || USE_SOLARIS_THREADS || USE_WIN32_THREADS
# warning "Threading model not totally supported; some features might not work"
#endif

#ifndef bugle_thread_self
# define bugle_thread_self() (0)
#endif

#ifndef bugle_thread_raise
# define bugle_thread_raise(sig) (raise((sig)))
#endif

/*** Higher-level stuff that doesn't depend on the threading implementation ***/

/* This initialisation mechanism is only valid for static constructor functions.
 * Any code that depends on the initialisation being complete must call
 * BUGLE_RUN_CONSTRUCTOR on the constructor first.
 */
#if BUGLE_GCC_HAVE_CONSTRUCTOR_ATTRIBUTE && !DEBUG_CONSTRUCTOR
# define BUGLE_CONSTRUCTOR(fn) static void fn(void) BUGLE_GCC_CONSTRUCTOR_ATTRIBUTE
# define BUGLE_RUN_CONSTRUCTOR(fn) ((void) 0)
#else
# define BUGLE_CONSTRUCTOR(fn) gl_once_define(static, fn ## _once)
# define BUGLE_RUN_CONSTRUCTOR(fn) (gl_once(fn ## _once, (fn)))
#endif

#endif /* !BUGLE_COMMON_THREADS_H */
