/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004  Bruce Merry
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#ifndef BUGLE_SRC_GLDUMP_H
#define BUGLE_SRC_GLDUMP_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <GL/gl.h>
#include "budgieutils.h"

int count_gl(budgie_function func, GLenum token);
GLenum gl_token_to_enum(const char *name);
const char *gl_enum_to_token(GLenum e);

bool dump_GLenum(const void *value, int count, FILE *out);
bool dump_GLalternateenum(const void *value, int count, FILE *out);
bool dump_GLerror(const void *value, int count, FILE *out);
bool dump_GLboolean(const void *value, int count, FILE *out);
bool dump_convert(const generic_function_call *gcall,
                  int arg,
                  const void *value,
                  FILE *out);

#endif /* !BUGLE_SRC_GLDUMP_H */
