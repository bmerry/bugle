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

#if HAVE_CONFIG_H
# include <config.h>
#endif
#ifndef _POSIX_SOURCE
# define _POSIX_SOURCE
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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "common/safemem.h"
#include "filters/stats.h"
#include "filters/statsparse.h"
#include "src/log.h"

static char *current_string = NULL;
static int current_string_length = 0;
static int current_string_memory = 0;

static void free_string(void)
{
    if (current_string) free(current_string);
}

static void append_char(char ch)
{
    current_string_length++;
    if (current_string_length >= current_string_memory)
    {
        if (current_string_memory == 0)
        {
            current_string_memory = 64;
            atexit(free_string);
        }
        current_string = bugle_realloc(current_string, current_string_memory);
    }
    current_string[current_string_length - 1] = ch;
    current_string[current_string_length] = '\0';
}

static void clear_string(void)
{
    current_string_length = 0;
    if (current_string) current_string[0] = '\0';
}

%}

%x STRING_MODE

%%

max                     { return ST_MAX; }
precision               { return ST_PRECISION; }
label                   { return ST_LABEL; }
substitute              { return ST_SUBSTITUTE; }

d|a|s|e|"{"|"}"|"+"|"-"|"*"|"/"|"="|"("|")" { return yytext[0]; }

\"		{ BEGIN(STRING_MODE); }
#.*$		/* Do nothing: eats comments */
[ \t\r\n]      	/* Do nothing: eats whitespace */
-?[0-9]+(\.[0-9]*)?|\.[0-9]+ {
                        yylval.num = atof(yytext);
                        return ST_NUMBER;
                }
.		{
			bugle_log_printf("stats", "parse", BUGLE_LOG_ERROR, "unexpected character in statistics file: %c", yytext[0]);
                        exit(1);
		}

<STRING_MODE>\\\n	/* Do nothing */
<STRING_MODE>\\.	{ append_char(yytext[1]); }
<STRING_MODE>\"	{ BEGIN(INITIAL);
			yylval.str = bugle_strdup(current_string);
			clear_string();
			return ST_STRING; }
<STRING_MODE><<EOF>>	{
			bugle_log_printf("stats", "parse", BUGLE_LOG_ERROR, "End of file in string in statistics file");
                        exit(1);
		}
<STRING_MODE>.	{ append_char(yytext[0]); }

%%

int yywrap()
{
    return 1;
}
