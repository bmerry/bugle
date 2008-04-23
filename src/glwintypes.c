/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2008  Bruce Merry
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
#include <bugle/glwintypes.h>

bool bugle_dump_glwin_drawable(glwin_drawable d, FILE *out)
{
    fprintf(out, "0x%08lx", (unsigned long) d);
    return true;
}

bool bugle_dump_glwin_bool(glwin_bool b, FILE *out)
{
    if (b == 0 || b == 1)
        fputs(b ? GLWIN_BOOL_TRUE : GLWIN_BOOL_FALSE, out);
    else
        fprintf(out, "(" GLWIN_BOOL_TYPE ") %u", (unsigned int) b);
    return true;
}