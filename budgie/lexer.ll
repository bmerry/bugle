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

#include "budgie/tree.h"
#include <cstring>
#include <string>
#include <stdlib.h>
#include <cassert>

using namespace std;

tree *yyltree;

%}

%x NODE_TYPE
%x NODE_NUMBER
DIGIT [0-9]
WHITE [ \t\n]
ID [A-Za-z_]+
STRG (.|(\n" "))+
TOKEN [A-Za-z0-9@_:<>.-]+

%%

%{
char *ptr;
char tmpc;
tree_node_p yylnode;

BEGIN(NODE_NUMBER);
%}

<NODE_NUMBER>^@{DIGIT}+	{ 
	yylnode = yyltree->get(atoi(yytext + 1));
        BEGIN(NODE_TYPE);
}
<NODE_TYPE>{ID}		{ yylnode->code = name_to_code(yytext); BEGIN(INITIAL); }

%{
/*
Note that we match "\n " so that we don't cross nodes. We suck in the
rest of the record, find the length, then replace what we don't need.
*/
%}
"strg: "{STRG}	{
	if (yylnode->length == -1)
        {
        	ptr = yytext + yyleng - 6;
                while (ptr >= yytext && memcmp(ptr, "lngt: ", 6)) ptr--;
                assert(ptr >= yytext);
                yylnode->length = atoi(ptr + 6);
        }
        tmpc = yytext[yylnode->length + 6];
        yytext[yylnode->length + 6] = '\0';
	yylnode->str = yytext + 6;
        yytext[yylnode->length + 6] = tmpc;
        yyless(yylnode->length + 6);
}

"srcp: "{TOKEN}	{
	ptr = strrchr(yytext, ':');
	tmpc = *ptr;
        *ptr = '\0';
	yylnode->filename = yytext + 6;
        yylnode->line = atoi(ptr + 1);
        *ptr = tmpc;
}

"name: "@{DIGIT}+	{ 
        switch (yylnode->code)
        {
        case TYPE_DECL:
        case NAMESPACE_DECL:
        case FUNCTION_DECL:
        case VAR_DECL:
        case FIELD_DECL:
        case CONST_DECL:
            yylnode->decl_name = yyltree->get(atoi(yytext + 7));
        default:
            yylnode->type_name = yyltree->get(atoi(yytext + 7));
        }
}

"ptd : "@{DIGIT}+	|
"refd: "@{DIGIT}+	|
"retn: "@{DIGIT}+	|
"elts: "@{DIGIT}+	|
"type: "@{DIGIT}+	{ yylnode->type = yyltree->get(atoi(yytext + 7)); }

"valu: "@{DIGIT}+	{ yylnode->value = yyltree->get(atoi(yytext + 7)); }
"purp: "@{DIGIT}+	{ yylnode->purpose = yyltree->get(atoi(yytext + 7)); }
"chan: "@{DIGIT}+	{ yylnode->chain = yyltree->get(atoi(yytext + 7)); }

"size: "@{DIGIT}+	{ yylnode->size = yyltree->get(atoi(yytext + 7)); }
"prms: "@{DIGIT}+	{ yylnode->prms = yyltree->get(atoi(yytext + 7)); }
"min : "@{DIGIT}+	{ yylnode->min = yyltree->get(atoi(yytext + 7)); }
"max : "@{DIGIT}+	{ yylnode->max = yyltree->get(atoi(yytext + 7)); }

"domn: "@{DIGIT}+	{ yylnode->values = yyltree->get(atoi(yytext + 7)); }
"flds: "@{DIGIT}+	{ yylnode->values = yyltree->get(atoi(yytext + 7)); }
"csts: "@{DIGIT}+	{ yylnode->values = yyltree->get(atoi(yytext + 7)); }
"cnst: "@{DIGIT}+	{ yylnode->init = yyltree->get(atoi(yytext + 7)); }
"unql: "@{DIGIT}+	{ yylnode->unqual = yyltree->get(atoi(yytext + 7)); }


"qual:"[ ]+[cvr]+	{
	ptr = yytext + 6;
        while (*ptr)
        {
            switch (*ptr)
            {
                case 'c': yylnode->flag_const = true; break;
                case 'v': yylnode->flag_volatile = true; break;
                case 'r': yylnode->flag_restrict = true; break;
            }
            ptr++;
        }
}

"lngt: "{DIGIT}+	{ yylnode->length = atoi(yytext + 6); }
"prec: "{DIGIT}+	{ yylnode->prec = atoi(yytext + 6); }
"low : "-?{DIGIT}+	{ yylnode->low = atoi(yytext + 6); }
"high: "-?{DIGIT}+	{ yylnode->high = atoi(yytext + 6); }

"line: "{DIGIT}+	{ yylnode->line = atoi(yytext + 6); }

"body: "@{DIGIT}+       |
"mngl: "@{DIGIT}+	|
"argt: "@{DIGIT}+	|
"bpos: "@{DIGIT}+	|
"init: "@{DIGIT}+	|
"decl: "@{DIGIT}+	|
"next: "@{DIGIT}+	|
"used: "{DIGIT}+	|
"scpe: "@{DIGIT}+	|
"op "[0-2]": "@{DIGIT}+	|
"algn: "{DIGIT}+	/* Ignore these fields */

"struct"	{ yylnode->flag_struct = true; }
"union"	{ yylnode->flag_union = true; }
"undefined"	{ yylnode->flag_undefined = true; }
"extern"	{ yylnode->flag_extern = true; }
"unsigned"	{ yylnode->flag_unsigned = true; }
"begn"	|
"end"	|
"null"	|
"clnp"	/* Ignore these tags */

[a-z][a-z ]{3}": " { printf("Unknown field %s\n", yytext); }
[a-z]+	{ printf("Unknown flag %s\n", yytext); }

\n@	{ yyless(0); return 1; }
<*>{WHITE}	/* eat whitespace */

<<EOF>>	{
		yy_delete_buffer(YY_CURRENT_BUFFER); // free memory
                yyterminate();
	}

%%

int yywrap()
{
    return 1;
}
