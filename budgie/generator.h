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

/* $Id: generator.h,v 1.4 2004/02/14 08:21:26 bruce Exp $ */
/*! \file generator.h
 * \brief Header file for code-generating code
 */

#ifndef BUDGIE_GENERATOR_H
#define BUDGIE_GENERATOR_H

#include <iostream>
#include <string>
#include <vector>
#include "tree.h"

//! A class used for overrides
/*! It contains either a (function, parameter) pair or a type.
 */
struct param_or_type
{
    tree_node_p func;
    int param;
    tree_node_p type;

    param_or_type() : func(NULL), param(-3), type(NULL) {}
    param_or_type(tree_node_p type_) : func(NULL), param(-2), type(type_) {}
    param_or_type(tree_node_p func_, int param_) : func(func_), param(param_), type(NULL) {}
    bool operator <(const param_or_type &b) const; // necessary for std::map
};

//! Generates wrapper functions for type overrides
std::string get_type(bool prototyp);
//! Generates wrapper functions for length overrides
std::string get_length(bool prototype);

//! Returns true if the type can be reasonably dumped
bool dumpable(tree_node_p type);

//! Specifies the types to be used for \c type_table and \c make_type_dumper
void specify_types(const std::vector<tree_node_p> &types);
//! Returns the code to create a type table
std::string type_table(bool header);
//! Returns the code for function \c dump_any_type
void make_type_dumper(bool prototype, std::ostream &out);
//! Creates a function to convert between arithmetic types
void type_converter(bool prototype, std::ostream &out);

//! Specifies the functions to be used by generators handling functions
void specify_functions(const std::vector<tree_node_p> &funcs);
//! Returns the code to generate a function table
void function_table(bool header, std::ostream &out);
//! Returns the code to intercept/wrap a function
std::string make_wrapper(tree_node_p node, bool prototype);
//! Returns macros that can be used to define generics
void generics(std::ostream &out);
//! Generates all the direct call function macros (header only)
void function_macros(std::ostream &out);
//! Generates the function_call type
void function_types(std::ostream &out);
//! Generates a generic invoker
void make_invoker(bool prototype, std::ostream &out);
//! Creates structures for quick and convenient access to state
void make_state_structs(bool prototype, std::ostream &out);

void set_dump_override(const param_or_type &p, const std::string &subst);
void set_load_override(const param_or_type &p, const std::string &subst);
void set_save_override(const param_or_type &p, const std::string &subst);
void set_type_override(const param_or_type &p, const std::string &new_type);
void set_length_override(const param_or_type &p, const std::string &len);
void set_state_spec(const std::string &name, tree_node_p type,
                    int count, const std::string loader);

//! returns a newly synthesized type node for dumping bitfields.
tree_node_p new_bitfield_type(tree_node_p base,
                              const std::vector<std::string> &tokens);

#endif /* BUDGIE_GENERATOR_H */
