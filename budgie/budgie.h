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

#ifndef BUDGIE_BUDGIE_H
#define BUDGIE_BUDGIE_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <string>
#include <vector>
#include "budgie/bc.h"

void add_library(const std::string &);
void add_include(const std::string &);
void set_limit_regex(const std::string &);

void add_alias(const std::string &, const std::string &);
tree_node_p get_alias(tree_node_p);

tree_node_p get_type_node(const std::string &type);
param_or_type_list *find_type(const std::string &type);
param_or_type_list *find_param(const std::string &func_regex, int param);
void add_new_bitfield(const std::string &, const std::string &,
                      const std::vector<std::string> &);

void push_state(const std::string &name);
void pop_state();
void add_state_value(const std::string &name,
                     const std::string &type,
                     int count,
                     const std::string &loader);
void set_state_key(const std::string &key,
                   const std::string &compare);
void set_state_constructor(const std::string &constructor);

#endif /* !BUDGIE_BUDGIE_H */
