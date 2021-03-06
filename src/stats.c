/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2010  Bruce Merry
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
#include <assert.h>
#include <math.h>
#include <bugle/memory.h>
#include <bugle/string.h>
#include <bugle/filters.h>
#include <bugle/log.h>
#include <bugle/input.h>
#include <bugle/stats.h>
#include <bugle/math.h>
#include <bugle/string.h>
#include <bugle/time.h>
#include "statsparse.h"
#include <bugle/hashtable.h>
#include <bugle/bool.h>

#define STATISTICSFILE "/.bugle/statistics"

extern FILE *stats_yyin;
extern int stats_yyparse(void);

static hash_table stats_signals;
static size_t stats_signals_num_active = 0;
static linked_list stats_signals_active; /* Pointers into stats_signals */

/* Flat list of available statistics. Initially it is the statistics
 * specified in the file, but later rewritten to instantiate any generics.
 * The instances are complete clones with no shared memory. Instances are
 * also sequential in the linked list.
 */
static linked_list *stats_statistics = NULL;
/* Maps a name to the first instance of a statistic in stats_statistics. */
static hash_table stats_statistics_first;

/*** Low-level utilities ***/

static double time_elapsed(bugle_timespec *old, bugle_timespec *now)
{
    return (now->tv_sec - old->tv_sec) + 1e-9 * (now->tv_nsec - old->tv_nsec);
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
stats_signal *bugle_stats_signal_new(const char *name, void *user_data,
                                     bugle_bool (*activate)(stats_signal *))
{
    stats_signal *si;

    assert(!stats_signal_get(name));
    si = BUGLE_ZALLOC(stats_signal);
    si->value = bugle_nan();
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
            return bugle_nan();
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
    return 0.0;  /* Unreachable, but keeps compilers quiet */
}

void bugle_stats_signal_update(stats_signal *si, double v)
{
    bugle_timespec now;

    bugle_gettime(&now);
    /* Integrate over time; a NaN indicates that this is the first time */
    if (bugle_isfinite(si->value))
        si->integral += time_elapsed(&si->last_updated, &now) * si->value;
    si->value = v;
    si->last_updated = now;
}

/* Convenience for accumulating signals */
void bugle_stats_signal_add(stats_signal *si, double dv)
{
    if (!bugle_isfinite(si->value))
        bugle_stats_signal_update(si, dv);
    else
        bugle_stats_signal_update(si, si->value + dv);
}

static bugle_bool stats_signal_activate(stats_signal *si)
{
    if (!si->active)
    {
        si->active = BUGLE_TRUE;
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
    sv->last_updated.tv_nsec = 0;
}

void bugle_stats_signal_values_clear(stats_signal_values *sv)
{
    bugle_free(sv->values);
    sv->values = NULL;
    sv->allocated = 0;
    sv->last_updated.tv_sec = 0;
    sv->last_updated.tv_nsec = 0;
}

void bugle_stats_signal_values_gather(stats_signal_values *sv)
{
    stats_signal *si;
    linked_list_node *s;
    size_t i;

    bugle_gettime(&sv->last_updated);

    if (sv->allocated < stats_signals_num_active)
    {
        sv->allocated = stats_signals_num_active;
        sv->values = bugle_nrealloc(sv->values, stats_signals_num_active, sizeof(stats_signal_value));
    }
    /* Make sure that everything is initialised to an insane value (NaN) */
    for (i = 0; i < stats_signals_num_active; i++)
        sv->values[i].value = sv->values[i].integral = bugle_nan();
    for (s = bugle_list_head(&stats_signals_active); s; s = bugle_list_next(s))
    {
        si = (stats_signal *) bugle_list_data(s);
        if (si->active && si->offset >= 0)
        {
            sv->values[si->offset].value = si->value;
            sv->values[si->offset].integral = si->integral;
            /* Have to update the integral from when the signal was updated
             * until this instant. */
            if (bugle_isfinite(si->value))
                sv->values[si->offset].integral +=
                    si->value * time_elapsed(&si->last_updated,
                                             &sv->last_updated);
        }
    }
}

/* Calls the callback function for each signal sub-expression in the
 * expression. The return value is the AND of the return values of all
 * the callbacks. If shortcut is BUGLE_TRUE, the function returns as soon as
 * any callback return BUGLE_FALSE, without calling the rest.
 */
static bugle_bool stats_expression_forall(stats_expression *expr, bugle_bool shortcut,
                                          bugle_bool (*callback)(stats_expression *, void *),
                                          void *arg)
{
    bugle_bool ans;
    switch (expr->type)
    {
    case STATS_EXPRESSION_NUMBER:
        return BUGLE_TRUE;
    case STATS_EXPRESSION_OPERATION:
        ans = stats_expression_forall(expr->left, shortcut, callback, arg);
        if (shortcut && !ans) return ans;
        if (expr->right)
            ans = stats_expression_forall(expr->right, shortcut, callback, arg) && ans;
        return ans;
    case STATS_EXPRESSION_SIGNAL:
        return callback(expr, arg);
    }
    return BUGLE_FALSE;  /* should never be reached */
}

/* Callback for stats_expression_activate_signals */
static bugle_bool stats_expression_activate_signals1(stats_expression *expr, void *arg)
{
    expr->signal = stats_signal_get(expr->signal_name);
    if (!expr->signal)
    {
        bugle_log_printf("stats", "expression", BUGLE_LOG_WARNING,
                         "Signal %s is not registered - missing filter-set?",
                         expr->signal_name);
        return BUGLE_FALSE;
    }
    return stats_signal_activate(expr->signal);
}

/* Initialises and activates all signals that this expression depends on.
 * It returns BUGLE_TRUE if they were successfully activated, or BUGLE_FALSE if any
 * failed to activate or were unregistered.
 */
bugle_bool bugle_stats_expression_activate_signals(stats_expression *expr)
{
    return stats_expression_forall(expr, BUGLE_FALSE, stats_expression_activate_signals1, NULL);
}

/* Checks whether this signal_name does NOT contain a *. This is because
 * the forall takes an AND rather than an OR. If the signal is generic, it
 * copies a pointer to the name into the arg, if not already present.
 */
static bugle_bool stats_expression_generic1(stats_expression *expr, void *arg)
{
    if (strchr(expr->signal_name, '*') != NULL)
    {
        char **out;
        out = (char **) arg;
        if (!*out) *out = expr->signal_name;
        return BUGLE_FALSE;
    }
    else return BUGLE_TRUE;
}

/* If the expression is generic, returns a pointer to a generic signal name
 * (which should NOT be freed). Otherwise, returns NULL.
 */
static char *stats_expression_generic(stats_expression *expr)
{
    char *generic_name = NULL;
    stats_expression_forall(expr, BUGLE_TRUE, stats_expression_generic1, &generic_name);
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
    if (!wildcard) return bugle_strdup(pattern);
    l_pattern = strlen(pattern);
    l_rep = strlen(rep);
    full = BUGLE_NMALLOC(l_pattern + l_rep, char);
    memcpy(full, pattern, wildcard - pattern);
    memcpy(full + (wildcard - pattern), rep, l_rep);
    strcpy(full + (wildcard - pattern) + l_rep, wildcard + 1);
    return full;
}

/* Checks whether instance matches pattern, where the first * is a wildcard. */
static bugle_bool pattern_match(const char *pattern, const char *instance)
{
    char *wildcard;
    size_t l_pattern, l_instance;
    size_t size1, size2;

    wildcard = strchr(pattern, '*');
    if (!wildcard)
        return strcmp(pattern, instance) == 0;

    l_pattern = strlen(pattern);
    l_instance = strlen(instance);
    if (l_instance + 1 < l_pattern) return BUGLE_FALSE;  /* instance too short */
    size1 = wildcard - pattern;
    size2 = l_pattern - 1 - size1;
    return memcmp(pattern, instance, size1) == 0
        && memcmp(wildcard + 1, instance + l_instance - size2, size2) == 0;
}

/* Assumes that pattern_match is BUGLE_TRUE, and returns the string that is the
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
    return bugle_strndup(instance + (wildcard - pattern), l_instance + 1 - l_pattern);
}

/* For a generic expression, checks whether substituting arg (a char *)
 * will yield a registered expression.
 */
static bugle_bool stats_expression_match1(stats_expression *expr, void *arg)
{
    char *full;
    bugle_bool found;

    if (!strchr(expr->signal_name, '*')) return BUGLE_TRUE; /* Not a generic */
    full = pattern_replace(expr->signal_name, (const char *) arg);
    found = stats_signal_get(full) != NULL;
    bugle_free(full);
    return found;
}

/* Instantiates a generic expression. */
static stats_expression *stats_expression_instantiate(stats_expression *base, const char *rep)
{
    stats_expression *n;

    assert(base);
    n = BUGLE_MALLOC(stats_expression);
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
        bugle_free(expr->signal_name);
        break;
    }
    bugle_free(expr);
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

void stats_statistic_free(stats_statistic *st)
{
    bugle_free(st->name);
    stats_expression_free(st->value);
    bugle_free(st->label);
    bugle_list_clear(&st->substitutions);
    bugle_free(st);
}

static stats_statistic *stats_statistic_instantiate(stats_statistic *st, const char *rep)
{
    stats_statistic *n;
    linked_list_node *i;

    n = BUGLE_MALLOC(stats_statistic);
    *n = *st;
    n->name = bugle_strdup(n->name);
    n->label = pattern_replace(n->label, rep);
    n->value = stats_expression_instantiate(st->value, rep);
    n->last = BUGLE_FALSE;

    bugle_list_init(&n->substitutions, bugle_free);
    for (i = bugle_list_head(&st->substitutions); i; i = bugle_list_next(i))
    {
        stats_substitution *su_old, *su_new;
        su_old = bugle_list_data(i);
        su_new = BUGLE_MALLOC(stats_substitution);
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
 * to the log and returns BUGLE_FALSE.
 */
static bugle_bool stats_load_config(void)
{
    const char *home;
    char *config = NULL;

    if (getenv("BUGLE_STATISTICS"))
        config = bugle_strdup(getenv("BUGLE_STATISTICS"));
    home = getenv("HOME");
    if (!config && !home)
    {
        bugle_log("stats", "config", BUGLE_LOG_ERROR,
                  "$HOME is not set; please set $HOME or $BUGLE_STATISTICS");
        return BUGLE_FALSE;
    }
    if (!config)
    {
        config = BUGLE_NMALLOC(strlen(home) + strlen(STATISTICSFILE) + 1, char);
        sprintf(config, "%s%s", home, STATISTICSFILE);
    }
    if ((stats_yyin = fopen(config, "r")) != NULL)
    {
        if (stats_yyparse() == 0)
        {
            stats_statistics = stats_statistics_get_list();
            bugle_free(config);
            return BUGLE_TRUE;
        }
        else
        {
            bugle_log_printf("stats", "config", BUGLE_LOG_ERROR,
                             "Parse error in %s", config);
            bugle_free(config);
            return BUGLE_FALSE;
        }
    }
    else
    {
        bugle_log_printf("stats", "config", BUGLE_LOG_ERROR,
                         "Failed to open %s", config);
        bugle_free(config);
        return BUGLE_FALSE;
    }
}

static bugle_bool stats_initialise(filter_set *handle)
{
    bugle_hash_init(&stats_signals, bugle_free);
    bugle_list_init(&stats_signals_active, NULL);
    bugle_hash_init(&stats_statistics_first, NULL);
    return BUGLE_TRUE;
}

/* Replaces each generic statistic from the config file with one or more
 * instances, and sets up stats_statistics_first.
 */
static bugle_bool stats_ordering_initialise(filter_set *handle)
{
    linked_list_node *i, *first, *last, *tmp;

    if (!stats_load_config()) return BUGLE_FALSE;

    /* We don't do i = bugle_list_next(i) here, because if we have a
     * pattern with no instances then we have to do the iteration before
     * the old i is deleted.
     */
    for (i = bugle_list_head(stats_statistics); i; )
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
                    if (stats_expression_forall(st->value, BUGLE_TRUE,
                                                stats_expression_match1, rep))
                    {
                        n = stats_statistic_instantiate(st, rep);
                        i = bugle_list_insert_after(stats_statistics, i, n);
                        count++;
                    }
                    bugle_free(rep);
                }
            }

            last = i;
            tmp = first;   /* The original, generic item */
            first = bugle_list_next(first);  /* The first instance */
            i = bugle_list_next(i);
            bugle_list_erase(stats_statistics, tmp);
        }
        else
            i = bugle_list_next(i);
        if (count)
        {
            stats_statistic *st;
            st = bugle_list_data(first);
            bugle_hash_set(&stats_statistics_first, st->name, first);
            st = bugle_list_data(last);
            st->last = BUGLE_TRUE;
        }
    }

    /* This filter does no interception, but it provides a sequence point
     * to allow display modules (showstats, logstats) to occur after
     * generator modules (stats_basic etc).
     */
    bugle_filter_new(handle, "stats_ordering");
    return BUGLE_TRUE;
}

static void stats_shutdown(filter_set *handle)
{
    if (stats_statistics)
        bugle_list_clear(stats_statistics);
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
        stats_shutdown,
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

    bugle_filter_set_new(&stats_info);
    bugle_filter_set_new(&stats_ordering_info);
}
