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
#include <algorithm>
#include <vector>
#include <string>
#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <sys/types.h>
#include <regex.h>
#include "budgie/bc.h"
#include "budgie/tree.h"
#include "budgie/treeutils.h"
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

static void pt_substitute(param_or_type_list *l, const string &code)
{
    for (list<param_or_type_match>::iterator i = l->pt_list.begin(); i != l->pt_list.end(); i++)
    {
        if (i->pt.type)
        {
            i->code = code;
            continue;
        }

        string ans = code;
        string::size_type pos = 0;
        while ((pos = ans.find("\\", pos)) < ans.length())
        {
            string::size_type pos2 = pos + 1;
            while (pos2 < ans.length()
                   && ans[pos2] >= '0'
                   && ans[pos2] <= '9') pos2++;
            int num = atoi(ans.substr(pos + 1, pos2 - pos - 1).c_str());
            if (pos2 == pos + 1 || num < 0 || num >= (int) l->nmatch)
            {
                pos++;
                continue;
            }
            string subst = i->text.substr(i->pmatch[num].rm_so,
                                          i->pmatch[num].rm_eo - i->pmatch[num].rm_so);
            ans.replace(pos, pos2 - pos, subst);
            // skip over the replacement, in case it somehow has \'s
            pos += subst.length();
        }
        i->code = ans;
    }
}

static void pt_foreach(param_or_type_list *l,
                       void (*func)(const param_or_type &, const string &))
{
    list<param_or_type_match>::const_iterator i;
    for (i = l->pt_list.begin(); i != l->pt_list.end(); i++)
        (*func)(i->pt, i->code);
}

%}

%token <str> TYPE_ID
%token <str> ID
%token <str> NUMBER
%token <str> TEXT
%token INCLUDE
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
%token STATE
%token KEY
%token CONSTRUCT
%token VALUE

%type <str> funcregex
%type <str> text
%type <num> number
%type <str> stateid
%type <str> ccode
%type <str> type
%type <pt> typeparam
%type <pt> typeparamcode
%type <str_list> idlist

%union
{
    std::vector<std::string> *str_list;
    std::string *str;
    param_or_type_list *pt;
    int num;
    tree_node_p node;
}

%%

bcfile: /* empty */
	| bcfile bcitem
;

bcitem: includeitem
	| libraryitem
        | limititem
        | newtypeitem
        | extratypeitem
        | lengthitem
        | typeitem
        | dumpitem
        | statetree
;

includeitem:
	INCLUDE text
        { add_include(*$2); delete $2; }
;
libraryitem:
	LIBRARY text
        { add_library(*$2); delete $2; }
;
limititem:
	LIMIT funcregex
        { set_limit_regex(*$2); delete $2; }
;
newtypeitem:
	NEWTYPE BITFIELD ID type idlist
        { add_new_bitfield(*$3, *$4, *$5); delete $4; delete $5; }
;
extratypeitem: EXTRATYPE type
	{ set_flags(get_type_node(*$2), FLAG_USE); delete $2; }
;
lengthitem:
	LENGTH typeparamcode
        { pt_foreach($2, set_length_override); delete $2; }
;
typeitem: TYPE typeparamcode
        { pt_foreach($2, set_type_override); delete $2; }
;
dumpitem: DUMP typeparamcode
        { pt_foreach($2, set_dump_override); delete $2; }
;
statetree: 
	stateheader '{' stateitems '}' { pop_state(); }
;
stateheader:
	STATE stateid { push_state(*$2); delete $2; }
;

stateitems: /* empty */
	| stateitems KEY type ID
        { set_state_key(*$3, *$4); delete $3; delete $4; }
        | stateitems CONSTRUCT ccode
        { set_state_constructor(*$3); delete $3; }
        | stateitems VALUE ID type number ID
        { add_state_value(*$3, *$4, $5, *$6); delete $3; delete $4; delete $6; }
        | stateitems statetree
;

typeparam:
	TYPE type
        { $$ = find_type(*$2); delete $2; }
        | PARAMETER funcregex number
        { $$ = find_param(*$2, $3); delete $2; }
;
typeparamcode:
	typeparam ccode { $$ = $1; pt_substitute($1, *$2); delete $2; }
;
startc: /* empty */ { assert(yychar == YYEMPTY); expect_c = 1; } ;
ccode: startc C_STATEMENTS { $$ = $2; } ;
idlist: /* empty */ { $$ = new vector<string>(); }
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
stateid: ID
	| BRACKET_PAIR { $$ = new string("[]"); }
;
number: NUMBER { $$ = atoi($1->c_str()); delete $1; } ;
