/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004  Bruce Merry
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#ifndef BUGLE_BUDGIELIB_BUDGIEFUNCS_H
#define BUGLE_BUDGIELIB_BUDGIEFUNCS_H

#include <stdio.h>
#include <stddef.h>
#include "common/bool.h"

typedef int budgie_function;
typedef int budgie_type;
#define NULL_TYPE (-1)
#define NULL_FUNCTION (-1)

typedef struct
{
    budgie_function id;
    int num_args;
    const void **args;
    void *retn;
    void *user_data;
} generic_function_call;

typedef enum
{
    CODE_ENUMERAL,
    CODE_INTEGRAL,
    CODE_FLOAT,
    CODE_COMPLEX,
    CODE_POINTER,
    CODE_ARRAY,
    CODE_RECORD,
    CODE_OTHER
} type_code;

typedef struct
{
    budgie_type type;
    ptrdiff_t offset;
} type_record_data;

typedef void (*type_dumper)(const void *, int, FILE *);
typedef budgie_type (*type_get_type)(const void *);
typedef int (*type_get_length)(const void *);

typedef struct
{
    type_code code;
    budgie_type type;    /* base type of pointers, arrays, functions etc */
    budgie_type pointer; /* pointer to this type (NULL_TYPE if not generated) */
    const type_record_data *fields;
    size_t size;
    size_t length;
    type_dumper dumper;
    type_get_type get_type;
    type_get_length get_length;
} type_data;

typedef bool (*arg_dumper)(const generic_function_call *, int, const void *, int, FILE *);
typedef budgie_type (*arg_get_type)(const generic_function_call *, int, const void *);
typedef int (*arg_get_length)(const generic_function_call *, int, const void *);

typedef struct
{
    budgie_type type;
    arg_dumper dumper;
    arg_get_type get_type;
    arg_get_length get_length;
} function_parameter_data;

typedef struct
{
    const char *name;
    void (*real)(void);
    size_t num_parameters;
    const function_parameter_data *parameters;
    function_parameter_data retn;
    bool has_retn;
} function_data;

typedef struct
{
    unsigned int value;
    const char *name;
} bitfield_pair;

extern const type_data type_table[];
/* Not const due to "real" field.
 * FIXME: split that field out for better efficiency */
extern function_data function_table[];
extern const char * const library_names[];
extern int number_of_types;
extern int number_of_functions;
extern int number_of_libraries;

/* Code used by budgie output */
void dump_bitfield(unsigned int value, FILE *out,
                   bitfield_pair *tags, int count);

bool check_set_reentrance(void);
void clear_reentrance(void);

char *string_io(void (*call)(FILE *, void *), void *data);

/* User functions */

bool dump_string(const char *value, FILE *out);
int count_string(const char *value);

void make_indent(int indent, FILE *out);
void dump_any_type(budgie_type type, const void *value, int length, FILE *out);
void dump_any_call(const generic_function_call *call, int indent, FILE *out);
void initialise_real(void);

#endif /* BUGLE_BUDGIE_BUDGIEFUNCS_H */
