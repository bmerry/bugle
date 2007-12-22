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

#ifndef BUGLE_BUDGIELIB_BUDGIEUTILS_H
#define BUGLE_BUDGIELIB_BUDGIEUTILS_H

#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <budgie/typeutils.h>

typedef int budgie_group;   /* function group */
typedef int budgie_function;
#define NULL_FUNCTION (-1)

typedef struct
{
    budgie_group group;
    budgie_function id;
    int num_args;
    const void **args;
    void *retn;
    void *user_data;
} generic_function_call;

typedef bool (*arg_dumper)(const generic_function_call *, int, const void *, int, FILE *);
typedef budgie_type (*arg_get_type)(const generic_function_call *, int, const void *);
typedef int (*arg_get_length)(const generic_function_call *, int, const void *);

typedef struct
{
    budgie_type type;
    arg_dumper dumper;
    arg_get_type get_type;
    arg_get_length get_length;
} group_parameter_data;

typedef struct
{
    size_t num_parameters;
    const group_parameter_data *parameters;
    group_parameter_data retn;
    bool has_retn;
} group_data;

typedef struct
{
    const char *name;
    void (*real)(void);
    budgie_group group;
} function_data;

typedef struct
{
    const char *name;
    void (*wrapper)(void);
} function_name_data;

/* Not const due to "real" field.
 * FIXME: split that field out for better efficiency */
extern function_data budgie_function_table[];
extern function_name_data budgie_function_name_table[]; /* Holds wrappers in alphabetical order */
extern const group_data budgie_group_table[];
extern const char * const library_names[];
extern int budgie_number_of_groups, budgie_number_of_functions;
extern int budgie_number_of_libraries;

bool check_set_reentrance(void);
void clear_reentrance(void);

/* Dumps a call, including arguments and return. Does not include a newline */
void budgie_dump_any_call(const generic_function_call *call, int indent, FILE *out);

/* Maps an overridden function name to a wrapper function, or NULL if none */
void (*budgie_get_function_wrapper(const char *name))(void);

void initialise_real(void);

#endif /* BUGLE_BUDGIE_BUDGIEUTILS_H */
