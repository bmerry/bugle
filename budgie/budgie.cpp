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

/* budgie is a utility to generate code for API interception. It is intended
 * to be able to stand alone from bugle, and in particular has no explicit
 * knowledge about OpenGL. However, the design was created with bugle in
 * mind, so it may be difficult to adapt for other APIs.
 */

/* $Id: budgie.cpp,v 1.22 2004/02/29 16:29:25 bruce Exp $ */
/*! \file budgie.cpp
 * Main program
 */

#include "tree.h"
#include "treeutils.h"
#include "generator.h"
#include "bc.h"
#include "budgie.h"
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <iterator>
#include <utility>
#include <stack>
#include <set>
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <unistd.h>
#include <sys/types.h>
#include <regex.h>

using namespace std;

extern FILE *bc_yyin;
extern int yyparse();

static tree T;  // the tree containing the entire .tu file
static vector<tree_node_p> functions;   // functions that we wish to intercept
static vector<tree_node_p> types;       // types that we need to handle
// newly created types for internal use
static vector<pair<tree_node_p, tree_node_p> > typedefs;

// the output files
static ofstream utilc, utilhead, libc, libhead, typehead, typec;

// Configuration
static vector<string> libraries;        // libraries to search for symbols
static vector<string> includefiles;     // files to include in the header
static string tufile = "";              // the source file
static string utilbase = "utils";       // output <utilbase>.c, <utilbase>.h
static string libbase = "lib";          // output <libbase>.c, <libbase>.h
static string typebase = "types";       // output for typedefs
static regex_t limit_regex;             // limit to functions matching this
static bool have_limit_regex = false;   // whether to apply the above

/* Vector of configuration files
 * These cannot be loaded by process_args directly, since
 * the processing requires the tree to already be loaded.
 */
static vector<string> configfiles;

/* Identify the nodes that are required. We call set_flags on the
 * type of the function to recursively tag the parameter and return
 * types. We do not call set_flags directly on the declaration as this
 * may drag in other, irrelevant nodes.
 */
static void identify()
{
    for (size_t i = 0; i < T.size(); i++)
        if (T.exists(i))
        {
            tree_node_p node = T[i];
            tree_code code = TREE_CODE(node);
            // try to eliminate various builtins.
            if (code == FUNCTION_DECL
                && DECL_SOURCE_FILE(node) != "<built-in>"
                && IDENTIFIER_POINTER(DECL_NAME(node)).substr(0, 9) != "__builtin"
                && (!have_limit_regex
                    || regexec(&limit_regex, IDENTIFIER_POINTER(DECL_NAME(node)).c_str(),
                               0, NULL, 0) == 0))
            {
                functions.push_back(node);
                set_flags(TREE_TYPE(node), FLAG_USE);
                node->user_flags |= 1;
            }
        }

    // capture these types
    size_t size = T.size(); // save, since we add along the way
    for (size_t i = 0; i < size; i++)
        if (T.exists(i) && (T[i]->user_flags & FLAG_USE))
        {
            tree_node_p node = T[i];
            // FIXME: add an IGNORETYPE command
            if (dumpable(node))
            {
                types.push_back(node);
                tree_node_p unconst = make_unconst(node);
                if (unconst->user_flags & FLAG_TEMPORARY)
		{
                    unconst->user_flags &= ~FLAG_TEMPORARY;
                    insert_tree(T, unconst);
                    types.push_back(unconst);
                }
            }
        }
}

// Reads a configuration file. See the man page for syntax
static void load_config(const string &name)
{
    bc_yyin = fopen(name.c_str(), "r");

    if (!bc_yyin)
    {
        cerr << "Could not open config file " << name << "\n";
        exit(1);
    }

    if (yyparse() != 0)
    {
        cerr << "Unknown parse error\n";
        exit(1);
    }

    fclose(bc_yyin);
}

static void process_args(int argc, char * const argv[])
{
    int opt;

    while ((opt = getopt(argc, argv, "T:t:o:l:c:")) != -1)
    {
        switch (opt)
        {
        case 'c':
            configfiles.push_back(optarg);
            break;
        case 't':
            tufile = optarg;
            break;
        case 'o':
            utilbase = optarg;
            break;
        case 'T':
            typebase = optarg;
            break;
        case 'l':
            libbase = optarg;
            break;
        case '?':
        case ':':
            exit(1);
        }
    }
}

static void headers()
{
    libc.open((libbase + ".c").c_str());
    libhead.open((libbase + ".h").c_str());
    utilc.open((utilbase + ".c").c_str());
    utilhead.open((utilbase + ".h").c_str());
    typec.open((typebase + ".c").c_str());
    typehead.open((typebase + ".h").c_str());

    utilc << "#include \"" << utilbase << ".h\"\n";
    utilc << "#include \"budgieutils.h\"\n";
    utilc << "#include \"state.h\"\n";
    utilc << "#include \"" << typebase << ".h\"\n";
    utilc << "#include <assert.h>\n\n";
    utilc << "#include <dlfcn.h>\n\n";
    utilhead << "#ifndef UTILS_H\n";
    utilhead << "#define UTILS_H\n\n";
    utilhead << "#include <stdio.h>\n";
    utilhead << "#include \"common/bool.h\"\n";
    utilhead << "#include <stdlib.h>\n";
    utilhead << "#include <string.h>\n";
    utilhead << "#include \"" << typebase << ".h\"\n";
    utilhead << "#include \"state.h\"\n";

    libc << "#include \"" << libbase << ".h\"\n";
    libc << "#include \"" << utilbase << ".h\"\n";
    libc << "#include \"budgieutils.h\"\n";
    libhead << "#ifndef LIB_H\n";
    libhead << "#define LIB_H\n\n";
    libhead << "#include \"" << typebase << ".h\"\n";

    typehead << "#ifndef TYPES_H\n";
    typehead << "#define TYPES_H\n\n";
    typehead << "#include \"budgieutils.h\"\n";

    typec << "#include \"types.h\"\n";
    typec << "#include <stddef.h>\n"; // for offsetof
    typec << "#include \"" << utilbase << ".h\"\n";
}

static void trailers()
{
    utilhead << "#endif /* !UTIL_H */\n";
    typehead << "#endif /* !TYPES_H */\n";
    libhead << "#endif /* !LIB_H */\n";
    libc.close();
    utilc.close();
    libhead.close();
    utilhead.close();
    typehead.close();
    typec.close();
}

static void tables()
{
    typehead << type_table(true);
    typec << type_table(false);
    function_table(true, typehead);
    function_table(false, typec);
    // Place this here, since some of the included files may wish
    // to have the enums defined
    for (size_t i = 0; i < includefiles.size(); i++)
        typehead << "#include \"" << includefiles[i] << "\"\n";
}

static void handle_types()
{
    // these are internally generated types, which will not be
    // listed in external headers
    for (size_t i = 0; i < typedefs.size(); i++)
    {
        typehead
            << "typedef "
            << type_to_string(typedefs[i].second,
                              IDENTIFIER_POINTER(DECL_NAME(TYPE_NAME(typedefs[i].first))),
                              false) << ";\n";
    }

    make_type_dumper(true, utilhead);
    make_type_dumper(false, utilc);
    utilhead << get_type(true);
    utilc << get_type(false);
    utilhead << get_length(true);
    utilc << get_length(false);
    type_converter(true, utilhead);
    type_converter(false, utilc);
}

static void library_table(ostream &out)
{
    out << "const char * const library_names[" << libraries.size() << "] =\n"
        << "{\n";
    for (size_t i = 0; i < libraries.size(); i++)
    {
        if (i) out << ",\n";
        out << "    \"" << libraries[i] << "\"";
    }
    out << "\n};\n"
        << "int number_of_libraries = " << libraries.size() << ";\n";
}

// make function pointers for the true functions
static void handle_functions()
{
    // declare the interceptor (user-defined)
    libhead << "void interceptor(function_call *);\n";

    // make function pointers for the true functions
    function_macros(utilhead);
    // make the function_call type and nested types
    function_types(typehead);
    // table of libraries
    library_table(typec);

    // write out function wrappers
    for (size_t i = 0; i < functions.size(); i++)
    {
        libhead << make_wrapper(functions[i], true);
        libc << make_wrapper(functions[i], false) << "\n";
    }
    make_invoker(true, utilhead);
    make_invoker(false, utilc);
    generics(utilhead);
}

static void handle_state()
{
    make_state_structs(true, utilhead);
    make_state_structs(false, utilc);
}

int main(int argc, char * const argv[])
{
    process_args(argc, argv);

    T.load(tufile);
    for_each(configfiles.begin(), configfiles.end(), load_config);

    identify();           // identify types and functions to use
    specify_types(types);
    specify_functions(functions);

    headers();            // initialise files
    tables();
    handle_types();       // type tables, dumping etc
    handle_functions();   // function tables, wrappers, invoker etc.
    handle_state();       // state structures
    trailers();           // clean up files

    T.clear();
    if (have_limit_regex)
        regfree(&limit_regex);
    return 0;
}

/* Here we put the routines called by the parser */
void add_include(const string &s)
{
    includefiles.push_back(s);
}

void add_library(const string &s)
{
    libraries.push_back(s);
}

void set_limit_regex(const string &s)
{
    int result = regcomp(&limit_regex, s.c_str(), REG_NOSUB | REG_EXTENDED);
    if (result != 0)
    {
        int sz = regerror(result, &limit_regex, NULL, 0);
        char *buffer = new char[sz + 1];
        regerror(result, &limit_regex, buffer, sz + 1);
        cerr << buffer;
        exit(1);
    }
    have_limit_regex = true;
}

tree_node_p get_type_node(const string &type)
{
    tree_node_p ans = find_by_name(T, type);
    if (ans == NULL_TREE)
    {
        cerr << "Could not find type `" << type << "'";
        exit(1);
    }
    if (TREE_CODE(ans) == TYPE_DECL)
        return TREE_TYPE(ans);
    else
        return ans;
}

param_or_type_list *find_type(const string &type)
{
    param_or_type_list *ans;

    ans = new param_or_type_list;
    ans->nmatch = 0;
    ans->pt_list.push_back(param_or_type_match());
    ans->pt_list.back().pt = param_or_type(get_type_node(type));
    ans->pt_list.back().text = type;
    ans->pt_list.back().pmatch = NULL;
    return ans;
}

param_or_type_list *find_param(const string &func_regex, int param)
{
    param_or_type_list *ans = new param_or_type_list;
    string name;

    regex_t preg;
    regcomp(&preg, ("^" + func_regex + "$").c_str(), REG_EXTENDED);
    ans->nmatch = preg.re_nsub + 1;
    regmatch_t *matches = new regmatch_t[ans->nmatch];
    bool found = false;
    for (size_t i = 0; i < T.size(); i++)
        if (T.exists(i))
        {
            tree_node_p node = T[i];
            tree_code code = TREE_CODE(node);
            // try to eliminate various builtins.
            if (code == FUNCTION_DECL
                && DECL_SOURCE_FILE(node) != "<built-in>"
                && IDENTIFIER_POINTER(DECL_NAME(node)).substr(0, 9) != "__builtin")
            {
                name = IDENTIFIER_POINTER(DECL_NAME(node));
                if (regexec(&preg, name.c_str(),
                            ans->nmatch, matches, 0) == 0)
                {
                    ans->pt_list.push_back(param_or_type_match());
                    ans->pt_list.back().pt = param_or_type(node, param);
                    ans->pt_list.back().text = name;
                    ans->pt_list.back().pmatch = matches;
                    ans->pt_list.back().code = "";
                    found = true;
                    matches = new regmatch_t[ans->nmatch];
                }
            }
        }
    delete[] matches;
    if (!found)
        cerr << "Warning: no functions matched the regex `" << func_regex << "'";

    regfree(&preg);
    return ans;
}

void add_new_bitfield(const string &new_type,
                      const string &old_type,
                      const vector<string> &tokens)
{
    tree_node_p old_node = get_type_node(old_type);
    tree_node_p new_node = new_bitfield_type(old_node, tokens);
    tree_node_p id = new tree_node;
    // create an ID node so that find_by_name will pick up the
    // new type
    TREE_CODE(id) = IDENTIFIER_NODE;
    IDENTIFIER_POINTER(id) = new_type;
    id->user_flags |= FLAG_ARTIFICIAL;
    DECL_NAME(TYPE_NAME(new_node)) = id;
    insert_tree(T, new_node);
    set_flags(new_node, FLAG_USE);
    typedefs.push_back(make_pair(new_node, old_node));
}

static gen_state_tree *current_state = &root_state;
static stack<gen_state_tree *> state_stack;

void push_state(const string &name)
{
    for (size_t i = 0; i < current_state->children.size(); i++)
    {
        if (name == current_state->children[i]->name)
        {
            cerr << "duplicate state name (" << name << ")\n";
            exit(1);
        }
    }
    state_stack.push(current_state);

    current_state->children.push_back(new gen_state_tree());
    current_state = current_state->children.back();
    current_state->name = name;
}

void pop_state()
{
    assert(!state_stack.empty());
    current_state = state_stack.top();
    state_stack.pop();
}

void add_state_value(const std::string &name,
                     const std::string &type,
                     int count,
                     const std::string &loader)
{
    push_state(name);
    current_state->count = count;
    current_state->type = get_type_node(type);
    current_state->loader = loader;
    pop_state();
}

void set_state_key(const std::string &key,
                   const std::string &compare)
{
    current_state->key_type = get_type_node(key);
    current_state->key_compare = compare;
}

void set_state_constructor(const std::string &constructor)
{
    current_state->constructor = constructor;
}
