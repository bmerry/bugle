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
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stddef.h>
#include <bugle/io.h>
#include <budgie/reflect.h>
#include "budgielib/defines.h"
#include "budgielib/internal.h"

int _budgie_function_count = FUNCTION_COUNT;
int _budgie_group_count = GROUP_COUNT;
int _budgie_type_count = TYPE_COUNT;

void _budgie_dump_bitfield(unsigned int value, bugle_io_writer *writer,
                           const bitfield_pair *tags, int count)
{
    bugle_bool first = BUGLE_TRUE;
    int i;
    if (value == 0)
    {
        bugle_io_puts("0", writer);
    }
    else
    {
        for (i = 0; i < count; i++)
            if (value & tags[i].value)
            {
                if (!first) bugle_io_puts(" | ", writer); else first = BUGLE_FALSE;
                bugle_io_puts(tags[i].name, writer);
                value &= ~tags[i].value;
            }
        if (value)
        {
            if (!first) bugle_io_puts(" | ", writer);
            bugle_io_printf(writer, "%08x", value);
        }
    }
}

bugle_bool budgie_dump_string(const char *value, bugle_io_writer *writer)
{
    /* FIXME: handle illegal dereferences */
    if (value == NULL) bugle_io_puts("NULL", writer);
    else
    {
        bugle_io_putc('"', writer);
        while (value[0])
        {
            switch (value[0])
            {
            case '"': bugle_io_puts("\\\"", writer); break;
            case '\\': bugle_io_puts("\\\\", writer); break;
            case '\n': bugle_io_puts("\\n", writer); break;
            case '\r': bugle_io_puts("\\r", writer); break;
            default:
                if (iscntrl(value[0]))
                    bugle_io_printf(writer, "\\%03o", (int) value[0]);
                else
                    bugle_io_putc(value[0], writer);
            }
            value++;
        }
        bugle_io_putc('"', writer);
    }
    return BUGLE_TRUE;
}

bugle_bool budgie_dump_string_length(const char *value, size_t length, bugle_io_writer *writer)
{
    size_t i;
    /* FIXME: handle illegal dereferences */
    if (value == NULL) bugle_io_puts("NULL", writer);
    else
    {
        bugle_io_putc('"', writer);
        for (i = 0; i < length; i++)
        {
            switch (value[0])
            {
            case '"': bugle_io_puts("\\\"", writer); break;
            case '\\': bugle_io_puts("\\\\", writer); break;
            case '\n': bugle_io_puts("\\n", writer); break;
            case '\r': bugle_io_puts("\\r", writer); break;
            case '\t': bugle_io_puts("\\t", writer); break;
            default:
                if (iscntrl(value[0]))
                    bugle_io_printf(writer, "\\%03o", (int) value[0]);
                else
                    bugle_io_putc(value[0], writer);
            }
            value++;
        }
        bugle_io_putc('"', writer);
    }
    return BUGLE_TRUE;
}

int budgie_count_string(const char *value)
{
    /* FIXME: handle illegal dereferences */
    const char *str = (const char *) value;
    if (str == NULL) return 0;
    else return strlen(str) + 1;
}
