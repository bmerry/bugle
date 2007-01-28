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

#ifndef _GNU_SOURCE
# define _GNU_SOURCE /* For open_memstream */
#endif
#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdlib.h>
#include <stddef.h>
#include "ioutils.h"

void budgie_make_indent(int indent, FILE *out)
{
    int i;
    for (i = 0; i < indent; i++)
        fputc(' ', out);
}

char *budgie_string_io(void (*call)(FILE *, void *), void *data)
{
    FILE *f;
    size_t size;
    char *buffer;

#if HAVE_OPEN_MEMSTREAM
    f = open_memstream(&buffer, &size);
    (*call)(f, data);
    fclose(f);
#else
    f = tmpfile();
    (*call)(f, data);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    buffer = malloc(size + 1);
    fread(buffer, 1, size, f);
    buffer[size] = '\0';
    fclose(f);
#endif

    return buffer;
}

