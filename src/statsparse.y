%{
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

#define YYERROR_VERBOSE
#define YYDEBUG 1

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <bugle/stats.h>
#include <bugle/linkedlist.h>
#include <bugle/log.h>
#include "xalloc.h"

int stats_yyerror(const char *msg);
extern int stats_yylex(void);

static stats_expression *stats_expression_new_number(double value)
{
    stats_expression *expr;

    expr = XMALLOC(stats_expression);
    expr->type = STATS_EXPRESSION_NUMBER;
    expr->op = STATS_OPERATION_NUMBER;
    expr->value = value;
    return expr;
}

static stats_expression *stats_expression_new_op(stats_operation_type op, stats_expression *left, stats_expression *right)
{
    stats_expression *expr;

    expr = XMALLOC(stats_expression);
    expr->type = STATS_EXPRESSION_OPERATION;
    expr->op = op;
    expr->left = left;
    expr->right = right;
    return expr;
}

static stats_expression *stats_expression_new_signal(stats_operation_type op, char *signal)
{
    stats_expression *expr;

    expr = XMALLOC(stats_expression);
    expr->type = STATS_EXPRESSION_SIGNAL;
    expr->op = op;
    expr->signal_name = signal;
    expr->signal = NULL;
    return expr;
}

static void stats_substitution_free(stats_substitution *sub)
{
    free(sub->replacement);
    free(sub);
}

static stats_statistic *stats_statistic_new()
{
    stats_statistic *st;

    st = XMALLOC(stats_statistic);
    st->name = NULL;
    st->value = NULL;
    st->precision = 1;
    st->maximum = 0.0;
    st->label = NULL;
    bugle_list_init(&st->substitutions, (void (*)(void *)) stats_substitution_free);
    return st;
}

static stats_substitution *stats_substitution_new(double value, char *replacement)
{
    stats_substitution *sub;

    sub = XMALLOC(stats_substitution);
    sub->value = value;
    sub->replacement = replacement;
    return sub;
}

static linked_list stats_statistics_list;

linked_list *stats_statistics_get_list(void)
{
    return &stats_statistics_list;
}

%}

%name-prefix="stats_yy"

%union
{
        double num;
        char *str;
        linked_list list;
        stats_statistic *stat;
        stats_expression *expr;
}

%token <str> ST_STRING
%token <num> ST_NUMBER
%token <str> ST_PRECISION
%token <str> ST_LABEL
%token <str> ST_MAX
%token <str> ST_SUBSTITUTE

%left <str> '-' '+'
%left <str> '*' '/'
%left <str> ST_NEG            /* unary minus */

%type <list> statistics
%type <stat> statistic
%type <expr> expr
%type <expr> signalexpr
%type <stat> attributes

%%

statistics: /* empty */           { bugle_list_init(&$$, (void (*)(void *)) stats_statistic_free); stats_statistics_list = $$; }
	| statistics statistic    { bugle_list_append(&$1, $2); stats_statistics_list = $$ = $1; }
;

statistic: ST_STRING '=' expr '{' attributes '}'
                                  { $$ = $5; $$->name = $1; $$->value = $3; }
;

expr: ST_NUMBER                   { $$ = stats_expression_new_number($1); }
	| signalexpr              { $$ = $1; }
	| expr '+' expr           { $$ = stats_expression_new_op(STATS_OPERATION_PLUS, $1, $3); }
	| expr '-' expr           { $$ = stats_expression_new_op(STATS_OPERATION_MINUS, $1, $3); }
        | expr '*' expr           { $$ = stats_expression_new_op(STATS_OPERATION_TIMES, $1, $3); }
        | expr '/' expr           { $$ = stats_expression_new_op(STATS_OPERATION_DIVIDE, $1, $3); }
        | '-' expr %prec ST_NEG   { $$ = stats_expression_new_op(STATS_OPERATION_UMINUS, $2, NULL); }
        | '(' expr ')'            { $$ = $2; }
;

signalexpr: 'd' '(' ST_STRING ')' { $$ = stats_expression_new_signal(STATS_OPERATION_DELTA, $3); }
        | 'a' '(' ST_STRING ')'   { $$ = stats_expression_new_signal(STATS_OPERATION_AVERAGE, $3); }
        | 's' '(' ST_STRING ')'   { $$ = stats_expression_new_signal(STATS_OPERATION_START, $3); }
        | 'e' '(' ST_STRING ')'   { $$ = stats_expression_new_signal(STATS_OPERATION_END, $3); }
;

attributes: /* empty */           { $$ = stats_statistic_new(); }
        | attributes ST_PRECISION ST_NUMBER
                                  { $$ = $1; $$->precision = (int) $3; }
	| attributes ST_LABEL ST_STRING
                                  { $$ = $1; free($$->label); $$->label = $3; }
        | attributes ST_MAX ST_NUMBER
                                  { $$ = $1; $$->maximum = $3; }
        | attributes ST_SUBSTITUTE ST_NUMBER ST_STRING 
                                  { $$ = $1; bugle_list_append(&$$->substitutions, stats_substitution_new($3, $4)); }
;

%%

int stats_yyerror(const char *msg)
{
    bugle_log_printf("stats", "parse", BUGLE_LOG_ERROR,
                     "parse error in statistics file: %s", msg);
    return 0;
}
