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

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <algorithm>
#include <list>
#include <string>
#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <sys/types.h>
#include "budgie/budgie.h"

using namespace std;

#define YYERROR_VERBOSE 1
#define YYDEBUG 1
#define yylex bc_yylex

int expect_c = 0;
extern int yylex(void);

int yyerror(const char *msg)
{
    fprintf(stderr, "parse error in config file: %s\n", msg);
    return 0;
}

%}

%token <str> TYPE_ID
%token <str> ID
%token <str> NUMBER
%token <str> TEXT
%token HEADER
%token LIBRARY
%token LIMIT
%token NEWTYPE
%token BITFIELD
%token EXTRATYPE
%token LENGTH
%token <str> C_STATEMENTS
%token TYPE
%token PARAMETER
%token DUMP
%token BRACKET_PAIR
%token ALIAS

%type <str> funcregex
%type <str> text
%type <num> number
%type <str> ccode
%type <str> type
%type <str_list> idlist

%union
{
    std::list<std::string> *str_list;
    std::string *str;
    int num;
}

%%

bcfile: /* empty */
	| bcfile bcitem
;
bcitem: headeritem
	| libraryitem
	| limititem
	| aliasitem
	| newtypeitem
	| extratypeitem
        | overrideitem
;

headeritem: HEADER text { parser_header(*$2); delete $2; } ;
libraryitem: LIBRARY text { parser_library(*$2); delete $2; } ;
limititem: LIMIT funcregex { parser_limit(*$2); delete $2; } ;
newtypeitem: NEWTYPE BITFIELD ID type idlist
        { parser_bitfield(*$3, *$4, *$5); delete $3; delete $4; delete $5; }
;
extratypeitem: EXTRATYPE type
	{ parser_extra_type(*$2); delete $2; }
;
overrideitem: LENGTH TYPE type ccode
        { parser_type(OVERRIDE_LENGTH, *$3, *$4); delete $3; delete $4; }
        | LENGTH PARAMETER funcregex number ccode
        { parser_param(OVERRIDE_LENGTH, *$3, $4, *$5); delete $3; delete $5; }
	| TYPE TYPE type ccode
        { parser_type(OVERRIDE_TYPE, *$3, *$4); delete $3; delete $4; }
        | TYPE PARAMETER funcregex number ccode
        { parser_param(OVERRIDE_TYPE, *$3, $4, *$5); delete $3; delete $5; }
	| DUMP TYPE type ccode
        { parser_type(OVERRIDE_DUMP, *$3, *$4); delete $3; delete $4; }
        | DUMP PARAMETER funcregex number ccode
        { parser_param(OVERRIDE_DUMP, *$3, $4, *$5); delete $3; delete $5; }
;
aliasitem: ALIAS ID ID
	{ parser_alias(*$2, *$3); delete $2; delete $3; }
;

startc: /* empty */ { assert(yychar == YYEMPTY); expect_c = 1; } ;
ccode: startc C_STATEMENTS { $$ = $2; } ;
idlist: /* empty */ { $$ = new list<string>(); }
	| idlist ID { $$ = $1; $$->push_back(*$2); delete $2; }
;

funcregex: text ;
text: TYPE_ID
	| ID
        | NUMBER
        | TEXT
        | BRACKET_PAIR { $$ = new string("[]"); }
;
type: TYPE_ID
	| ID
;
number: NUMBER { $$ = atoi($1->c_str()); delete $1; } ;
