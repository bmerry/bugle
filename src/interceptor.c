/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004, 2005  Bruce Merry
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
#include "src/glfuncs.h"
#include "filters.h"
#include "conffile.h"
#include "tracker.h"
#include "log.h"
#include "common/hashtable.h"
#include "common/safemem.h"
#include "common/threads.h"
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
    char *config = NULL, *chain_name;
    const bugle_linked_list *root;
    const config_chain *chain;
    const config_filterset *set;
    const config_variable *var;
    filter_set *f;
    bugle_list_node *i, *j;
    bool debugging;

    if (getenv("BUGLE_FILTERS"))
        config = bugle_strdup(getenv("BUGLE_FILTERS"));
    home = getenv("HOME");
    chain_name = getenv("BUGLE_CHAIN");
    debugging = getenv("BUGLE_DEBUGGER") != NULL;
    /* If using the debugger and no chain is specified, we use passthrough
     * mode.
     */
    if ((config || home) && (!debugging || chain_name))
    {
        if (!config)
        {
            config = bugle_malloc(strlen(home) + strlen(FILTERFILE) + 1);
            sprintf(config, "%s%s", home, FILTERFILE);
        }
        if ((yyin = fopen(config, "r")))
        {
            if (yyparse() == 0)
            {
                chain = NULL;
                if (chain_name)
                {
                    chain = bugle_config_get_chain(chain_name);
                    if (!chain)
                    {
                        fprintf(stderr, "no such chain %s\n", chain_name);
                        bugle_filters_help();
                        exit(1);
                    }
                }
                if (!chain)
                {
                    root = bugle_config_get_root();
                    if (bugle_list_head(root))
                        chain = (const config_chain *) bugle_list_data(bugle_list_head(root));
                    if (!chain)
                        fputs("no chains defined; running in passthrough mode\n", stderr);
                }
                if (chain)
                {
                    for (i = bugle_list_head(&chain->filtersets); i; i = bugle_list_next(i))
                    {
                        set = (const config_filterset *) bugle_list_data(i);
                        f = bugle_get_filter_set_handle(set->name);
                        if (!f)
                        {
                            fprintf(stderr, "warning: ignoring unknown filter-set %s\n",
                                    set->name);
                            continue;
                        }
                        for (j = bugle_list_head(&set->variables); j; j = bugle_list_next(j))
                        {
                            var = (const config_variable *) bugle_list_data(j);
                            if (!filter_set_variable(f,
                                                     var->name,
                                                     var->value))
                                exit(1);
                        }
                    }
                    for (i = bugle_list_head(&chain->filtersets); i; i = bugle_list_next(i))
                    {
                        set = (const config_filterset *) bugle_list_data(i);
                        f = bugle_get_filter_set_handle(set->name);
                        if (f) bugle_enable_filter_set(f);
                    }
                }
                bugle_config_destroy();
            }
            fclose(yyin);
        }
        else
            fprintf(stderr, "failed to open config file %s; running in passthrough mode\n", config);
        free(config);
    }
    else if (!debugging)
        fputs("$HOME not defined; running in passthrough mode\n", stderr);

    /* Always load the invoke filter-set */
    f = bugle_get_filter_set_handle("invoke");
    if (!f)
    {
        fputs("could not find the 'invoke' filter-set; aborting\n", stderr);
        exit(1);
    }
    bugle_enable_filter_set(f);
    /* Auto-load the debugger filter-set if using the debugger */
    if (debugging)
    {
        f = bugle_get_filter_set_handle("debugger");
        if (!f)
        {
            fputs("could not find the 'debugger' filter-set; aborting\n", stderr);
            exit(1);
        }
        bugle_enable_filter_set(f);
    }
}

static void initialise_core_filters(void)
{
    trackcontext_initialise();
    trackdisplaylist_initialise();
    trackbeginend_initialise();
    trackextensions_initialise();
    trackobjects_initialise();
    log_initialise();
}

/* The Linux ABI requires that OpenGL 1.2 functions be accessible by
 * direct dynamic linking, but everything else should be accessed by
 * glXGetProcAddressARB. We deal with that here.
 */
static void initialise_addresses(void)
{
    size_t i;

    for (i = 0; i < NUMBER_OF_FUNCTIONS; i++)
        if (!bugle_gl_function_table[i].version
            || strcmp(bugle_gl_function_table[i].version, "GL_VERSION_1_2") > 0)
            budgie_function_table[i].real = CALL_glXGetProcAddressARB((const GLubyte *) budgie_function_table[i].name);
}

static bugle_thread_once_t init_key_once = BUGLE_THREAD_ONCE_INIT;
static void initialise_all(void)
{
    bugle_initialise_hashing();
    initialise_real();
    initialise_addresses();
    initialise_filters();
    initialise_core_filters();
    initialise_dump_tables();
    load_config();
}

void budgie_interceptor(function_call *call)
{
    bugle_thread_once(&init_key_once, initialise_all);
    run_filters(call);
}
