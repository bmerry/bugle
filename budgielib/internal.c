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
#include <budgie/reflect.h>
#include "budgielib/defines.h"
#include "budgielib/internal.h"

int _budgie_function_count = FUNCTION_COUNT;
int _budgie_group_count = GROUP_COUNT;
int _budgie_type_count = TYPE_COUNT;

void _budgie_dump_bitfield(unsigned int value, char **buffer, size_t *size,
                           const bitfield_pair *tags, int count)
{
    bool first = true;
    int i;
    for (i = 0; i < count; i++)
        if (value & tags[i].value)
        {
            if (!first) budgie_snputs_advance(buffer, size, " | "); else first = false;
            budgie_snputs_advance(buffer, size, tags[i].name);
            value &= ~tags[i].value;
        }
    if (value)
    {
        if (!first) budgie_snputs_advance(buffer, size, " | ");
        budgie_snprintf_advance(buffer, size, "%08x", value);
    }
}

bool budgie_dump_string(const char *value, char **buffer, size_t *size)
{
    /* FIXME: handle illegal dereferences */
    if (value == NULL) budgie_snputs_advance(buffer, size, "NULL");
    else
    {
        budgie_snputc_advance(buffer, size, '"');
        while (value[0])
        {
            switch (value[0])
            {
            case '"': budgie_snputs_advance(buffer, size, "\\\""); break;
            case '\\': budgie_snputs_advance(buffer, size, "\\\\"); break;
            case '\n': budgie_snputs_advance(buffer, size, "\\n"); break;
            case '\r': budgie_snputs_advance(buffer, size, "\\r"); break;
            default:
                if (iscntrl(value[0]))
                    budgie_snprintf_advance(buffer, size, "\\%03o", (int) value[0]);
                else
                    budgie_snputc_advance(buffer, size, value[0]);
            }
            value++;
        }
        budgie_snputc_advance(buffer, size, '"');
    }
    return true;
}

bool budgie_dump_string_length(const char *value, size_t length, char **buffer, size_t *size)
{
    size_t i;
    /* FIXME: handle illegal dereferences */
    if (value == NULL) budgie_snputs_advance(buffer, size, "NULL");
    else
    {
        budgie_snputc_advance(buffer, size, '"');
        for (i = 0; i < length; i++)
        {
            switch (value[0])
            {
            case '"': budgie_snputs_advance(buffer, size, "\\\""); break;
            case '\\': budgie_snputs_advance(buffer, size, "\\\\"); break;
            case '\n': budgie_snputs_advance(buffer, size, "\\n"); break;
            case '\r': budgie_snputs_advance(buffer, size, "\\r"); break;
            case '\t': budgie_snputs_advance(buffer, size, "\\t"); break;
            default:
                if (iscntrl(value[0]))
                    budgie_snprintf_advance(buffer, size, "\\%03o", (int) value[0]);
                else
                    budgie_snputc_advance(buffer, size, value[0]);
            }
            value++;
        }
        budgie_snputc_advance(buffer, size, '"');
    }
    return true;
}

int budgie_count_string(const char *value)
{
    /* FIXME: handle illegal dereferences */
    const char *str = (const char *) value;
    if (str == NULL) return 0;
    else return strlen(str) + 1;
}
