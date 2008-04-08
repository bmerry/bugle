/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2008  Bruce Merry
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

/* Dumping code that is function-agnostic, and does not depend on being
 * able to call OpenGL (hence suitable for incorporating into gldb).
 */

#ifndef BUGLE_GL_GLTYPES_H
#define BUGLE_GL_GLTYPES_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <GL/gl.h>
#include <stdio.h>
#include <stdbool.h>
#include <bugle/gl/overrides.h>

bool bugle_dump_GLenum(GLenum e, FILE *out);
bool bugle_dump_GLblendenum(GLenum e, FILE *out);
bool bugle_dump_GLprimitiveenum(GLenum e, FILE *out);
bool bugle_dump_GLcomponentsenum(GLenum e, FILE *out);
bool bugle_dump_GLerror(GLenum e, FILE *out);
bool bugle_dump_GLboolean(GLboolean b, FILE *out);
bool bugle_dump_GLpolygonstipple(const GLubyte (*pattern)[4], FILE *out);
bool bugle_dump_GLxfbattrib(const GLxfbattrib *a, FILE *out);

#endif /* !BUGLE_GL_GLTYPES_H */
