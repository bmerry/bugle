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

/* $Id: treeutils.h,v 1.13 2004/01/11 15:00:22 bruce Exp $ */
/*! \file treeutils.h
 * \brief Higher-level management of C++ parse trees
 */

#ifndef BUDGIE_TREEUTILS_H
#define BUDGIE_TREEUTILS_H

#include "tree.h"
#include <string>
#include <vector>

#define FLAG_USE 0x1          // will be used to generate dumpers
#define FLAG_TEMPORARY 0x2    // constructed on the fly, should be deleted soon
#define FLAG_ARTIFICIAL 0x4   // constructed internally, not seen in .tu file
#define FLAG_NO_RECURSE 0x8   // set_flags should not recurse past this type

// Generates a new node, which points at the base. It is a reference
// if ref is true, a pointer otherwise.
tree_node_p make_pointer(tree_node_p base);

// Similar, but creates a version of base that is not const qualified.
tree_node_p make_unconst(tree_node_p base);

// Opposite of make_unconst
tree_node_p make_const(tree_node_p base);

// Cleans up the memory allocated by the previous functions. It works
// recursively, so it should only be called on the top of a ladder
// of wrappers.
void destroy_temporary(tree_node_p node);

// Like type_to_string (see below), but if basename is not "" then
// the parameters are named basename0, basename1, ...
std::string function_type_to_string(tree_node_p type,
                                    const std::string &name,
                                    bool brackets,
                                    const std::string &basename = "");

// Creates a C description of the type. If name is "" it produces
// a pure type, otherwise a declaration (e.g. int * vs. int *x).
// brackets should be set true if name is "" or is a pointer, to ensure
// correct bracketing. Setting brackets to true is never wrong but may
// cause superfluous bracketing.
std::string type_to_string(tree_node_p type,
                           const std::string &name,
                           bool brackets);

// Produces an identifier for the given type, using rules similar to
// C++ name mangling.
std::string type_to_id(tree_node_p type);

// other utilities

// Ensures that node and all its descendants are merged into t (actually
// only nodes not already in some tree are inserted, and recursion stops
// at that point).
void insert_tree(tree &t, tree_node_p node);

// sets flags on node and its descendants.
void set_flags(tree_node_p node, unsigned int flags);

// returns a FUNCTION_DECL or TYPE_DECL for a named function. Alternately,
// if name starts with a + it is taken as a mangled type description
// and the return is some form of type node e.g. INTEGER_TYPE.
tree_node_p find_by_name(tree &t, const std::string &name);

#endif /* BUDGIE_TREEUTILS_H */
