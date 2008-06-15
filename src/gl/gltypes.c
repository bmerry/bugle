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
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <bugle/gl/glheaders.h>
#include <budgie/reflect.h>
#include <bugle/apireflect.h>
#include <bugle/gl/gltypes.h>

bool bugle_dump_GLenum(GLenum e, char **buffer, size_t *size)
{
    const char *name = bugle_api_enum_name(e);
    if (!name)
        budgie_snprintf_advance(buffer, size, "<unknown enum 0x%.4x>", (unsigned int) e);
    else
        budgie_snputs_advance(buffer, size, name);
    return true;
}

bool bugle_dump_GLerror(GLenum err, char **buffer, size_t *size)
{
    switch (err)
    {
    case GL_NO_ERROR: budgie_snputs_advance(buffer, size, "GL_NO_ERROR"); break;
    default: bugle_dump_GLenum(err, buffer, size);
    }
    return true;
}

bool bugle_dump_GLblendenum(GLenum token, char **buffer, size_t *size)
{
    switch (token)
    {
    case GL_ZERO: budgie_snputs_advance(buffer, size, "GL_ZERO"); break;
    case GL_ONE: budgie_snputs_advance(buffer, size, "GL_ONE"); break;
    default: bugle_dump_GLenum(token, buffer, size);
    }
    return true;
}

bool bugle_dump_GLprimitiveenum(GLenum token, char **buffer, size_t *size)
{
    switch (token)
    {
    case GL_POINTS: budgie_snputs_advance(buffer, size, "GL_POINTS"); break;
    case GL_LINES: budgie_snputs_advance(buffer, size, "GL_LINES"); break;
    case GL_LINE_LOOP: budgie_snputs_advance(buffer, size, "GL_LINE_LOOP"); break;
    case GL_LINE_STRIP: budgie_snputs_advance(buffer, size, "GL_LINE_STRIP"); break;
    case GL_TRIANGLES: budgie_snputs_advance(buffer, size, "GL_TRIANGLES"); break;
    case GL_TRIANGLE_STRIP: budgie_snputs_advance(buffer, size, "GL_TRIANGLE_STRIP"); break;
    case GL_TRIANGLE_FAN: budgie_snputs_advance(buffer, size, "GL_TRIANGLE_FAN"); break;
#if BUGLE_GLTYPE_GL
    case GL_QUADS: budgie_snputs_advance(buffer, size, "GL_QUADS"); break;
    case GL_QUAD_STRIP: budgie_snputs_advance(buffer, size, "GL_QUAD_STRIP"); break;
    case GL_POLYGON: budgie_snputs_advance(buffer, size, "GL_POLYGON"); break;
#endif
    default: bugle_dump_GLenum(token, buffer, size);
    }
    return true;
    /* Note: doesn't include GL_EXT_geometry_shader4 primitives, but those
     * are unique anyway.
     */
}

bool bugle_dump_GLcomponentsenum(GLenum token, char **buffer, size_t *size)
{
    if (token >= 1 && token <= 4)
        budgie_snprintf_advance(buffer, size, "%d", (int) token);
    else
        bugle_dump_GLenum(token, buffer, size);
    return true;
}

bool bugle_dump_GLboolean(GLboolean b, char **buffer, size_t *size)
{
    if (b == 0 || b == 1)
        budgie_snputs_advance(buffer, size, b ? "GL_TRUE" : "GL_FALSE");
    else
        budgie_snprintf_advance(buffer, size, "(GLboolean) %u", (unsigned int) b);
    return true;
}

bool bugle_dump_GLpolygonstipple(const GLubyte (*pattern)[4], char **buffer, size_t *size)
{
    GLubyte cur;
    int i, j, k;

    budgie_snputs_advance(buffer, size, "{ ");
    for (i = 0; i < 32; i++)
        for (j = 0; j < 4; j++)
        {
            cur = pattern[i][j];
            for (k = 0; k < 8; k++)
                budgie_snputc_advance(buffer, size, (cur & (1 << (7 - k))) ? '1' : '0');
            budgie_snputc_advance(buffer, size, ' ');
        }
    budgie_snputs_advance(buffer, size, "}");
    return true;
}

bool bugle_dump_GLxfbattrib(const GLxfbattrib *a, char **buffer, size_t *size)
{
    budgie_snputs_advance(buffer, size, "{ ");
    bugle_dump_GLenum(a->attribute, buffer, size);
    budgie_snprintf_advance(buffer, size, ", %d, %d }", (int) a->components, (int) a->index);
    return true;
}
