%{
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

#define YYERROR_VERBOSE
#define YYDEBUG 1

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define yymaxdepth stats_yymaxdepth
#define yyparse stats_yyparse
#define yylex stats_yylex
#define yyerror stats_yyerror
#define yylval stats_yylval
#define yychar stats_yychar
#define yydebug stats_yydebug
#define yypact stats_yypact
#define yyr1 stats_yyr1
#define yyr2 stats_yyr2
#define yydef stats_yydef
#define yychk stats_yychk
#define yypgo stats_yypgo
#define yyact stats_yyact
#define yyexca stats_yyexca
#define yyerrflag stats_yyerrflag
#define yynerrs stats_yynerrs
#define yyps stats_yyps
#define yypv stats_yypv
#define yys stats_yys
#define yy_yys stats_yys
#define yystate stats_yystate
#define yytmp stats_yytmp
#define yyv stats_yyv
#define yy_yyv stats_yyv
#define yyval stats_yyval
#define yylloc stats_yylloc
#define yyreds stats_yyreds
#define yytoks stats_yytoks
#define yylhs stats_yylhs
#define yylen stats_yylen
#define yydefred stats_yydefred
#define yydgoto stats_yydgoto
#define yysindex stats_yysindex
#define yyrindex stats_yyrindex
#define yygindex stats_yygindex
#define yytable stats_yytable
#define yycheck stats_yycheck
#define yyname stats_yyname
#define yyrule stats_yyrule

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "filters/stats.h"
#include "common/linkedlist.h"
#include "common/safemem.h"

int yyerror(const char *msg);
extern int yylex(void);

static stats_expression *stats_expression_new_number(double value)
{
    stats_expression *expr;

    expr = (stats_expression *) bugle_malloc(sizeof(stats_expression));
    expr->type = STATS_EXPRESSION_NUMBER;
    expr->op = STATS_OPERATION_NUMBER;
    expr->value = value;
    return expr;
}

static stats_expression *stats_expression_new_op(stats_operation_type op, stats_expression *left, stats_expression *right)
{
    stats_expression *expr;

    expr = (stats_expression *) bugle_malloc(sizeof(stats_expression));
    expr->type = STATS_EXPRESSION_OPERATION;
    expr->op = op;
    expr->left = left;
    expr->right = right;
    return expr;
}

static stats_expression *stats_expression_new_signal(stats_operation_type op, char *signal)
{
    stats_expression *expr;

    expr = (stats_expression *) bugle_malloc(sizeof(stats_expression));
    expr->type = STATS_EXPRESSION_SIGNAL;
    expr->op = op;
    expr->signal_name = signal;
    expr->signal = NULL;
    return expr;
}

static stats_statistic *stats_statistic_new()
{
    stats_statistic *st;

    st = (stats_statistic *) bugle_malloc(sizeof(stats_statistic));
    st->name = NULL;
    st->value = NULL;
    st->precision = 1;
    st->maximum = HUGE_VAL;
    st->label = NULL;
    bugle_list_init(&st->substitutions, true);
    return st;
}

static bugle_linked_list stats_statistics_list;

bugle_linked_list *stats_statistics_get_list(void)
{
    return &stats_statistics_list;
}

%}

%union
{
        double num;
        char *str;
        bugle_linked_list list;
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

statistics: /* empty */           { bugle_list_init(&$$, false); stats_statistics_list = $$; }
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
                                  { $$ = $1; /* FIXME */ }
;

%%

int yyerror(const char *msg)
{
    fprintf(stderr, "parse error in statistics file: %s\n", msg);
    return 0;
}
