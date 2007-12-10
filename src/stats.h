/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2006  Bruce Merry
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

#ifndef BUGLE_FILTERS_STATS_H
#define BUGLE_FILTERS_STATS_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <sys/time.h>
#include <sys/types.h>
#include "common/linkedlist.h"
#include <stdbool.h>

#if HAVE_FINITE
# define FINITE(x) (finite(x))
#elif HAVE_ISFINITE
# define FINITE(x) (isfinite(x))
#else
# define FINITE(x) ((x) != (x) && (x) != HUGE_VAL && (x) != -HUGE_VAL)
#endif

#if HAVE_ISNAN
# define ISNAN(x) isnan(x)
#else
# define ISNAN(x) ((x) != (x))
#endif

#ifndef NAN
# if HAVE_NAN
#  define NAN (nan(""))
# else
#  define NAN (0.0 / 0.0)
# endif
#endif

typedef enum
{
    STATS_EXPRESSION_NUMBER,
    STATS_EXPRESSION_OPERATION,
    STATS_EXPRESSION_SIGNAL
} stats_expression_type;

typedef enum
{
    STATS_OPERATION_NUMBER,
    STATS_OPERATION_PLUS,
    STATS_OPERATION_MINUS,
    STATS_OPERATION_TIMES,
    STATS_OPERATION_DIVIDE,
    STATS_OPERATION_UMINUS,
    STATS_OPERATION_DELTA,
    STATS_OPERATION_AVERAGE,
    STATS_OPERATION_START,
    STATS_OPERATION_END
} stats_operation_type;

/* An uninitialised signal has NaN as its value. This indicates to the
 * integrator that the last_updated field is invalid, and also means that
 * displaying the value makes it clear that no value exists.
 */
typedef struct stats_signal_s
{
    double value;
    double integral;                /* value integrated over time */
    struct timeval last_updated;
    int offset;                     /* for value tables */

    bool active;
    void *user_data;
    bool (*activate)(struct stats_signal_s *);
} stats_signal;

typedef struct stats_expression_s
{
    stats_expression_type type;
    stats_operation_type op;
    double value;                       /* for number type */
    char *signal_name;                  /* for signal types */
    stats_signal *signal;               /* for signal types */
    struct stats_expression_s *left, *right;   /* for computations (right is NULL for uminus) */
} stats_expression;

typedef struct
{
    double value;
    char *replacement;
} stats_substitution;

typedef struct
{
    char *name;
    stats_expression *value;

    int precision;                      /* default value if unset */
    double maximum;                     /* 0.0 for unset */
    char *label;
    linked_list substitutions;
    bool last;                          /* Last in an instance set */
} stats_statistic;

/*** Internal functions for the lexer and parser code ***/

linked_list *stats_statistics_get_list(void);
void statistics_initialise(void);

/*** Public API for generators ***/

/* Used by generators to list signals that they expose. Signals may not
 * be multiply registered. Slots are only assigned when signals are
 * activated.
 */
stats_signal *bugle_stats_signal_register(const char *name, void *user_data,
                                          bool (*activate)(stats_signal *));
/* Sets a new value, replacing any previous one */
void bugle_stats_signal_update(stats_signal *si, double v);

/* Convenience for accumulating signals */
void bugle_stats_signal_add(stats_signal *si, double dv);

void bugle_filter_set_stats_generator(const char *name);

/*** Public API for loggers ***/

typedef struct
{
    double value;
    double integral;
} stats_signal_value;

typedef struct
{
    int allocated;
    stats_signal_value *values;
    struct timeval last_updated;
} stats_signal_values;

void bugle_stats_signal_values_init(stats_signal_values *sv);
void bugle_stats_signal_values_clear(stats_signal_values *sv);
void bugle_stats_signal_values_gather(stats_signal_values *sv);

/* Evaluates the expression. If a signal is missing, the return is NaN. */
double bugle_stats_expression_evaluate(const stats_expression *expr,
                                       stats_signal_values *old_signals,
                                       stats_signal_values *new_signals);

stats_substitution *bugle_stats_statistic_find_substitution(stats_statistic *st, double v);
/* Returns the first of a set if it exists, or NULL if not. */
linked_list_node *bugle_stats_statistic_find(const char *name);
/* List the registered statistics, for when an illegal one is mentioned */
void bugle_stats_statistic_list(void);

bool bugle_stats_expression_activate_signals(stats_expression *expr);
void bugle_filter_set_stats_logger(const char *name);

#endif /* BUGLE_FILTERS_STATS_H */
