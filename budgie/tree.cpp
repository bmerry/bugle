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

/* $Id: tree.cpp,v 1.9 2004/01/11 16:07:11 bruce Exp $ */

#include "tree.h"
#include <cstdio>
#include <fstream>
#include <cassert>
#include <cstring>
#include <string>
#include <vector>
#include <utility>
#include <cstdlib>

using namespace std;

extern FILE *yyin;
extern tree *yyltree;
extern int yylex(void);

typedef pair<tree_code, const char *> name_pair;

// Use tree.def from gcc 3.3.2 to map the enums to symbolic names
// for matching in the lexical scanner
#define DEFTREECODE(SYM, STRING, TYPE, NARGS) name_pair(SYM, STRING),
name_pair name_map[] =
{
#include "tree.def"
    name_pair(LAST_AND_UNUSED_TREE_CODE, "")
};
#undef DEFTREECODE

// returns the tree code corresponding to the string, or
// LAST_AND_UNUSED_TREE_CODE if not found
tree_code name_to_code(const string &name)
{
    unsigned int i = 0;
    while (name_map[i].first != LAST_AND_UNUSED_TREE_CODE
           && name_map[i].second != name)
        i++;
    return name_map[i].first;
}

// A value of -1 for the number indicates that it is not merged into
// a tree. This is used by insert_tree to identify newly created nodes.
tree_node::tree_node()
{
    value = NULL_TREE;
    chain = NULL_TREE;
    purpose = NULL_TREE;
    type = NULL_TREE;
    type_name = NULL_TREE;
    decl_name = NULL_TREE;
    prms = NULL_TREE;
    size = NULL_TREE;
    min = NULL_TREE;
    max = NULL_TREE;
    init = NULL_TREE;
    values = NULL_TREE;
    prec = 0;
    high = 0;
    low = 0;
    line = 0;
    flag_const = false;
    flag_volatile = false;
    flag_restrict = false;
    flag_undefined = false;
    flag_extern = false;
    flag_unsigned = false;
    flag_static = false;
    flag_struct = false;
    flag_union = false;
    filename = "";
    str = "";
    length = -1;
    unqual = this;

    number = -1;
    user_flags = 0;
}

tree::~tree()
{
    clear();
}

void tree::fix_name_errors()
{
    for (unsigned i = 0; i < nodes.size(); i++)
        if (exists(i))
        {
            if (DECL_NAME(nodes[i]) && TREE_CODE(DECL_NAME(nodes[i])) == TYPE_DECL)
            {
                TYPE_NAME(nodes[i]) = DECL_NAME(nodes[i]);
                DECL_NAME(nodes[i]) = NULL_TREE;
            }
            if (TYPE_NAME(nodes[i]) && TREE_CODE(TYPE_NAME(nodes[i])) == IDENTIFIER_NODE)
            {
                DECL_NAME(nodes[i]) = TYPE_NAME(nodes[i]);
                TYPE_NAME(nodes[i]) = NULL_TREE;
            }
        }
}

// Reads in the named .tu file. FIXME: no error checking.
void tree::load(const string &filename)
{
    clear();
    yyin = fopen(filename.c_str(), "r");
    yyltree = this;
    while (yylex());
    fclose(yyin);
    fix_name_errors();
}

// returns the node with the given number, creating it first if necessary
tree_node_p &tree::operator[] (unsigned int number)
{
    if (number >= nodes.size()) nodes.resize(number + 1, NULL);
    if (!nodes[number])
    {
        nodes[number] = new tree_node;
        nodes[number]->number = number;
    }
    return nodes[number];
}

bool tree::exists(unsigned int number)
{
    return number >= 0 && number < nodes.size()
        && nodes[number] != NULL;
}

void tree::clear()
{
    for (vector<tree_node_p>::size_type i = 0; i < nodes.size(); i++)
        if (nodes[i]) delete nodes[i];
    nodes.clear();
}

/* Emulation of gcc internal functions */

int cp_type_quals(tree_node_p node)
{
    if (TREE_CODE(node) == ARRAY_TYPE)
    {
        return cp_type_quals(TREE_TYPE(node));
    }
    return
        (node->flag_const ? TYPE_QUAL_CONST : 0)
        | (node->flag_volatile ? TYPE_QUAL_VOLATILE : 0)
        | (node->flag_restrict ? TYPE_QUAL_RESTRICT : 0);
}
