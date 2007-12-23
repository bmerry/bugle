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
#include "budgielib/defines.h"
#include "budgielib/internal.h"

int _budgie_function_count = FUNCTION_COUNT;
int _budgie_group_count = GROUP_COUNT;
int _budgie_type_count = TYPE_COUNT;

void _budgie_dump_bitfield(unsigned int value, FILE *out,
                           const bitfield_pair *tags, int count)
{
    bool first = true;
    int i;
    for (i = 0; i < count; i++)
        if (value & tags[i].value)
        {
            if (!first) fputs(" | ", out); else first = false;
            fputs(tags[i].name, out);
            value &= ~tags[i].value;
        }
    if (value)
    {
        if (!first) fputs(" | ", out);
        fprintf(out, "%08x", value);
    }
}

bool budgie_dump_string(const char *value, FILE *out)
{
    /* FIXME: handle illegal dereferences */
    if (value == NULL) fputs("NULL", out);
    else
    {
        fputc('"', out);
        while (value[0])
        {
            switch (value[0])
            {
            case '"': fputs("\\\"", out); break;
            case '\\': fputs("\\\\", out); break;
            case '\n': fputs("\\n", out); break;
            case '\r': fputs("\\r", out); break;
            default:
                if (iscntrl(value[0]))
                    fprintf(out, "\\%03o", (int) value[0]);
                else
                    fputc(value[0], out);
            }
            value++;
        }
        fputc('"', out);
    }
    return true;
}

bool budgie_dump_string_length(const char *value, size_t length, FILE *out)
{
    size_t i;
    /* FIXME: handle illegal dereferences */
    if (value == NULL) fputs("NULL", out);
    else
    {
        fputc('"', out);
        for (i = 0; i < length; i++)
        {
            switch (value[0])
            {
            case '"': fputs("\\\"", out); break;
            case '\\': fputs("\\\\", out); break;
            case '\n': fputs("\\n", out); break;
            case '\r': fputs("\\r", out); break;
            default:
                if (iscntrl(value[0]))
                    fprintf(out, "\\%03o", (int) value[0]);
                else
                    fputc(value[0], out);
            }
            value++;
        }
        fputc('"', out);
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
