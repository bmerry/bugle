#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "src/filters.h"
#include "src/utils.h"
#include "src/types.h"
#include <stdio.h>
#include <stdbool.h>

/* Invoke filter-set */

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
    log_file = fopen("/tmp/fakegl.log", "w");
    if (!log_file) return false;
    register_filter(handle, "log", log_callback);
    register_filter_depends("log", "invoke");
    return true;
}

static void destroy_log(filter_set *handle)
{
    if (log_file) fclose(log_file);
}

/* General */

void initialise_filter_library(void)
{
    register_filter_set("invoke", initialise_invoke, NULL);
    register_filter_set("log", initialise_log, destroy_log);
}
