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
#include "src/conffile.h"
#include "common/linkedlist.h"
#include "common/safemem.h"

static bugle_linked_list config_root;
int yyerror(const char *msg);
extern int yylex(void);

%}

%union
{
	char *str;
        bugle_linked_list list;
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

input: 		/* empty */ { bugle_list_init(&$$); config_root = $$; }
		| input chainitem { bugle_list_append(&$1, $2); config_root = $$ = $1; }
;

chainitem:	CHAIN WORD '{' chainspec '}' {
			$$ = bugle_malloc(sizeof(config_chain));
                        $$->name = $2;
                        $$->filtersets = $4;
	        }
;

chainspec:	/* empty */ { bugle_list_init(&$$); }
		| chainspec filtersetitem { bugle_list_append(&$1, $2); $$ = $1; }
;

filtersetitem:	FILTERSET WORD {
			$$ = bugle_malloc(sizeof(config_filterset));
                        $$->name = $2;
                        bugle_list_init(&$$->variables);
		}
		| FILTERSET WORD '{' filtersetspec '}' {
			$$ = bugle_malloc(sizeof(config_filterset));
                        $$->name = $2;
                        $$->variables = $4;
                }
;

filtersetspec:	/* empty */ { bugle_list_init(&$$); }
		| filtersetspec variableitem { bugle_list_append(&$1, $2); $$ = $1; }
;

variableitem:	WORD string {
			$$ = bugle_malloc(sizeof(config_variable));
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

const bugle_linked_list *bugle_config_get_root(void)
{
    return &config_root;
}

const config_chain *bugle_config_get_chain(const char *name)
{
    bugle_list_node *i;
    for (i = bugle_list_head(&config_root); i; i = bugle_list_next(i))
        if (strcmp(((config_chain *) bugle_list_data(i))->name, name) == 0)
            return (const config_chain *) bugle_list_data(i);
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
    bugle_list_node *i;
    free(filterset->name);
    for (i = bugle_list_head(&filterset->variables); i; i = bugle_list_next(i))
        destroy_variable((config_variable *) bugle_list_data(i));
    bugle_list_clear(&filterset->variables, false);
    free(filterset);
}

static void destroy_chain(config_chain *chain)
{
    bugle_list_node *i;
    free(chain->name);
    for (i = bugle_list_head(&chain->filtersets); i; i = bugle_list_next(i))
        destroy_filterset((config_filterset *) bugle_list_data(i));
    bugle_list_clear(&chain->filtersets, false);
    free(chain);
}

void bugle_config_destroy(void)
{
    bugle_list_node *i;
    for (i = bugle_list_head(&config_root); i; i = bugle_list_next(i))
        destroy_chain((config_chain *) bugle_list_data(i));
    bugle_list_clear(&config_root, false);
}
