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

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "src/utils.h"
#include "src/lib.h"
#include "filters.h"
#include "canon.h"
#include "safemem.h"
#include "conffile.h"
#include "hashtable.h"
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
    filter_set *f;
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
                        f = get_filter_set_handle(set->name);
                        if (!f)
                        {
                            fprintf(stderr, "warning: ignoring unknown filter-set %s\n",
                                    set->name);
                            continue;
                        }
                        for (j = list_head(&set->variables); j; j = list_next(j))
                        {
                            var = (const config_variable *) list_data(j);
                            if (!set_filter_set_variable(f,
                                                         var->name,
                                                         var->value))
                                fprintf(stderr, "warning: unused variable %s in filter-set %s\n",
                                        var->name, set->name);
                        }
                    }
                    for (i = list_head(&chain->filtersets); i; i = list_next(i))
                    {
                        set = (const config_filterset *) list_data(i);
                        f = get_filter_set_handle(set->name);
                        if (f) enable_filter_set(f);
                    }
                    done = true;
                }
                config_destroy();
            }
            fclose(yyin);
        }
        else
            fprintf(stderr, "failed to open config file %s; running in passthrough mode\n", config);
        free(config);
    }
    else
        fputs("$HOME not defined; running in passthrough mode\n", stderr);
    if (!done)
    {
        f = get_filter_set_handle("invoke");
        if (!f)
        {
            fputs("could not locate invoke filter-set; aborting\n", stderr);
            exit(1);
        }
        enable_filter_set(f);
    }
}

void interceptor(function_call *call)
{
    static bool initialised = false;

    if (!initialised)
    {
        initialise_hashing();
        initialise_canonical();
        initialise_filters();
        initialise_dump();
        init_real();
        load_config();
        initialised = true;
    }
    run_filters(call);
}
