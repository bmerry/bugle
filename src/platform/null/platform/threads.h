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
 * - TLS destructors are not guaranteed to be called
 */

#ifndef BUGLE_PLATFORM_THREADS_H
#define BUGLE_PLATFORM_THREADS_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stddef.h>

typedef int bugle_thread_once_t;

#define BUGLE_THREAD_ONCE_INIT 0
#define bugle_thread_once(name, function) ((void) 0)

typedef int bugle_thread_lock_t;
#define bugle_thread_lock_init(name) ((void) 0)
#define bugle_thread_lock_destroy(name) ((void) 0)
#define bugle_thread_lock_lock(name) ((void) 0)
#define bugle_thread_lock_unlock(name) ((void) 0)

typedef int bugle_thread_rwlock_t;
# define bugle_thread_rwlock_init(name) ((void) 0)
# define bugle_thread_rwlock_destroy(name) ((void) 0)
# define bugle_thread_rwlock_rdlock(name) ((void) 0)
# define bugle_thread_rwlock_wrlock(name) ((void) 0)
# define bugle_thread_rwlock_unlock(name) ((void) 0)

typedef int bugle_thread_key_t;
#define bugle_thread_key_create(name, destructor) ((void) 0)
#define bugle_thread_getspecific(key) (NULL)
#define bugle_thread_setspecific(key, value) ((void) 0)
#define bugle_thread_key_delete(key) ((void) 0)

typedef int bugle_thread_t;
#define bugle_thread_self() (0UL)
#define bugle_thread_raise(sig) ((void) 0)

#define bugle_flockfile(f) ((void) 0)
#define bugle_funlockfile(f) ((void) 0)

#define BUGLE_CONSTRUCTOR(fn) 
#define BUGLE_RUN_CONSTRUCTOR(fn) fn()

#endif /* !BUGLE_PLATFORM_THREADS_H */
