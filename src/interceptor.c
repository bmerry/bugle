/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2008  Bruce Merry
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

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <bugle/glwin/glwin.h>
#include <bugle/glwin/trackcontext.h>
#include <bugle/gl/globjects.h>
#include <bugle/gl/gldisplaylist.h>
#include <bugle/gl/glbeginend.h>
#include <bugle/gl/glextensions.h>
#include <bugle/filters.h>
#include <bugle/xevent.h>
#include <bugle/log.h>
#include <bugle/stats.h>
#include <bugle/hashtable.h>
#include "common/threads.h"
#include <budgie/reflect.h>
#include <budgie/addresses.h>
#include "conffile.h"
#include "dlopen.h"
#include "xalloc.h"

#define FILTERFILE "/.bugle/filters"

extern FILE *yyin;
extern int yyparse(void);

/* FIXME: design for a per-API initialisation block */
extern void bugle_initialise_addresses_extra(void);

static void toggle_filterset(const xevent_key *key, void *arg, XEvent *event)
{
    filter_set *set;

    set = (filter_set *) arg;
    if (bugle_filter_set_is_active(set))
        filter_set_deactivate(set);
    else
        filter_set_activate(set);
}

static void load_config(void)
{
    const char *home;
    char *config = NULL, *chain_name;
    const linked_list *root;
    const config_chain *chain;
    const config_filterset *set;
    const config_variable *var;
    filter_set *f;
    linked_list_node *i, *j;
    bool debugging;
    xevent_key key;

    if (getenv("BUGLE_FILTERS"))
        config = xstrdup(getenv("BUGLE_FILTERS"));
    home = getenv("HOME");
    chain_name = getenv("BUGLE_CHAIN");
    if (chain_name && !*chain_name) chain_name = NULL;
    debugging = getenv("BUGLE_DEBUGGER") != NULL;
    /* If using the debugger and no chain is specified, we use passthrough
     * mode.
     */
    if ((config || home) && (!debugging || chain_name))
    {
        if (!config)
        {
            config = XNMALLOC(strlen(home) + strlen(FILTERFILE) + 1, char);
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
                        filters_help();
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
                        f = bugle_filter_set_get_handle(set->name);
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
                        f = bugle_filter_set_get_handle(set->name);
                        if (f)
                        {
                            filter_set_add(f, set->active);
                            if (set->key)
                            {
                                if (!bugle_xevent_key_lookup(set->key, &key))
                                    fprintf(stderr, "warning: ignoring unknown toggle key %s for filter-set %s\n",
                                            set->key, set->name);
                                else
                                    bugle_xevent_key_callback(&key, NULL, toggle_filterset, f);
                            }
                        }
                    }
                }
                bugle_config_shutdown();
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
    f = bugle_filter_set_get_handle("invoke");
    if (!f)
    {
        fputs("could not find the 'invoke' filter-set; aborting\n", stderr);
        exit(1);
    }
    filter_set_add(f, true);
    /* Auto-load the debugger filter-set if using the debugger */
    if (debugging)
    {
        f = bugle_filter_set_get_handle("debugger");
        if (!f)
        {
            fputs("could not find the 'debugger' filter-set; aborting\n", stderr);
            exit(1);
        }
        filter_set_add(f, true);
    }
}

static void initialise_core_filters(void)
{
    trackcontext_initialise();
    gldisplaylist_initialise();
    glbeginend_initialise();
    glextensions_initialise();
    globjects_initialise();
    log_initialise();
    statistics_initialise();
}

BUGLE_CONSTRUCTOR(initialise_all);

static void initialise_all(void)
{
    budgie_function_address_initialise();
    bugle_function_address_initialise_extra();
    xevent_initialise();
    filters_initialise();
    initialise_core_filters();
    dump_initialise();
    dlopen_initialise();
    load_config();
    filters_finalise();
}

/* Used by xevents */
void bugle_initialise_all(void)
{
    BUGLE_RUN_CONSTRUCTOR(initialise_all);
}

void budgie_interceptor(function_call *call)
{
    BUGLE_RUN_CONSTRUCTOR(initialise_all);
    filters_run(call);
}

BUDGIEAPIPROC budgie_address_generator(budgie_function id)
{
#if BUGLE_GLWIN_CONTEXT_DEPENDENT
    return bugle_glwin_get_proc_address(budgie_function_name(id));
#else
    return NULL;
#endif
}
