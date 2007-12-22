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

#ifndef BUDGIELIB_IOUTILS_H
#define BUDGIELIB_IOUTILS_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdio.h>

/* Calls "call" with a FILE * and "data". The data that "call" writes into
 * the file is returned in as a malloc'ed string.
 * FIXME: error checking.
 * FIXME: is ftell portable on _binary_ streams (which tmpfile is)?
 */
char *budgie_string_io(void (*call)(FILE *, void *), void *data);

void budgie_make_indent(int indent, FILE *out);

#endif /* !BUDGIELIB_IOUTILS_H */
