#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "src/utils.h"
#include "src/lib.h"
#include "src/filters.h"
#include "src/canon.h"
#include "src/safemem.h"
#include "src/conffile.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#define FILTERFILE "/.bugle/filters"

extern FILE *yyin;
extern int yyparse(void);

static void load_config(void)
{
    const char *home;
    char *config, *chain_name;
    const linked_list *root;
    const config_chain *chain;
    const config_filterset *set;
    const config_variable *var;
    list_node *i, *j;
    bool done = false;

    home = getenv("HOME");
    if (home)
    {
        config = xmalloc(strlen(home) + strlen(FILTERFILE) + 1);
        sprintf(config, "%s%s", home, FILTERFILE);
        if ((yyin = fopen(config, "r")))
        {
            if (yyparse() == 0)
            {
                chain_name = getenv("BUGLE_CHAIN");
                chain = NULL;
                if (chain_name)
                {
                    chain = config_get_chain(chain_name);
                    if (!chain)
                        fprintf(stderr, "could not find chain %s, trying default\n", chain_name);
                }
                if (!chain)
                {
                    root = config_get_root();
                    if (list_head(root))
                        chain = (const config_chain *) list_data(list_head(root));
                    if (!chain)
                        fputs("no chains defined; running in passthrough mode\n", stderr);
                }
                if (chain)
                {
                    for (i = list_head(&chain->filtersets); i; i = list_next(i))
                    {
                        set = (const config_filterset *) list_data(i);
                        for (j = list_head(&set->variables); j; j = list_next(j))
                        {
                            var = (const config_variable *) list_data(j);
                            if (!set_filter_set_variable(get_filter_set_handle(set->name),
                                                         var->name,
                                                         var->value))
                                fprintf(stderr, "warning: unused variable %s in filter-set %s\n",
                                        var->name, set->name);
                        }
                    }
                    for (i = list_head(&chain->filtersets); i; i = list_next(i))
                    {
                        set = (const config_filterset *) list_data(i);
                        enable_filter_set(get_filter_set_handle(set->name));
                    }
                    done = true;
                }
                config_destroy();
            }
            fclose(yyin);
        }
        else
            fprintf(stderr, "failed to open config file %s; running in passthrough mode\n", config);
    }
    else
        fputs("$HOME not defined; running in passthrough mode\n", stderr);
    if (!done)
        enable_filter_set(get_filter_set_handle("invoke"));
}

void interceptor(function_call *call)
{
    static bool initialised = false;

    if (!initialised)
    {
        initialise_canonical();
        initialise_filters();
        init_real();
        load_config();
        initialised = true;
    }
    run_filters(call);
}
