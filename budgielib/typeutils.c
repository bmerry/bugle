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
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <stdbool.h>
#include "typeutils.h"

void budgie_dump_bitfield(unsigned int value, FILE *out,
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

void budgie_dump_any_type(budgie_type type, const void *value, int length, FILE *out)
{
    const type_data *info;
    budgie_type new_type;

    assert(type >= 0);
    info = &budgie_type_table[type];
    if (info->get_type) /* highly unlikely */
    {
        new_type = (*info->get_type)(value);
        if (new_type != type)
        {
            budgie_dump_any_type(new_type, value, length, out);
            return;
        }
    }
    /* More likely e.g. for strings. Note that we don't override the length
     * if specified, since it may come from a parameter override
     */
    if (info->get_length && length == -1) 
        length = (*info->get_length)(value);

    assert(info->dumper);
    (*info->dumper)(value, length, out);
}

void budgie_dump_any_type_extended(budgie_type type,
                                   const void *value,
                                   int length,
                                   int outer_length,
                                   const void *pointer,
                                   FILE *out)
{
    int i;
    const char *v;

    if (pointer)
        fprintf(out, "%p -> ", pointer);
    if (outer_length == -1)
        budgie_dump_any_type(type, value, length, out);
    else
    {
        v = (const char *) value;
        fputs("{ ", out);
        for (i = 0; i < outer_length; i++)
        {
            if (i) fputs(", ", out);
            budgie_dump_any_type(type, (const void *) v, length, out);
            v += budgie_type_table[type].size;
        }
        fputs(" }", out);
    }
}

