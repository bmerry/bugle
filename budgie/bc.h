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

#ifndef BUGLE_BUDGIE_BC_H
#define BUGLE_BUDGIE_BC_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <list>
#include <string>
#include <cstddef>
#include <regex.h>
#include "budgie/tree.h"
#include "budgie/generator.h"

extern int expect_c;
extern int bc_yylex(void);

struct param_or_type_match
{
    param_or_type pt;

    std::string text;
    regmatch_t *pmatch;
    std::string code;
};

struct param_or_type_list
{
    std::list<param_or_type_match> pt_list;
    std::size_t nmatch;

    ~param_or_type_list()
    { for (std::list<param_or_type_match>::iterator i = pt_list.begin();
           i != pt_list.end(); i++)
        if (i->pmatch) delete[] i->pmatch;
    }
};

extern void *get_type_tree(const std::string &s);
extern void *get_function_tree(const std::string &s);

#include "budgie/bcparser.h"

#endif /* !BUGLE_BUDGIE_BC_H */
