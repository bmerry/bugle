/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2008, 2012  Bruce Merry
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

#include <bugle/porting.h>
#include <bugle/gl/glheaders.h>
#include <bugle/bool.h>
#include <bugle/io.h>
#include <bugle/gl/overrides.h>

#ifdef __cplusplus
extern "C" {
#endif

bugle_bool bugle_dump_GLenum(GLenum e, bugle_io_writer *writer);
bugle_bool bugle_dump_GLblendenum(GLenum e, bugle_io_writer *writer);
bugle_bool bugle_dump_GLprimitiveenum(GLenum e, bugle_io_writer *writer);
bugle_bool bugle_dump_GLcomponentsenum(GLenum e, bugle_io_writer *writer);
bugle_bool bugle_dump_GLerror(GLenum e, bugle_io_writer *writer);
bugle_bool bugle_dump_GLboolean(GLboolean b, bugle_io_writer *writer);
bugle_bool bugle_dump_GLpolygonstipple(const GLubyte (*pattern)[4], bugle_io_writer *writer);
bugle_bool bugle_dump_GLxfbattrib(const GLxfbattrib *a, bugle_io_writer *writer);
/* The parameter is passed as GLushort because ES2 doesn't have the typedef for GLhalf */
bugle_bool bugle_dump_GLhalf(GLushort h, bugle_io_writer *writer);

#ifdef __cplusplus
}
#endif

#endif /* !BUGLE_GL_GLTYPES_H */
