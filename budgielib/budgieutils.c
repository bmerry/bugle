#if HAVE_CONFIG_H
# include <config.h>
#endif
#define _POSIX_SOURCE
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include "common/bool.h"
#include "budgieutils.h"
#include <setjmp.h>

/* FIXME: define in a header somewhere
 * FIXME: dump_any_type should be table-driven
 */
void dump_any_type(budgie_type type, const void *value, int length, FILE *out);
budgie_type get_arg_type(const generic_function_call *call, int param);
int get_arg_length(const generic_function_call *call, int param, const void *value);

void dump_bitfield(unsigned int value, FILE *out,
                   bitfield_pair *tags, int count)
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

bool dump_string(const void *value, int count, FILE *out)
{
    /* FIXME: handle illegal dereferences */
    const char *str = *(const char * const *) value;
    if (str == NULL) fputs("NULL", out);
    else fprintf(out, "\"%s\"", str);
    return true;
}

int count_string(const void *value)
{
    /* FIXME: handle illegal deferences */
    const char *str = (const char *) value;
    if (str == NULL) return 0;
    else return strlen(str) + 1;
}

void dump_any_call(const generic_function_call *call, int indent, FILE *out)
{
    size_t i;
    const function_data *data;
    parameter_dumper cur_dumper;

    make_indent(indent, out);
    data = &function_table[call->id];
    fputs(data->name, out);
    fputs("(", out);
    for (i = 0; i < data->num_parameters; i++)
    {
        if (i) fputs(", ", out);
        cur_dumper = data->parameters[i].dumper;
        if (!cur_dumper || !(*cur_dumper)(call, i, call->args[i], out))
            dump_any_type(get_arg_type(call, i),
                          call->args[i],
                          get_arg_length(call, i, call->args[i]),
                          out);
    }
    fputs(")", out);
    if (call->ret)
    {
        fputs(" = ", out);
        cur_dumper = data->retn.dumper;
        if (!cur_dumper || !(*cur_dumper)(call, -1, call->ret, out))
            dump_any_type(get_arg_type(call, -1),
                          call->ret,
                          get_arg_length(call, -1, call->ret),
                          out);
    }
    fputs("\n", out);
}

void make_indent(int indent, FILE *out)
{
    int i;
    for (i = 0; i < indent; i++)
        fputc(' ', out);
}

/* Memory validation */

static sigjmp_buf valid_buf;

static void valid_sigsegv_handler(int sig)
{
    siglongjmp(valid_buf, 1);
}

bool valid_address_range(const char *base, size_t count)
{
    volatile char test;
    struct sigaction old, act;
    bool result = false;

    if (sigsetjmp(valid_buf, 1) == 0)
    {
        act.sa_handler = valid_sigsegv_handler;
        sigemptyset(&act.sa_mask);
        act.sa_flags = 0;
        while (sigaction(SIGSEGV, &act, &old) == -1)
        {
            if (errno != EINTR)
            {
                perror("failed to set SIGSEGV handler");
                exit(1);
            }
        }
        while (count)
        {
            test = *base;
            base++;
            count--;
        }
        result = true;
    }
    while (sigaction(SIGSEGV, &old, NULL) == -1)
    {
        if (errno != EINTR)
        {
            perror("failed to reset SIGSEGV handler");
            exit(1);
        }
    }
    return result;
}
