/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2008-2009  Bruce Merry
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
#include <bugle/porting.h>
#include <bugle/apireflect.h>
#include <bugle/glwin/glwintypes.h>
#include <budgie/reflect.h>

bugle_bool bugle_dump_glwin_drawable(glwin_drawable d, char **buffer, size_t *size)
{
    budgie_snprintf_advance(buffer, size, "0x%08lx", (unsigned long) d);
    return BUGLE_TRUE;
}

bugle_bool bugle_dump_glwin_bool(glwin_bool b, char **buffer, size_t *size)
{
    if (b == 0 || b == 1)
        budgie_snputs_advance(buffer, size, b ? GLWIN_BOOL_TRUE : GLWIN_BOOL_FALSE);
    else
        budgie_snprintf_advance(buffer, size, "(" GLWIN_BOOL_TYPE ") %u", (unsigned int) b);
    return BUGLE_TRUE;
}

bugle_bool bugle_dump_glwin_enum(glwin_enum e, char **buffer, size_t *size)
{
    const char *name = bugle_api_enum_name(e, BUGLE_API_EXTENSION_BLOCK_GLWIN);
    if (!name)
        budgie_snprintf_advance(buffer, size, "<unknown enum 0x%.4x>", (unsigned int) e);
    else
        budgie_snputs_advance(buffer, size, name);
    return BUGLE_TRUE;
}

int bugle_count_glwin_attributes(const glwin_attrib *attribs, glwin_attrib terminator)
{
    int i = 0;
    if (attribs == NULL) return 0;
    while (attribs[i] != terminator) i += 2;
    return i + 1;
}

bugle_bool bugle_dump_glwin_attributes(const glwin_attrib *attribs, glwin_attrib terminator, char **buffer, size_t *size)
{
    int i = 0;

    if (!attribs)
        return BUGLE_FALSE;   /* Fall back on default dumping for NULL */

    budgie_snprintf_advance(buffer, size, "%p -> { ", attribs);
    while (attribs[i] != terminator)
    {
        bugle_dump_glwin_enum(attribs[i], buffer, size);
        budgie_snprintf_advance(buffer, size, ", %d, ", (int) attribs[i + 1]);
        i += 2;
    }
    bugle_dump_glwin_enum(terminator, buffer, size);
    budgie_snputs_advance(buffer, size, " }");
    return BUGLE_TRUE;
}
