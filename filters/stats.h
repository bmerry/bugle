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
#include "common/bool.h"

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
    char *name;
    stats_expression *value;

    int precision;                      /* default value if unset */
    double maximum;                     /* HUGE_VAL for unset */
    char *label;
    bugle_linked_list substitutions;
} stats_statistic;

bugle_linked_list *stats_statistics_get_list(void);

#endif /* BUGLE_FILTERS_STATS_H */
