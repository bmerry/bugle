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

#include "tree.h"
#include "treeutils.h"
#include <iostream>
#include <string>
#include <sstream>
#include <stack>
#include <cassert>
#include <map>
#include <set>

using namespace std;

tree_node_p make_pointer(tree_node_p base)
{
    tree_node_p p = new tree_node;

    p->code = POINTER_TYPE;
    p->type = base;
    p->user_flags |= FLAG_TEMPORARY | FLAG_ARTIFICIAL;
    return p;
}

tree_node_p make_const(tree_node_p base)
{
    // const const is silly, so we reject it
    if (CP_TYPE_CONST_P(base)) return base;

    tree_node_p p = new tree_node;
    *p = *base;  // start by copying the base
    p->user_flags |= FLAG_TEMPORARY | FLAG_ARTIFICIAL;
    TYPE_NAME(p) = NULL;
    p->number = -1; // this node is not bound into the tree yet
    TYPE_MAIN_VARIANT(p) = base;
    // in arrays, qualifiers are actually applied to the base type
    if (TREE_CODE(p) == ARRAY_TYPE)
        TREE_TYPE(p) = make_const(TREE_TYPE(base));
    else
        p->flag_const = true;
    return p;
}

/* This one gets a little tricky, and may actually be impossible.
 * If one defines typedef const struct { ... } foo then the unconst
 * of foo is an anonymous struct. I'm not sure if this is legal C
 * through.
 * FIXME: find out
 */
tree_node_p make_unconst(tree_node_p base)
{
    if (!CP_TYPE_CONST_P(base)) return base;

    // For arrays, we must unconst the elements and re-create the
    // array type
    if (TREE_CODE(base) == ARRAY_TYPE)
    {
        tree_node_p arr = new tree_node;
        *arr = *base;
        TREE_TYPE(arr) = make_unconst(TREE_TYPE(base));
        TYPE_NAME(arr) = NULL;
        arr->user_flags |= FLAG_TEMPORARY | FLAG_ARTIFICIAL;
        arr->number = -1;
        arr->flag_const = false; // should be anyway, but just in case
        return arr;
    }
    // FIXME: using TYPE_MAIN_VARIANT could in theory discard other
    // qualifiers
    if (TYPE_MAIN_VARIANT(base) != NULL_TREE)
        return TYPE_MAIN_VARIANT(base);
    if (TYPE_NAME(base) != NULL_TREE)
        return TREE_TYPE(TYPE_NAME(base));
    // Hopefully shouldn't get here, but let's copy the type and just
    // clear the const bit
    tree_node_p p = new tree_node;
    *p = *base;
    TYPE_NAME(p) = NULL;
    p->number = -1;
    p->user_flags |= FLAG_TEMPORARY | FLAG_ARTIFICIAL;
    p->flag_const = false;
    return p;
}

void destroy_temporary(tree_node_p node)
{
    if (!node || !(node->user_flags & FLAG_TEMPORARY)) return;
    node->user_flags &= ~FLAG_TEMPORARY;
    destroy_temporary(TREE_TYPE(node));
    destroy_temporary(TYPE_MAIN_VARIANT(node));
    delete node;
}

string function_type_to_string(tree_node_p type, const string &name,
                               bool brackets, const string &basename)
{
    string base;
    ostringstream fname;
    tree_node_p cur;

    if (brackets) fname << "(";
    fname << name;
    if (brackets) fname << ")";
    fname << "(";

    cur = TREE_ARG_TYPES(type);
    int count = 0;
    while (cur != NULL_TREE && TREE_CODE(TREE_VALUE(cur)) != VOID_TYPE)
    {
        if (basename != "")
        {
            ostringstream tmp;
            tmp << basename << count;
            base = type_to_string(TREE_VALUE(cur), tmp.str(), false);
        }
        else
            base = type_to_string(TREE_VALUE(cur), "", true);

        if (count) fname << ", ";
        fname << base;

        cur = TREE_CHAIN(cur);
        count++;
    }
    if (cur == NULL_TREE && count)
        fname << ", ...";
    if (!count) fname << "void";
    fname << ")";

    return type_to_string(TREE_TYPE(type), fname.str(), false);
}

string type_to_string(tree_node_p type, const string &name, bool brackets)
{
    string ans;
    string name2;
    ostringstream o;

    if (brackets) name2 = "(" + name + ")";
    else name2 = name;

    if (type == NULL_TREE)
        return name;
    else if (TREE_CODE(type) == IDENTIFIER_NODE)
    {
        ans = IDENTIFIER_POINTER(type);
        if (name != "") ans += " ";
        return ans + name;
    }
    else if (cp_type_quals(type) != 0 && TREE_CODE(type) != ARRAY_TYPE)
    {
        /* Qualified must be handled carefully.
         * The TYPE_MAIN_VARIANT often points to an anonymous version.
         * However the TYPE_NAME is misleading, as it does not take the
         * qualifiers into account. Hence we add the qualifier, then
         * try to resolve the name without resorting to TYPE_MAIN_VARIANT
         * if possible.
         *
         * In addition, arrays must not be looked up through this path
         * as they are arrays of consts, not const arrays.
         */
        name2 = name;
        if (CP_TYPE_CONST_P(type))
            name2 = "const " + name2;
        if (CP_TYPE_VOLATILE_P(type))
            name2 = "volatile " + name2;
        /*
        if (CP_TYPE_RESTRICT_P(type))
            ans += "restrict ";
        */
        if (TYPE_NAME(type) != NULL_TREE)
            return type_to_string(TYPE_NAME(type), name2, brackets);
        else
            return type_to_string(TYPE_MAIN_VARIANT(type), name2, brackets);
    }
    else if (TYPE_NAME(type) != NULL_TREE) // handle named types
        return type_to_string(TYPE_NAME(type), name, brackets);
    else if (DECL_NAME(type) != NULL_TREE) // handle TYPE_DECLs
    {
        string tname = type_to_string(DECL_NAME(type), name, brackets);
        if (TREE_CODE(type) == RECORD_TYPE)
            tname = "struct " + tname;
        else if (TREE_CODE(type) == UNION_TYPE)
            tname = "union " + tname;
        return tname;
    }
    else if (TREE_CODE(type) == ARRAY_TYPE)
    {
        o << name2 << "[";
        // arrays are not always sized
        if (TYPE_DOMAIN(type) != NULL_TREE
            && TYPE_MAX_VALUE(TYPE_DOMAIN(type)) != NULL_TREE)
            o << TREE_INT_CST_LOW(TYPE_MAX_VALUE(TYPE_DOMAIN(type))) + 1;
        o << "]";
        return type_to_string(TREE_TYPE(type), o.str(), false);
    }
    else if (TREE_CODE(type) == POINTER_TYPE
             || TREE_CODE(type) == REFERENCE_TYPE)
    {
        if (TREE_CODE(type) == POINTER_TYPE)
            name2 = "*" + name;
        else
            name2 = "&" + name;
        return type_to_string(TREE_TYPE(type), name2, true);
    }
    else if (TREE_CODE(type) == FUNCTION_TYPE)
    {
        return function_type_to_string(type, name, brackets, "");
    }
    else if (TREE_CODE(type) == VOID_TYPE)
        return "void";

    throw anonymous_type_error("anonymous type passed to type_to_string");
}

// attempt to mimic C++ name mangling, with the difference that
// named types are always substituted with their names even if they
// can be broken down into simple types.
string type_to_id(tree_node_p type)
{
    ostringstream ans;
    bool named = false;
    tree_node_p name_node;

    if (type->flag_const)
        ans << "K";

    if (DECL_NAME(type) != NULL_TREE)
    {
        named = true;
        name_node = DECL_NAME(type);
    }
    else if (TYPE_NAME(type) != NULL_TREE && TREE_CODE(TYPE_NAME(type)) == TYPE_DECL)
    {
        named = true;
        name_node = DECL_NAME(TYPE_NAME(type));
    }

    if (named)
    {
        string name = IDENTIFIER_POINTER(name_node);

        if (name == "bool") ans << "b";
        else if (name == "char") ans << "c";
        else if (name == "unsigned char") ans << "h";
        else if (name == "signed char") ans << "a";
        else if (name == "unsigned short" || name == "short unsigned int") ans << "t";
        else if (name == "short" || name == "short int") ans << "s";
        else if (name == "unsigned int") ans << "j";
        else if (name == "int") ans << "i";
        else if (name == "unsigned long" || name == "long unsigned int") ans << "m";
        else if (name == "long" || name == "long int") ans << "l";
        else if (name == "unsigned long long") ans << "y";
        else if (name == "long long") ans << "x";
        else if (name == "unsigned __int128") ans << "o";
        else if (name == "__int128") ans << "n";
        else if (name == "float") ans << "f";
        else if (name == "double") ans << "d";
        else if (name == "long double") ans << "e";
        else if (name == "__float128") ans << "g";
        else if (name == "wchar_t") ans << "w";
        else if (name == "void") ans << "v";
        else
        {
            // The substitutions are mainly a safety precaution, but
            // the tests above do miss out on the C99 complex types
            // as well as lots of C++ constructs
            for (string::size_type i = 0; i < name.length(); i++)
            {
                char &ch = name[i];
                if ((ch < 'A' || ch > 'Z')
                    && (ch < 'a' || ch > 'z')
                    && (ch < '0' || ch > '9')
                    && ch != '_')
                    ch = '_';     // set by reference
            }
            ans << name.length() << name;
        }
    }
    else
    {
        tree_node_p cur;
        switch (TREE_CODE(type))
        {
        case VOID_TYPE:
            ans << "v";
            break;
        case POINTER_TYPE:
            ans << "P" << type_to_id(TREE_TYPE(type));
            break;
        case ARRAY_TYPE:
            ans << "A";
            if (TYPE_DOMAIN(type) != NULL_TREE)
                ans << TREE_INT_CST_LOW(TYPE_MAX_VALUE(TYPE_DOMAIN(type))) + 1;
            ans << "_" << type_to_id(TREE_TYPE(type));
            break;
        case FUNCTION_TYPE:
            ans << "F";
            ans << type_to_id(TREE_TYPE(type));
            cur = TREE_ARG_TYPES(type);
            while (cur != NULL_TREE && TREE_CODE(TREE_VALUE(cur)) != VOID_TYPE)
            {
                ans << type_to_id(TREE_VALUE(cur));
                cur = TREE_CHAIN(cur);
            }
            if (cur == NULL_TREE)
                ans << "z";
            ans << "E";
            break;
        default:
            ans << type->number;
        }
    }
    return ans.str();
}

void insert_tree(tree &t, tree_node_p node)
{
    if (!node || node->number != -1) return;
    node->user_flags &= ~FLAG_TEMPORARY;
    node->number = t.size();
    // FIXME: quick hack: t::operator [] will allocate it's own node,
    // which we free and replace with ours
    tree_node_p dummy = t[node->number];
    t[node->number] = node;
    delete dummy;
    // FIXME: should use the full range, but this is all we need for now
    insert_tree(t, TYPE_NAME(node));
    insert_tree(t, DECL_NAME(node));
    insert_tree(t, TYPE_MAIN_VARIANT(node));
    insert_tree(t, TREE_TYPE(node));
    insert_tree(t, node->values);
    insert_tree(t, node->min);
    insert_tree(t, node->max);
}

void set_flags(tree_node_p node, unsigned int flags)
{
    stack<tree_node_p> todo;
    tree_node_p cur;

    todo.push(node);
    while (!todo.empty())
    {
        cur = todo.top();
        todo.pop();
        if (cur && (cur->user_flags & flags) != flags)
        {
            cur->user_flags |= flags;
            if (cur->user_flags & FLAG_NO_RECURSE)
                continue;
            todo.push(cur->value);
            switch (TREE_CODE(cur))
            {
            case TREE_LIST:
            case FIELD_DECL:
            case VAR_DECL:
            case CONST_DECL:
                todo.push(cur->chain);
            default:;
            }
            todo.push(cur->purpose);
            todo.push(cur->type);
            todo.push(cur->unqual);
            todo.push(cur->type_name);
            todo.push(cur->decl_name);
            todo.push(cur->prms);
            // todo.push(cur->size);
            // todo.push(cur->min);
            // todo.push(cur->max);
            todo.push(cur->init);
            if (TREE_CODE(cur) != ARRAY_TYPE)
                todo.push(cur->values);
        }
    }
}

tree_node_p find_by_name(tree &t, const string &name)
{
    if (name.length() > 0 && name[0] == '+')
    {
        for (unsigned int i = 0; i < t.size(); i++)
            if (t.exists(i) && name == "+" + type_to_id(t[i]))
                return t[i];
    }
    else
    {
        for (unsigned int i = 0; i < t.size(); i++)
            if (t.exists(i))
            {
                tree_node_p cur = t[i];
                if (DECL_NAME(cur) != NULL_TREE
                    && IDENTIFIER_POINTER(DECL_NAME(cur)) == name)
                    return cur;
            }
    }
    return NULL_TREE;
}
