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

//! Returns a string containing two \c get_type functions.
std::string get_type(bool prototype);
std::string get_length(bool prototype);

//! Returns true if the type can be reasonably dumped
bool dumpable(tree_node_p type);

//! Specifies the types to be used for \c type_table and \c make_type_dumper
void specify_types(const std::vector<tree_node_p> &types);
//! Returns the code to create a type table
std::string type_table(bool header);
//! Returns the code for function \c dump_any_type
std::string make_type_dumper(bool prototype);

//! Specifies the functions to be used by generators handling functions
void specify_functions(const std::vector<tree_node_p> &funcs);
//! Returns the code to generate a function table
void function_table(bool header, std::ostream &out);
//! Returns the code to intercept/wrap a function
std::string make_wrapper(tree_node_p node, bool prototype);
//! Returns macros that can be used to define generics
void generics(std::ostream &out);
//! Generates all the function pointers needed
void function_pointers(bool prototype, std::ostream &out);
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
