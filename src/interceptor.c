#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "src/utils.h"
#include "src/lib.h"
#include "src/filters.h"
#include "src/canon.h"

void interceptor(function_call *call)
{
    static bool initialised = false;

    if (!initialised)
    {
        initialise_canonical();
        initialise_filters();
        enable_filter_set(get_filter_set_handle("error"));
        enable_filter_set(get_filter_set_handle("invoke"));
        enable_filter_set(get_filter_set_handle("log"));
        init_real();
        initialised = true;
    }
    run_filters(call);
}
