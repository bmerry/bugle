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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "src/conffile.h"
#include <bugle/memory.h>
#include <bugle/linkedlist.h>

static linked_list config_root;
int yyerror(const char *msg);
extern int yylex(void);

static void variable_free(config_variable *variable);
static void filterset_free(config_filterset *filterset);
static void chain_free(config_chain *chain);

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
%token <str> ACTIVE
%token <str> INACTIVE
%token <str> STRING
%token <str> WORD

%type <str> string
%type <list> input
%type <chain> chainitem
%type <list> chainspec
%type <filterset> filtersetitem
%type <filterset> filtersetoptions
%type <filterset> filtersetactive
%type <filterset> filtersetallocator
%type <list> filtersetspec
%type <variable> variableitem

%%

input: 		/* empty */ { bugle_list_init(&$$, (void (*)(void *)) chain_free); config_root = $$; }
		| input chainitem { bugle_list_append(&$1, $2); config_root = $$ = $1; }
;

chainitem:	CHAIN WORD '{' chainspec '}' {
			$$ = BUGLE_MALLOC(config_chain);
                        $$->name = $2;
                        $$->filtersets = $4;
	        }
;

chainspec:	/* empty */ { bugle_list_init(&$$, (void (*)(void *)) filterset_free); }
		| chainspec filtersetitem { bugle_list_append(&$1, $2); $$ = $1; }
;

filtersetitem:	FILTERSET WORD filtersetoptions {
			$$ = $3;
                        $$->name = $2;
                        bugle_list_init(&$$->variables, (void (*)(void *)) variable_free);
		}
		| FILTERSET WORD filtersetoptions '{' filtersetspec '}' {
			$$ = $3;
                        $$->name = $2;
                        $$->variables = $5;
                }
;

filtersetoptions: filtersetallocator {
			$$ = $1;
		}
                | string filtersetactive {
                	$$ = $2;
                        $$->key = $1;
                }
;

filtersetactive: filtersetallocator {
			$$ = $1;
		}
                | ACTIVE filtersetallocator {
                	$$ = $2;
                }
                | INACTIVE filtersetallocator {
                	$$ = $2;
                        $$->active = 0;
                }
;

filtersetallocator: /* empty */ {
                	$$ = BUGLE_MALLOC(config_filterset);
                        $$->name = NULL;
                        $$->key = NULL;
                        $$->active = 1;
                        bugle_list_init(&$$->variables, (void (*)(void *)) variable_free);
		}
;

filtersetspec:	/* empty */ { bugle_list_init(&$$, (void (*)(void *)) variable_free); }
		| filtersetspec variableitem { bugle_list_append(&$1, $2); $$ = $1; }
;

variableitem:	WORD string {
			$$ = BUGLE_MALLOC(config_variable);
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

const linked_list *bugle_config_get_root(void)
{
    return &config_root;
}

const config_chain *bugle_config_get_chain(const char *name)
{
    linked_list_node *i;
    for (i = bugle_list_head(&config_root); i; i = bugle_list_next(i))
        if (strcmp(((config_chain *) bugle_list_data(i))->name, name) == 0)
            return (const config_chain *) bugle_list_data(i);
    return NULL;
}

static void variable_free(config_variable *variable)
{
    bugle_free(variable->name);
    bugle_free(variable->value);
    bugle_free(variable);
}

static void filterset_free(config_filterset *filterset)
{
    bugle_free(filterset->name);
    if (filterset->key) bugle_free(filterset->key);
    bugle_list_clear(&filterset->variables);
    bugle_free(filterset);
}

static void chain_free(config_chain *chain)
{
    bugle_free(chain->name);
    bugle_list_clear(&chain->filtersets);
    bugle_free(chain);
}

void bugle_config_shutdown(void)
{
    bugle_list_clear(&config_root);
}
