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

/* Wrappers around GCC __attributes__. The BUGLE_HAVE_ATTRIBUTE_*
 * tokens are defined in the internal config.h. The checks against
 * the GCC version are to allow the attributes to apply even when
 * third-party users compile against the headers.
 */

#ifndef BUGLE_ATTRIBUTES_H
#define BUGLE_ATTRIBUTES_H

#if BUGLE_HAVE_ATTRIBUTE_FORMAT_PRINTF || (__GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR > 4))
# define BUGLE_ATTRIBUTE_FORMAT_PRINTF(a, b) __attribute__((format(__printf__, a, b)))
#else
# define BUGLE_ATTRIBUTE_FORMAT_PRINTF(a, b)
#endif

#if BUGLE_HAVE_ATTRIBUTE_NORETURN || (__GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR > 4))
# define BUGLE_ATTRIBUTE_NORETURN __attribute__((__noreturn__))
#else
# define BUGLE_ATTRIBUTE_NORETURN
#endif

#if BUGLE_HAVE_ATTRIBUTE_MALLOC || (__GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR >= 96))
# define BUGLE_ATTRIBUTE_MALLOC __attribute__((__malloc__))
#else
# define BUGLE_ATTRIBUTE_MALLOC
#endif

#endif /* !BUGLE_ATTRIBUTES_H */
