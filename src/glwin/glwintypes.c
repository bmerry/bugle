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
#include <bugle/io.h>
#include <bugle/glwin/glwintypes.h>
#include <budgie/reflect.h>

bugle_bool bugle_dump_glwin_drawable(glwin_drawable d, bugle_io_writer *writer)
{
    bugle_io_printf(writer, "0x%08lx", (unsigned long) d);
    return BUGLE_TRUE;
}

bugle_bool bugle_dump_glwin_bool(glwin_bool b, bugle_io_writer *writer)
{
    if (b == 0 || b == 1)
        bugle_io_puts(b ? GLWIN_BOOL_TRUE : GLWIN_BOOL_FALSE, writer);
    else
        bugle_io_printf(writer, "(" GLWIN_BOOL_TYPE ") %u", (unsigned int) b);
    return BUGLE_TRUE;
}

bugle_bool bugle_dump_glwin_enum(glwin_enum e, bugle_io_writer *writer)
{
    const char *name = bugle_api_enum_name(e, BUGLE_API_EXTENSION_BLOCK_GLWIN);
    if (!name)
        bugle_io_printf(writer, "<unknown enum 0x%.4x>", (unsigned int) e);
    else
        bugle_io_puts(name, writer);
    return BUGLE_TRUE;
}

int bugle_count_glwin_attributes(const glwin_attrib *attribs, glwin_attrib terminator)
{
    int i = 0;
    if (attribs == NULL) return 0;
    while (attribs[i] != terminator) i += 2;
    return i + 1;
}

bugle_bool bugle_dump_glwin_attributes(const glwin_attrib *attribs, glwin_attrib terminator, bugle_io_writer *writer)
{
    int i = 0;

    if (!attribs)
        return BUGLE_FALSE;   /* Fall back on default dumping for NULL */

    bugle_io_printf(writer, "%p -> { ", attribs);
    while (attribs[i] != terminator)
    {
        bugle_dump_glwin_enum(attribs[i], writer);
        bugle_io_printf(writer, ", %d, ", (int) attribs[i + 1]);
        i += 2;
    }
    bugle_dump_glwin_enum(terminator, writer);
    bugle_io_puts(" }", writer);
    return BUGLE_TRUE;
}
