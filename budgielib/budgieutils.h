#ifndef BUGLE_BUDGIELIB_BUDGIEFUNCS_H
#define BUGLE_BUDGIELIB_BUDGIEFUNCS_H

#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>

typedef int budgie_function;
typedef int budgie_type;
#define NULL_TYPE (-1)
#define NULL_FUNCTION (-1)

typedef struct
{
    budgie_function id;
    int num_args;
    const void **args;
    void *ret;
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

typedef struct
{
    type_code code;
    budgie_type type;
    const type_record_data *fields;
    size_t size;
    size_t length;
} type_data;

typedef bool (*parameter_dumper)(const generic_function_call *, int, const void *, FILE *);

typedef struct
{
    budgie_type type;
    parameter_dumper dumper;
} function_parameter_data;

typedef struct
{
    const char *name;
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
extern const function_data function_table[];

void dump_bitfield(unsigned int value, FILE *out,
                   bitfield_pair *tags, int count);
bool dump_string(const void *value, int count, FILE *out);
int count_string(const void *value);

/* User functions */

void dump_any_call(const generic_function_call *call, int indent, FILE *out);
void make_indent(int indent, FILE *out);

#endif /* BUGLE_BUDGIE_BUDGIEFUNCS_H */
