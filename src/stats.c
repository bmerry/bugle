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

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <assert.h>
#include <math.h>
#include "src/glutils.h"
#include "src/tracker.h"
#include "src/filters.h"
#include "src/log.h"
#include "src/xevent.h"
#include "src/glexts.h"
#include "src/stats.h"
#include "src/statsparse.h"
#include "common/safemem.h"
#include "common/hashtable.h"
#include "common/bool.h"
#include "xalloc.h"
#include "xstrndup.h"
#include "xvasprintf.h"

#define STATISTICSFILE "/.bugle/statistics"

extern FILE *stats_yyin;
extern int stats_yyparse(void);

static hash_table stats_signals;
static size_t stats_signals_num_active = 0;
static linked_list stats_signals_active;

/* Flat list of available statistics. Initially it is the statistics
 * specified in the file, but later rewritten to instantiate any generics.
 * The instances are complete clones with no shared memory. Instances are
 * also sequential in the linked list.
 */
static linked_list *stats_statistics = NULL;
/* Maps a name to the first instance of a statistic in stats_statistics. */
static hash_table stats_statistics_first;

/*** Low-level utilities ***/

static double time_elapsed(struct timeval *old, struct timeval *now)
{
    return (now->tv_sec - old->tv_sec) + 1e-6 * (now->tv_usec - old->tv_usec);
}

/* Finds the named signal object. If it does not exist, NULL is returned. */
static stats_signal *stats_signal_get(const char *name)
{
    return bugle_hash_get(&stats_signals, name);
}

/* Used by generators to list signals that they expose. Signals may not
 * be multiply registered. Slots are only assigned when signals are
 * activated.
 */
stats_signal *bugle_stats_signal_register(const char *name, void *user_data,
                                          bool (*activate)(stats_signal *))
{
    stats_signal *si;

    assert(!stats_signal_get(name));
    si = XZALLOC(stats_signal);
    si->value = NAN;
    si->integral = 0.0;
    si->offset = -1;
    si->user_data = user_data;
    si->activate = activate;
    bugle_hash_set(&stats_signals, name, si);
    return si;
}

/* Evaluates the expression. If a signal is missing, the return is NaN. */
double bugle_stats_expression_evaluate(const stats_expression *expr,
                                       stats_signal_values *old_signals,
                                       stats_signal_values *new_signals)
{
    double l = 0.0, r = 0.0;
    double elapsed;

    elapsed = time_elapsed(&old_signals->last_updated, &new_signals->last_updated);
    switch (expr->type)
    {
    case STATS_EXPRESSION_NUMBER:
        return expr->value;
    case STATS_EXPRESSION_OPERATION:
        if (expr->left)
            l = bugle_stats_expression_evaluate(expr->left, old_signals, new_signals);
        if (expr->right)
            r = bugle_stats_expression_evaluate(expr->right, old_signals, new_signals);
        switch (expr->op)
        {
        case STATS_OPERATION_PLUS: return l + r;
        case STATS_OPERATION_MINUS: return l - r;
        case STATS_OPERATION_TIMES: return l * r;
        case STATS_OPERATION_DIVIDE: return l / r;
        case STATS_OPERATION_UMINUS: return -l;
        default: abort(); /* Should never be reached */
        }
    case STATS_EXPRESSION_SIGNAL:
        /* Generate a NaN, and let good old IEEE 754 take care of propagating it. */
        if (!expr->signal)
            return NAN;
        switch (expr->op)
        {
        case STATS_OPERATION_DELTA:
            return new_signals->values[expr->signal->offset].value - old_signals->values[expr->signal->offset].value;
        case STATS_OPERATION_AVERAGE:
            return (new_signals->values[expr->signal->offset].integral - old_signals->values[expr->signal->offset].integral) / elapsed;
        case STATS_OPERATION_START:
            return old_signals->values[expr->signal->offset].value;
        case STATS_OPERATION_END:
            return new_signals->values[expr->signal->offset].value;
        default:
            abort(); /* Should never be reached */
        }
    }
    abort(); /* Should never be reached */
}

void bugle_stats_signal_update(stats_signal *si, double v)
{
    struct timeval now;

    gettimeofday(&now, NULL);
    /* Integrate over time; a NaN indicates that this is the first time */
    if (FINITE(si->value))
        si->integral += time_elapsed(&si->last_updated, &now) * si->value;
    si->value = v;
    si->last_updated = now;
}

/* Convenience for accumulating signals */
void bugle_stats_signal_add(stats_signal *si, double dv)
{
    if (!FINITE(si->value))
        bugle_stats_signal_update(si, dv);
    else
        bugle_stats_signal_update(si, si->value + dv);
}

static bool stats_signal_activate(stats_signal *si)
{
    if (!si->active)
    {
        si->active = true;
        if (si->activate)
            si->active = (*si->activate)(si);
        else
            bugle_stats_signal_update(si, 0.0);
        if (si->active)
        {
            si->offset = stats_signals_num_active++;
            bugle_list_append(&stats_signals_active, si);
        }
    }
    return si->active;
}

void bugle_stats_signal_values_init(stats_signal_values *sv)
{
    sv->allocated = 0;
    sv->values = NULL;
    sv->last_updated.tv_sec = 0;
    sv->last_updated.tv_usec = 0;
}

void bugle_stats_signal_values_clear(stats_signal_values *sv)
{
    free(sv->values);
    sv->values = NULL;
    sv->allocated = 0;
    sv->last_updated.tv_sec = 0;
    sv->last_updated.tv_usec = 0;
}

void bugle_stats_signal_values_gather(stats_signal_values *sv)
{
    stats_signal *si;
    linked_list_node *s;
    int i;

    gettimeofday(&sv->last_updated, NULL);

    if (sv->allocated < stats_signals_num_active)
    {
        sv->allocated = stats_signals_num_active;
        sv->values = xnrealloc(sv->values, stats_signals_num_active, sizeof(stats_signal_value));
    }
    /* Make sure that everything is initialised to an insane value (NaN) */
    for (i = 0; i < stats_signals_num_active; i++)
        sv->values[i].value = sv->values[i].integral = NAN;
    for (s = bugle_list_head(&stats_signals_active); s; s = bugle_list_next(s))
    {
        si = (stats_signal *) bugle_list_data(s);
        if (si->active && si->offset >= 0)
        {
            sv->values[si->offset].value = si->value;
            sv->values[si->offset].integral = si->integral;
            /* Have to update the integral from when the signal was updated
             * until this instant. */
            if (FINITE(si->value))
                sv->values[si->offset].integral +=
                    si->value * time_elapsed(&si->last_updated,
                                             &sv->last_updated);
        }
    }
}

/* Calls the callback function for each signal sub-expression in the
 * expression. The return value is the AND of the return values of all
 * the callbacks. If shortcut is true, the function returns as soon as
 * any callback return false, without calling the rest.
 */
static bool stats_expression_forall(stats_expression *expr, bool shortcut,
                                    bool (*callback)(stats_expression *, void *),
                                    void *arg)
{
    bool ans;
    switch (expr->type)
    {
    case STATS_EXPRESSION_NUMBER:
        return true;
    case STATS_EXPRESSION_OPERATION:
        ans = stats_expression_forall(expr->left, shortcut, callback, arg);
        if (shortcut && !ans) return ans;
        if (expr->right)
            ans = stats_expression_forall(expr->right, shortcut, callback, arg) && ans;
        return ans;
    case STATS_EXPRESSION_SIGNAL:
        return callback(expr, arg);
    }
    return false;  /* should never be reached */
}

/* Callback for stats_expression_activate_signals */
static bool stats_expression_activate_signals1(stats_expression *expr, void *arg)
{
    expr->signal = stats_signal_get(expr->signal_name);
    if (!expr->signal)
    {
        bugle_log_printf("stats", "expression", BUGLE_LOG_WARNING,
                         "Signal %s is not registered - missing filter-set?",
                         expr->signal_name);
        return false;
    }
    return stats_signal_activate(expr->signal);
}

/* Initialises and activates all signals that this expression depends on.
 * It returns true if they were successfully activated, or false if any
 * failed to activate or were unregistered.
 */
bool bugle_stats_expression_activate_signals(stats_expression *expr)
{
    return stats_expression_forall(expr, false, stats_expression_activate_signals1, NULL);
}

/* Checks whether this signal_name does NOT contain a *. This is because
 * the forall takes an AND rather than an OR. If the signal is generic, it
 * copies a pointer to the name into the arg, if not already present.
 */
static bool stats_expression_generic1(stats_expression *expr, void *arg)
{
    if (strchr(expr->signal_name, '*') != NULL)
    {
        char **out;
        out = (char **) arg;
        if (!*out) *out = expr->signal_name;
        return false;
    }
    else return true;
}

/* If the expression is generic, returns a pointer to a generic signal name
 * (which should NOT be freed). Otherwise, returns NULL.
 */
static char *stats_expression_generic(stats_expression *expr)
{
    char *generic_name = NULL;
    stats_expression_forall(expr, true, stats_expression_generic1, &generic_name);
    return generic_name;
}

/* Creates a new string by substituting rep for the first * in pattern.
 * The caller must free the memory.
 */
static char *pattern_replace(const char *pattern, const char *rep)
{
    size_t l_pattern, l_rep;
    char *wildcard;
    char *full;

    wildcard = strchr(pattern, '*');
    if (!wildcard) return xstrdup(pattern);
    l_pattern = strlen(pattern);
    l_rep = strlen(rep);
    full = XNMALLOC(l_pattern + l_rep, char);
    memcpy(full, pattern, wildcard - pattern);
    memcpy(full + (wildcard - pattern), rep, l_rep);
    strcpy(full + (wildcard - pattern) + l_rep, wildcard + 1);
    return full;
}

/* Checks whether instance matches pattern, where the first * is a wildcard. */
static bool pattern_match(const char *pattern, const char *instance)
{
    char *wildcard;
    size_t l_pattern, l_instance;
    size_t size1, size2;

    wildcard = strchr(pattern, '*');
    if (!wildcard)
        return strcmp(pattern, instance) == 0;

    l_pattern = strlen(pattern);
    l_instance = strlen(instance);
    if (l_instance + 1 < l_pattern) return false;  /* instance too short */
    size1 = wildcard - pattern;
    size2 = l_pattern - 1 - size1;
    return memcmp(pattern, instance, size1) == 0
        && memcmp(wildcard + 1, instance + l_instance - size2, size2) == 0;
}

/* Assumes that pattern_match is true, and returns the string that is the
 * substitution for the *. The caller must free the memory.
 */
static char *pattern_match_rep(const char *pattern, const char *instance)
{
    char *wildcard;
    size_t l_pattern, l_instance;

    assert(pattern_match(pattern, instance));
    wildcard = strchr(pattern, '*');
    l_pattern = strlen(pattern);
    l_instance = strlen(instance);
    return xstrndup(instance + (wildcard - pattern), l_instance + 1 - l_pattern);
}

/* For a generic expression, checks whether substituting arg (a char *)
 * will yield a registered expression.
 */
static bool stats_expression_match1(stats_expression *expr, void *arg)
{
    char *full;
    bool found;

    if (!strchr(expr->signal_name, '*')) return true; /* Not a generic */
    full = pattern_replace(expr->signal_name, (const char *) arg);
    found = stats_signal_get(full) != NULL;
    free(full);
    return found;
}

/* Instantiates a generic expression. */
static stats_expression *stats_expression_instantiate(stats_expression *base, const char *rep)
{
    stats_expression *n;

    assert(base);
    n = XMALLOC(stats_expression);
    *n = *base;
    switch (base->type)
    {
    case STATS_EXPRESSION_NUMBER:
        break;
    case STATS_EXPRESSION_OPERATION:
        if (n->left) n->left = stats_expression_instantiate(n->left, rep);
        if (n->right) n->right = stats_expression_instantiate(n->right, rep);
        break;
    case STATS_EXPRESSION_SIGNAL:
        n->signal = NULL;
        n->signal_name = pattern_replace(n->signal_name, rep);
        break;
    default: abort(); /* should never be reached */
    }
    return n;
}

/* Frees the expression object itself and its children */
static void stats_expression_free(stats_expression *expr)
{
    assert(expr);
    switch (expr->type)
    {
    case STATS_EXPRESSION_NUMBER: break;
    case STATS_EXPRESSION_OPERATION:
        if (expr->left) stats_expression_free(expr->left);
        if (expr->right) stats_expression_free(expr->right);
        break;
    case STATS_EXPRESSION_SIGNAL:
        free(expr->signal_name);
        break;
    }
    free(expr);
}

stats_substitution *bugle_stats_statistic_find_substitution(stats_statistic *st, double v)
{
    linked_list_node *i;
    stats_substitution *cur;

    for (i = bugle_list_head(&st->substitutions); i; i = bugle_list_next(i))
    {
        cur = (stats_substitution *) bugle_list_data(i);
        if (fabs(cur->value - v) < 1e-9) return cur;
    }
    return NULL;
}

static void stats_statistic_free(stats_statistic *st)
{
    linked_list_node *i;
    stats_substitution *sub;

    free(st->name);
    stats_expression_free(st->value);
    free(st->label);
    for (i = bugle_list_head(&st->substitutions); i; i = bugle_list_next(i))
    {
        sub = (stats_substitution *) bugle_list_data(i);
        free(sub->replacement);
    }
    bugle_list_clear(&st->substitutions);
    free(st);
}

static stats_statistic *stats_statistic_instantiate(stats_statistic *st, const char *rep)
{
    stats_statistic *n;
    linked_list_node *i;

    n = XMALLOC(stats_statistic);
    *n = *st;
    n->name = xstrdup(n->name);
    n->label = pattern_replace(n->label, rep);
    n->value = stats_expression_instantiate(st->value, rep);
    n->last = false;

    bugle_list_init(&n->substitutions, true);
    for (i = bugle_list_head(&st->substitutions); i; i = bugle_list_next(i))
    {
        stats_substitution *su_old, *su_new;
        su_old = bugle_list_data(i);
        su_new = XMALLOC(stats_substitution);
        *su_new = *su_old;
        su_new->replacement = pattern_replace(su_old->replacement, rep);
        bugle_list_append(&n->substitutions, su_new);
    }
    return n;
}

/* Returns the first of a set if it exists, or NULL if not. */
linked_list_node *bugle_stats_statistic_find(const char *name)
{
    return bugle_hash_get(&stats_statistics_first, name);
}

/* List the registered statistics, for when an illegal one is mentioned */
void bugle_stats_statistic_list(void)
{
    linked_list_node *j;
    stats_statistic *st;

    fputs("The registered statistics are:\n", stderr);

    for (j = bugle_list_head(stats_statistics); j; j = bugle_list_next(j))
    {
        st = (stats_statistic *) bugle_list_data(j);
        if (st->last)
            fprintf(stderr, "  %s\n", st->name);
    }
}

/*** stats filterset ***/

/* Reads the statistics configuration file. On error, prints a message
 * to the log and returns false.
 */
static bool stats_load_config(void)
{
    const char *home;
    char *config = NULL;

    if (getenv("BUGLE_STATISTICS"))
        config = xstrdup(getenv("BUGLE_STATISTICS"));
    home = getenv("HOME");
    if (!config && !home)
    {
        bugle_log("stats", "config", BUGLE_LOG_ERROR,
                  "$HOME is not set; please set $HOME or $BUGLE_STATISTICS");
        return false;
    }
    if (!config)
    {
        config = XNMALLOC(strlen(home) + strlen(STATISTICSFILE) + 1, char);
        sprintf(config, "%s%s", home, STATISTICSFILE);
    }
    if ((stats_yyin = fopen(config, "r")) != NULL)
    {
        if (stats_yyparse() == 0)
        {
            stats_statistics = stats_statistics_get_list();
            free(config);
            return true;
        }
        else
        {
            bugle_log_printf("stats", "config", BUGLE_LOG_ERROR,
                             "Parse error in %s", config);
            free(config);
            return false;
        }
    }
    else
    {
        bugle_log_printf("stats", "config", BUGLE_LOG_ERROR,
                         "Failed to open %s", config);
        free(config);
        return false;
    }
}

static bool stats_initialise(filter_set *handle)
{
    bugle_hash_init(&stats_signals, true);
    bugle_list_init(&stats_signals_active, false);
    bugle_hash_init(&stats_statistics_first, false);
    return true;
}

/* Replaces each generic statistic from the config file with one or more
 * instances, and sets up stats_statistics_first.
 */
static bool stats_ordering_initialise(filter_set *handle)
{
    linked_list_node *i, *first, *last, *tmp;

    if (!stats_load_config()) return false;
    for (i = bugle_list_head(stats_statistics); i; i = bugle_list_next(i))
    {
        stats_statistic *st;
        char *pattern;
        int count = 1;

        st = (stats_statistic *) bugle_list_data(i);
        pattern = stats_expression_generic(st->value);
        first = last = i;
        if (pattern)
        {
            const hash_table_entry *j;

            count--;
            for (j = bugle_hash_begin(&stats_signals); j; j = bugle_hash_next(&stats_signals, j))
            {
                if (pattern_match(pattern, j->key))
                {
                    char *rep;
                    stats_statistic *n;

                    rep = pattern_match_rep(pattern, j->key);
                    if (stats_expression_forall(st->value, true,
                                                stats_expression_match1, rep))
                    {
                        n = stats_statistic_instantiate(st, rep);
                        i = bugle_list_insert_after(stats_statistics, i, n);
                        count++;
                    }
                    free(rep);
                }
            }
            stats_statistic_free(st);

            last = i;
            tmp = first;   /* The original, generic item */
            first = bugle_list_next(first);  /* The first instance */
            bugle_list_erase(stats_statistics, tmp);
        }
        if (count)
        {
            stats_statistic *st;
            st = bugle_list_data(first);
            bugle_hash_set(&stats_statistics_first, st->name, first);
            st = bugle_list_data(last);
            st->last = true;
        }
    }

    /* This filter does no interception, but it provides a sequence point
     * to allow display modules (showstats, logstats) to occur after
     * generator modules (stats_basic etc).
     */
    bugle_filter_register(handle, "stats_ordering");
    return true;
}

static void stats_destroy(filter_set *handle)
{
    linked_list_node *i;
    stats_statistic *st;

    if (stats_statistics)
    {
        for (i = bugle_list_head(stats_statistics); i; i = bugle_list_next(i))
        {
            st = (stats_statistic *) bugle_list_data(i);
            stats_statistic_free(st);
        }
        bugle_list_clear(stats_statistics);
    }
    bugle_list_clear(&stats_signals_active);
    bugle_hash_clear(&stats_signals);
    bugle_hash_clear(&stats_statistics_first);
}

/*** Global initialisation */

void bugle_filter_set_stats_generator(const char *name)
{
    bugle_filter_set_depends(name, "stats");
    bugle_filter_set_order(name, "stats_ordering");
}

void bugle_filter_set_stats_logger(const char *name)
{
    bugle_filter_set_depends(name, "stats");
    bugle_filter_set_depends(name, "stats_ordering");
}

void statistics_initialise(void)
{
    static const filter_set_info stats_info =
    {
        "stats",
        stats_initialise,
        stats_destroy,
        NULL,
        NULL,
        NULL,
        NULL
    };

    /* Filter-set which exists for dependencies: every logger depends on
     * it and it depends on every generator. The initialisation also takes
     * care of expanding any generic statistics.
     */
    static const filter_set_info stats_ordering_info =
    {
        "stats_ordering",
        stats_ordering_initialise,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
    };

    bugle_filter_set_register(&stats_info);
    bugle_filter_set_register(&stats_ordering_info);
}
