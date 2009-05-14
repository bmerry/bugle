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
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <GL/glx.h>
#include <budgie/reflect.h>
#include <bugle/glwin/glwintypes.h>
#include <bugle/glx/glxdump.h>
#include <bugle/bool.h>

int bugle_count_glXChooseVisual_attributes(const int *attr)
{
    int i = 0;
    if (!attr) return 0;
    while (attr[i])
    {
        switch (attr[i])
        {
        case GLX_USE_GL:
        case GLX_RGBA:
        case GLX_DOUBLEBUFFER:
        case GLX_STEREO:
            i++;
            break;
        default:
            i += 2;
        }
    }
    return i + 1;
}

bugle_bool bugle_dump_glXChooseVisual_attributes(const int *attr, char **buffer, size_t *size)
{
    int i = 0;

    if (!attr)
        return BUGLE_FALSE;   /* Fall back on default dumping for NULL */

    budgie_snprintf_advance(buffer, size, "%p -> { ", attr);
    while (attr[i])
    {
        switch (attr[i])
        {
        case GLX_USE_GL:
        case GLX_RGBA:
        case GLX_DOUBLEBUFFER:
        case GLX_STEREO:
            bugle_dump_glwin_enum(attr[i], buffer, size);
            budgie_snputs_advance(buffer, size, ", ");
            i++;
            break;
        default:
            bugle_dump_glwin_enum(attr[i], buffer, size);
            budgie_snprintf_advance(buffer, size, ", %d, ", attr[i + 1]);
            i += 2;
        }
    }
    budgie_snputs_advance(buffer, size, "None }");
    return BUGLE_TRUE;
}
