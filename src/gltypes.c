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

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <GL/gl.h>
#include <GL/glx.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <bugle/gltypes.h>
#include <bugle/glreflect.h>

bool bugle_dump_GLenum(GLenum e, FILE *out)
{
    const char *name = bugle_gl_enum_name(e);
    if (!name)
        fprintf(out, "<unknown enum 0x%.4x>", (unsigned int) e);
    else
        fputs(name, out);
    return true;
}

bool bugle_dump_GLerror(GLenum err, FILE *out)
{
    switch (err)
    {
    case GL_NO_ERROR: fputs("GL_NO_ERROR", out); break;
    default: bugle_dump_GLenum(err, out);
    }
    return true;
}

bool bugle_dump_GLblendenum(GLenum token, FILE *out)
{
    switch (token)
    {
    case GL_ZERO: fputs("GL_ZERO", out); break;
    case GL_ONE: fputs("GL_ONE", out); break;
    default: bugle_dump_GLenum(token, out);
    }
    return true;
}

bool bugle_dump_GLprimitiveenum(GLenum token, FILE *out)
{
    switch (token)
    {
    case GL_POINTS: fputs("GL_POINTS", out); break;
    case GL_LINES: fputs("GL_LINES", out); break;
    case GL_LINE_LOOP: fputs("GL_LINE_LOOP", out); break;
    case GL_LINE_STRIP: fputs("GL_LINE_STRIP", out); break;
    case GL_TRIANGLES: fputs("GL_TRIANGLES", out); break;
    case GL_TRIANGLE_STRIP: fputs("GL_TRIANGLE_STRIP", out); break;
    case GL_TRIANGLE_FAN: fputs("GL_TRIANGLE_FAN", out); break;
    case GL_QUADS: fputs("GL_QUADS", out); break;
    case GL_POLYGON: fputs("GL_POLYGON", out); break;
    default: bugle_dump_GLenum(token, out);
    }
    return true;
    /* Note: doesn't include GL_EXT_geometry_shader4 primitives, but those
     * are unique anyway.
     */
}

bool bugle_dump_GLcomponentsenum(GLenum token, FILE *out)
{
    if (token >= 1 && token <= 4)
        fprintf(out, "%d", (int) token);
    else
        bugle_dump_GLenum(token,  out);
    return true;
}

bool bugle_dump_GLboolean(GLboolean b, FILE *out)
{
    if (b == 0 || b == 1)
        fputs(b ? "GL_TRUE" : "GL_FALSE", out);
    else
        fprintf(out, "(GLboolean) %u", (unsigned int) b);
    return true;
}

bool bugle_dump_Bool(Bool b, FILE *out)
{
    if (b == 0 || b == 1)
        fputs(b ? "True" : "False", out);
    else
        fprintf(out, "(Bool) %u", (unsigned int) b);
    return true;
}

bool bugle_dump_GLXDrawable(GLXDrawable d, FILE *out)
{
    fprintf(out, "0x%08x", (unsigned int) d);
    return true;
}

bool bugle_dump_GLpolygonstipple(const GLubyte (*pattern)[4], FILE *out)
{
    GLubyte cur;
    int i, j, k;

    fputs("{ ", out);
    for (i = 0; i < 32; i++)
        for (j = 0; j < 4; j++)
        {
            cur = pattern[i][j];
            for (k = 0; k < 8; k++)
                fputc((cur & (1 << (7 - k))) ? '1' : '0', out);
            fputc(' ', out);
        }
    fputs("}", out);
    return true;
}

bool bugle_dump_GLxfbattrib(const GLxfbattrib *a, FILE *out)
{
    fputs("{ ", out);
    bugle_dump_GLenum(a->attribute, out);
    fprintf(out, ", %d, %d }", (int) a->components, (int) a->index);
    return true;
}
