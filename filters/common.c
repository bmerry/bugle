#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "src/filters.h"
#include "src/utils.h"
#include "src/types.h"
#include "src/safemem.h"
#include "common/bool.h"
#include <stdio.h>

/* Invoke filter-set */

static char *log_filename = NULL;

static bool invoke_callback(function_call *call, void *data)
{
    invoke(call);
    return true;
}

static bool initialise_invoke(filter_set *handle)
{
    register_filter(handle, "invoke", invoke_callback);
    return true;
}

/* Log filter-set */

static FILE *log_file;

static bool log_callback(function_call *call, void *data)
{
    dump_any_call(&call->generic, 0, log_file);
    return true;
}

static bool initialise_log(filter_set *handle)
{
    if (log_filename)
        log_file = fopen(log_filename, "w");
    else
        log_file = stderr;
    if (!log_file)
    {
        if (log_filename)
            fprintf(stderr, "failed to open log file %s\n", log_filename);
        return false;
    }
    register_filter(handle, "log", log_callback);
    register_filter_depends("log", "invoke");
    return true;
}

static void destroy_log(filter_set *handle)
{
    if (log_filename)
    {
        if (log_file) fclose(log_file);
        free(log_filename);
    }
}

static bool set_variable_log(filter_set *handle, const char *name, const char *value)
{
    if (strcmp(name, "filename") == 0)
    {
        log_filename = xstrdup(value);
        return true;
    }
    return false;
}

/* General */

void initialise_filter_library(void)
{
    register_filter_set("invoke", initialise_invoke, NULL, NULL);
    register_filter_set("log", initialise_log, destroy_log, set_variable_log);
}
