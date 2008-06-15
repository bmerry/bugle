/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2006, 2008  Bruce Merry
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

#ifndef BUGLE_GL_GLBEGINEND_H
#define BUGLE_GL_GLBEGINEND_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdbool.h>

/* True if we are in begin/end, OR if there is no current context.
 * glbeginend is required.
 */
bool bugle_gl_in_begin_end(void);

/* Must be used for filter-sets that call bugle_gl_in_begin_end() to guarantee
 * correct answers when intercepting glBegin or glEnd.
 */
void bugle_gl_filter_post_queries_begin_end(const char *name);

void glbeginend_initialise(void);

#endif /* !BUGLE_GL_GLBEGINEND_H */