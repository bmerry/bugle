%{
/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004  Bruce Merry
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
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "src/safemem.h"
#include "src/conffile.h"
#include "confparse.h"

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
        current_string = xrealloc(current_string, current_string_memory);
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

chain		{ return CHAIN; }
filterset	{ return FILTERSET; }
[A-Za-z_]+	{ yylval.str = xstrdup(yytext); return WORD; }
\"		{ BEGIN(STRING_MODE); }
#.*$		/* Do nothing: eats comments */
"{"|"}"		{ return yytext[0]; }
[ \t\r\n]      	/* Do nothing: eats whitespace */
.		{
			fprintf(stderr, "unexpected character in config file: %c\n", yytext[0]);
                        exit(1);
		}

<STRING_MODE>\\\n	/* Do nothing */
<STRING_MODE>\\.	{ append_char(yytext[1]); }
<STRING_MODE>\"	{ BEGIN(INITIAL); 
			yylval.str = xstrdup(current_string);
			clear_string();
			return STRING; }
<STRING_MODE><<EOF>>	{
			fprintf(stderr, "End of file in string in config file\n");
                        exit(1);
		}
<STRING_MODE>.	{ append_char(yytext[0]); }

%%

int yywrap()
{
    return 1;
}