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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <bugle/stats.h>
#include "src/statsparse.h"
#include <bugle/log.h>
#include "xalloc.h"

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
        current_string = xnrealloc(current_string, current_string_memory, sizeof(char));
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
%option never-interactive
%option prefix="stats_yy"
%option outfile="lex.yy.c"
%option noyywrap

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
                        stats_yylval.num = atof(yytext);
                        return ST_NUMBER;
                }
.		{
			bugle_log_printf("stats", "parse", BUGLE_LOG_ERROR, "unexpected character in statistics file: %c", yytext[0]);
                        exit(1);
		}

<STRING_MODE>\\\n	/* Do nothing */
<STRING_MODE>\\.	{ append_char(yytext[1]); }
<STRING_MODE>\"	{ BEGIN(INITIAL);
			stats_yylval.str = xstrdup(current_string);
			clear_string();
			return ST_STRING; }
<STRING_MODE><<EOF>>	{
			bugle_log_printf("stats", "parse", BUGLE_LOG_ERROR, "End of file in string in statistics file");
                        exit(1);
		}
<STRING_MODE>.	{ append_char(yytext[0]); }

%%
