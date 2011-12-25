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

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <bugle/bool.h>
#include <bugle/io.h>
#include <string.h>
#include <bugle/gl/glheaders.h>
#include <budgie/reflect.h>
#include <bugle/apireflect.h>
#include <bugle/gl/gltypes.h>
#include <math.h>

bugle_bool bugle_dump_GLenum(GLenum e, bugle_io_writer *writer)
{
    const char *name = bugle_api_enum_name(e, BUGLE_API_EXTENSION_BLOCK_GL);
    if (!name)
        bugle_io_printf(writer, "<unknown enum 0x%.4x>", (unsigned int) e);
    else
        bugle_io_puts(name, writer);
    return BUGLE_TRUE;
}

bugle_bool bugle_dump_GLerror(GLenum err, bugle_io_writer *writer)
{
    switch (err)
    {
    case GL_NO_ERROR: bugle_io_puts("GL_NO_ERROR", writer); break;
    default: bugle_dump_GLenum(err, writer);
    }
    return BUGLE_TRUE;
}

bugle_bool bugle_dump_GLblendenum(GLenum token, bugle_io_writer *writer)
{
    switch (token)
    {
    case GL_ZERO: bugle_io_puts("GL_ZERO", writer); break;
    case GL_ONE: bugle_io_puts("GL_ONE", writer); break;
    default: bugle_dump_GLenum(token, writer);
    }
    return BUGLE_TRUE;
}

bugle_bool bugle_dump_GLprimitiveenum(GLenum token, bugle_io_writer *writer)
{
    switch (token)
    {
    case GL_POINTS: bugle_io_puts("GL_POINTS", writer); break;
    case GL_LINES: bugle_io_puts("GL_LINES", writer); break;
    case GL_LINE_LOOP: bugle_io_puts("GL_LINE_LOOP", writer); break;
    case GL_LINE_STRIP: bugle_io_puts("GL_LINE_STRIP", writer); break;
    case GL_TRIANGLES: bugle_io_puts("GL_TRIANGLES", writer); break;
    case GL_TRIANGLE_STRIP: bugle_io_puts("GL_TRIANGLE_STRIP", writer); break;
    case GL_TRIANGLE_FAN: bugle_io_puts("GL_TRIANGLE_FAN", writer); break;
#if BUGLE_GLTYPE_GL
    case GL_QUADS: bugle_io_puts("GL_QUADS", writer); break;
    case GL_QUAD_STRIP: bugle_io_puts("GL_QUAD_STRIP", writer); break;
    case GL_POLYGON: bugle_io_puts("GL_POLYGON", writer); break;
#endif
    default: bugle_dump_GLenum(token, writer);
    }
    return BUGLE_TRUE;
    /* Note: doesn't include GL_EXT_geometry_shader4 primitives, but those
     * are unique anyway.
     */
}

bugle_bool bugle_dump_GLcomponentsenum(GLenum token, bugle_io_writer *writer)
{
    if (token >= 1 && token <= 4)
        bugle_io_printf(writer, "%d", (int) token);
    else
        bugle_dump_GLenum(token, writer);
    return BUGLE_TRUE;
}

bugle_bool bugle_dump_GLboolean(GLboolean b, bugle_io_writer *writer)
{
    if (b == 0 || b == 1)
        bugle_io_puts(b ? "GL_TRUE" : "GL_FALSE", writer);
    else
        bugle_io_printf(writer, "(GLboolean) %u", (unsigned int) b);
    return BUGLE_TRUE;
}

bugle_bool bugle_dump_GLpolygonstipple(const GLubyte (*pattern)[4], bugle_io_writer *writer)
{
    GLubyte cur;
    int i, j, k;

    bugle_io_puts("{ ", writer);
    for (i = 0; i < 32; i++)
        for (j = 0; j < 4; j++)
        {
            cur = pattern[i][j];
            for (k = 0; k < 8; k++)
                bugle_io_putc((cur & (1 << (7 - k))) ? '1' : '0', writer);
            bugle_io_putc(' ', writer);
        }
    bugle_io_puts("}", writer);
    return BUGLE_TRUE;
}

bugle_bool bugle_dump_GLxfbattrib(const GLxfbattrib *a, bugle_io_writer *writer)
{
    bugle_io_puts("{ ", writer);
    bugle_dump_GLenum(a->attribute, writer);
    bugle_io_printf(writer, ", %d, %d }", (int) a->components, (int) a->index);
    return BUGLE_TRUE;
}

bugle_bool bugle_dump_GLhalf(GLhalfARB h, bugle_io_writer *writer)
{
    int s = h >> 15;
    int e = (h >> 10) & 0x1f;
    int m = h & 0x3ff;
    float f;
    if (e == 31 && m != 0)
        bugle_io_puts("NaN", writer);
    else if (e == 31)
        bugle_io_puts(s ? "-Inf" : "Inf", writer);
    else
    {
        if (e == 0)
            f = ldexp((double) m, -24);   /* denorm or zero */
        else
            f = ldexp(1024.0 + m, e - 25);
        if (s)
            f = -f;
        bugle_io_printf(writer, "%g", f);
    }
    return BUGLE_TRUE;
}
