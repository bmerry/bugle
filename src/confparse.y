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

#define YYERROR_VERBOSE
#define YYDEBUG 1

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "src/linkedlist.h"
#include "src/conffile.h"
#include "src/safemem.h"

static linked_list config_root;
int yyerror(const char *msg);
extern int yylex(void);

%}

%union
{
	char *str;
        linked_list list;
        config_chain *chain;
        config_filterset *filterset;
        config_variable *variable;
}

%token <str> CHAIN
%token <str> FILTERSET
%token <str> STRING
%token <str> WORD

%type <str> string
%type <list> input
%type <chain> chainitem
%type <list> chainspec
%type <filterset> filtersetitem
%type <list> filtersetspec
%type <variable> variableitem

%%

input: 		/* empty */ { list_init(&$$); config_root = $$; }
		| input chainitem { list_append(&$1, $2); config_root = $$ = $1; }
;

chainitem:	CHAIN WORD '{' chainspec '}' {
			$$ = xmalloc(sizeof(config_chain));
                        $$->name = $2;
                        $$->filtersets = $4;
	        }
;

chainspec:	/* empty */ { list_init(&$$); }
		| chainspec filtersetitem { list_append(&$1, $2); $$ = $1; }
;

filtersetitem:	FILTERSET WORD {
			$$ = xmalloc(sizeof(config_filterset));
                        $$->name = $2;
                        list_init(&$$->variables);
		}
		| FILTERSET WORD '{' filtersetspec '}' {
			$$ = xmalloc(sizeof(config_filterset));
                        $$->name = $2;
                        $$->variables = $4;
                }
;

filtersetspec:	/* empty */ { list_init(&$$); }
		| filtersetspec variableitem { list_append(&$1, $2); $$ = $1; }
;

variableitem:	WORD string {
			$$ = xmalloc(sizeof(config_variable));
                        $$->name = $1;
                        $$->value = $2;
		}
;

string:		WORD { $$ = $1; }
		| STRING { $$ = $1; }
;

%%

int yyerror(const char *msg)
{
    fprintf(stderr, "parse error in config file: %s\n", msg);
    return 0;
}

const linked_list *config_get_root(void)
{
    return &config_root;
}

const config_chain *config_get_chain(const char *name)
{
    list_node *i;
    for (i = list_head(&config_root); i; i = list_next(i))
        if (strcmp(((config_chain *) list_data(i))->name, name) == 0)
            return (const config_chain *) list_data(i);
    return NULL;
}

static void destroy_variable(config_variable *variable)
{
    free(variable->name);
    free(variable->value);
    free(variable);
}

static void destroy_filterset(config_filterset *filterset)
{
    list_node *i;
    free(filterset->name);
    for (i = list_head(&filterset->variables); i; i = list_next(i))
        destroy_variable((config_variable *) list_data(i));
    list_clear(&filterset->variables, false);
    free(filterset);
}

static void destroy_chain(config_chain *chain)
{
    list_node *i;
    free(chain->name);
    for (i = list_head(&chain->filtersets); i; i = list_next(i))
        destroy_filterset((config_filterset *) list_data(i));
    list_clear(&chain->filtersets, false);
    free(chain);
}

void config_destroy(void)
{
    list_node *i;
    for (i = list_head(&config_root); i; i = list_next(i))
        destroy_chain((config_chain *) list_data(i));
    list_clear(&config_root, false);
}
