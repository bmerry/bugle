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
#include <GL/glx.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <bugle/glx/glxtypes.h>

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
