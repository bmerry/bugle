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
#include <cstring>
#include <cstdlib>
#include <cassert>
#include "budgie/bc.h"

using namespace std;

/* #define ECHO {} */

static int nesting = 0;

static void finish_c(string **s)
{
    *s = new string(yytext);
    BEGIN(INITIAL);
}

#define YY_USER_ACTION \
    if (expect_c) \
    { \
        BEGIN(C_CODE_START); \
        expect_c = 0; \
        nesting = 0; \
        yyless(0); \
        YY_BREAK; \
    }

%}

DIGIT	[0-9]
NUM	[-+]?[0-9]+
ID	[A-Za-z][A-Za-z0-9_]*
WS	[ \t\n]+

%x C_CODE
%x C_CODE_START
%x C_COMMENT
%x C_STRING

%%

#.*$		{ /* eat comments */ }

INCLUDE		{ return INCLUDE; }
LIBRARY		{ return LIBRARY; }
TYPE		{ return TYPE; }
LENGTH		{ return LENGTH; }
LIMIT		{ return LIMIT; }
PARAMETER	{ return PARAMETER; }
NEWTYPE		{ return NEWTYPE; }
BITFIELD	{ return BITFIELD; }
EXTRATYPE	{ return EXTRATYPE; }
DUMP		{ return DUMP; }
STATE		{ return STATE; }
KEY		{ return KEY; }
CONSTRUCTOR	{ return CONSTRUCT; } /* not CONSTRUCTOR, due to conflict with tree.def */
VALUE		{ return VALUE; }
ALIAS		{ return ALIAS; }
"[]"		{ return BRACKET_PAIR; }

"{"|"}"		{ return yytext[0]; }

\+{ID}		{ yylval.str = new string(yytext); return TYPE_ID; }
{NUM}		{ yylval.str = new string(yytext); return NUMBER; }
{ID}		{ yylval.str = new string(yytext); return ID; }
[^ \t\n]+	{ yylval.str = new string(yytext); return TEXT; }
{WS}       	{ /* eat whitespace */ }

<C_CODE_START>{WS}	{ /* eat whitespace */ }
<C_CODE_START>.	{ yyless(0); BEGIN(C_CODE); }
<C_CODE>\"	{ yymore(); BEGIN(C_STRING); }
<C_CODE>"{"|"("	{ yymore(); nesting++; }
<C_CODE>"}"|")"	{ yymore(); nesting--; }
<C_CODE>"/*"	{ yymore(); BEGIN(C_COMMENT); }
<C_CODE>\n	{ if (nesting <= 0)
		  {
                      finish_c(&yylval.str);
                      return C_STATEMENTS;
                  }
                  else
                      yymore();
		}
<C_CODE>.	{ yymore(); }

<C_STRING>\\.	|
<C_STRING>\\\n  { yymore(); }
<C_STRING>\"	{ yymore(); BEGIN(C_CODE); }
<C_STRING>.|\n	{ yymore(); }

<C_COMMENT>"*/"	{ yymore(); BEGIN(C_CODE); }
<C_COMMENT>.|\n	{ yymore(); }

<C_CODE,C_COMMENT,C_STRING><<EOF>> { fputs("EOF inside C code\n", stderr); exit(1); }

%%

int yywrap() { return 1; }
