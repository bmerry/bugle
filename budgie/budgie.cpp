/* $Id: budgie.cpp,v 1.22 2004/02/29 16:29:25 bruce Exp $ */
/*! \file budgie.cpp
 * Main program
 */

#include "tree.h"
#include "treeutils.h"
#include "generator.h"
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <iterator>
#include <utility>
#include <set>
#include <cassert>
#include <cstddef>
#include <unistd.h>
#include <sys/types.h>
#include <regex.h>

using namespace std;

tree T;  // the tree containing the entire .tu file
vector<tree_node_p> functions;   // functions that we wish to intercept
vector<tree_node_p> types;       // types that we need to handle
// newly created types for internal use
vector<pair<tree_node_p, tree_node_p> > typedefs;

// the output files
ofstream utilc, utilhead, libc, libhead, typehead, typec;

// Configuration
vector<string> libraries;        // libraries to search for symbols
vector<string> includefiles;     // files to include in the header
string tufile = "";              // the source file
string utilbase = "utils";       // output <utilbase>.c, <utilbase>.h
string libbase = "lib";          // output <libbase>.c, <libbase>.h
string typebase = "types";       // output for typedefs
regex_t limit_regex;             // limit to functions matching this
bool have_limit_regex = false;   // whether to apply the above

/* Vector of configuration files
 * These cannot be loaded by process_args directly, since
 * the processing requires the tree to already be loaded.
 */
vector<string> configfiles;

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

static void library_init()
{
    /* Write out the library initialiser
     * This looks up the original functions and saves pointers to them
     */
    utilhead << "void init_real();\n";
    utilc << "static bool initialised = false;\n";
    utilc << "void init_real()\n"
        << "{\n"
        << "    void *handles[" << libraries.size() << "];\n"
        << "    const char *names[" << libraries.size() << "] =\n"
        << "    {\n";
    // build a table of library names
    for (size_t i = 0; i < libraries.size(); i++)
    {
        utilc << "        \"" << libraries[i] << "\"";
        if (i + 1 < libraries.size()) utilc << ", ";
        utilc << "\n";
    }
    // initialise the libraries
    utilc << "    };\n"
        << "    char *error;\n"
        << "    size_t i;\n"
        << "    if (initialised) return;\n"
        << "    for (i = 0; i < " << libraries.size() << "; i++)\n"
        << "    {\n"
        << "        handles[i] = dlopen(names[i], RTLD_LAZY);\n"
        << "        if (!handles[i])\n"
        << "        {\n"
        << "            fprintf(stderr, \"%s\", dlerror());\n"
        << "            exit(1);\n"
        << "        }\n"
        << "    }\n";
    // look up the functions
    for (size_t i = 0; i < functions.size(); i++)
    {
        string name = IDENTIFIER_POINTER(DECL_NAME(functions[i]));
        tree_node_p ptr = make_pointer(TREE_TYPE(functions[i]), false);
        utilc << "    for (i = 0; i < " << libraries.size() << "; i++)\n"
            << "    {\n"
            << "        " << name
            << "_real = (" << type_to_string(ptr, "", true)
            << ") dlsym(handles[i], \"" << name << "\");\n"
            << "        if ((error = dlerror()) == NULL) break;\n"
            << "    }\n";
//            << "    if (error != NULL)\n"
//            << "        fprintf(stderr, \"Warning: %s\\n\", error);\n";
        delete ptr;
    }
    utilc << "    initialised = true;\n"
        << "}\n\n";
}

struct config_thing
{
    param_or_type p;
    vector<string> substrings;
};

static vector<config_thing> get_param_or_types(istream &in, const string &pos)
{
    string token;
    in >> token;
    vector<config_thing> answer;
    if (!in)
    {
        cerr << "Unexpected end of line" << pos;
        exit(1);
    }
    if (token != "TYPE" && token != "PARAMETER")
    {
        cerr << "Expected TYPE or PARAMETER" << pos;
        exit(1);
    }
    if (token == "PARAMETER")
    {
        string func_regex;
        int param;
        in >> func_regex >> param;
        if (!in)
        {
            cerr << "PARAMETER must be followed by function regex and parameter number" << pos;
            exit(1);
        }

        regex_t preg;
        regcomp(&preg, ("^" + func_regex + "$").c_str(), REG_EXTENDED);
        size_t group_count = 1 + count(func_regex.begin(), func_regex.end(), '(');
        regmatch_t matches[group_count];
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
                    string name = IDENTIFIER_POINTER(DECL_NAME(node));
                    if (regexec(&preg, name.c_str(),
                                group_count, matches, 0) == 0)
                    {
                        config_thing cur;
                        cur.p = param_or_type(node, param);
                        for (size_t j = 0; j < group_count; j++)
                            if (matches[j].rm_so != -1)
                                cur.substrings.push_back(name.substr(matches[j].rm_so,
                                                                     matches[j].rm_eo - matches[j].rm_so));
                        answer.push_back(cur);
                        found = true;
                    }
                }
            }
        if (!found) cerr << "Warning: no functions matched the regex `" << func_regex << "'" << pos;
        regfree(&preg);
    }
    else
    {
        string type;
        tree_node_p type_node;
        in >> type;
        if (!in)
        {
            cerr << "Expected a type" << pos;
            exit(1);
        }

        type_node = find_by_name(T, type);
        if (type_node == NULL_TREE)
        {
            cerr << "Could not find type `" << type << "'" << pos;
            exit(1);
        }
        if (TREE_CODE(type_node) == TYPE_DECL)
            type_node = TREE_TYPE(type_node);

        config_thing cur;
        cur.p = type_node;
        answer.push_back(cur);
    }
    return answer;
}

static string substitute(const string &in, const vector<string> &subst)
{
    string ans = in;
    string::size_type pos = 0;
    while ((pos = ans.find("\\", pos)) < ans.length())
    {
        if (pos + 1 == ans.length()) break; // trailing \ -- WTF?
        int num = ans[pos + 1] - '0';
        if (num < 0 || num > 9 || num >= (int) subst.size())
        {
            pos++;
            continue;
        }
        ans.replace(pos, 2, subst[num]);
        // skip over the replacement, in case it somehow has \'s
        pos += subst[num].length();
    }
    return ans;
}

// Reads a configuration file. See the man page for syntax
static void load_config(const string &name)
{
    ifstream in(name.c_str());
    string line, token;
    int lno = 0;

    while (getline(in, line))
    {
        lno++;
        ostringstream position;
        position << " (" << name << ":" << lno << ").\n";
        string pos = position.str();

        istringstream l(line);
        l >> token;
        if (!l || token[0] == '#') continue;
        if (token == "INCLUDE")
        {
            l >> ws;
            getline(l, token);
            if (!l)
            {
                cerr << "Include file name missing" << pos;
                exit(1);
            }
            includefiles.push_back(token);
        }
        else if (token == "LIBRARY")
        {
            l >> ws;
            getline(l, token);
            if (!l)
            {
                cerr << "LIBRARY file name missing" << pos;
                exit(1);
            }
            libraries.push_back(token);
        }
        else if (token == "LIMIT")
        {
            l >> ws;
            getline(l, token);
            if (!l)
            {
                cerr << "LIMIT regex missing" << pos;
                exit(1);
            }
            int result = regcomp(&limit_regex, token.c_str(), REG_NOSUB | REG_EXTENDED);
            if (result != 0)
            {
                int sz = regerror(result, &limit_regex, NULL, 0);
                char *buffer = new char[sz + 1];
                regerror(result, &limit_regex, buffer, sz + 1);
                cerr << buffer << pos;
                exit(1);
            }
            have_limit_regex = true;
        }
        else if (token == "EXTRATYPE")
        {
            string type;
            l >> type;
            tree_node_p type_node = find_by_name(T, type);
            if (type_node == NULL_TREE)
            {
                cerr << "Could not find type `" << type << "'" << pos;
                exit(1);
            }
            set_flags(type_node, FLAG_USE);
        }
        else if (token == "NEWTYPE")
        {
            l >> token;
            if (!l)
            {
                cerr << "NEWTYPE requires a subcommand" << pos;
                exit(1);
            }
            if (token == "BITFIELD")
            {
                string new_type, old_type;
                l >> new_type >> old_type;
                vector<string> tokens;
                copy(istream_iterator<string>(l),
                     istream_iterator<string>(),
                     back_inserter(tokens));

                tree_node_p old_node = find_by_name(T, old_type);
                if (old_node == NULL_TREE)
                {
                    cerr << "Could not find type `" << old_type << "'" << pos;
                    exit(1);
                }
                if (TREE_CODE(old_node) == TYPE_DECL)
                    old_node = TREE_TYPE(old_node);
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
            else
            {
                cerr << "Unknown NEWTYPE subcommand `" << token << "'" << pos;
                exit(1);
            }
        }
        else if (token == "LENGTH")
        {
            vector<config_thing> things = get_param_or_types(l, pos);
            string len;
            l >> ws;
            getline(l, len);
            for (size_t i = 0; i < things.size(); i++)
            {
                string len2 = substitute(len, things[i].substrings);
                set_length_override(things[i].p, len2);
            }
        }
        else if (token == "TYPE")
        {
            string new_type;

            vector<config_thing> things = get_param_or_types(l, pos);
            l >> ws;
            getline(l, new_type);
            for (size_t i = 0; i < things.size(); i++)
            {
                string new_type2 = substitute(new_type, things[i].substrings);
                set_type_override(things[i].p, new_type2);
            }
        }
        else if (token == "STATE")
        {
            string state_name;
            string state_type;
            int state_count;
            string state_loader;
            l >> state_name >> state_type >> state_count >> state_loader;
            if (!l)
            {
                cerr << "Invalid STATE specification" << pos;
                exit(1);
            }

            tree_node_p type_node = find_by_name(T, state_type);
            if (type_node == NULL_TREE)
            {
                cerr << "Could not find type `" << state_type << "'" << pos;
                exit(1);
            }

            set_state_spec(state_name, type_node, state_count, state_loader);
            set_flags(type_node, FLAG_USE);
        }
        else if (token == "DUMP" || token == "SAVE" || token == "LOAD")
        {
            // FIXME: save and load will be broken
            string cmd = token;
            string sub;
            vector<config_thing> things = get_param_or_types(l, pos);
            l >> sub;
            if (!l)
            {
                cerr << "Expected the name of a subroutine" << pos;
                exit(1);
            }

            for (size_t i = 0; i < things.size(); i++)
            {
                string sub2 = substitute(sub, things[i].substrings);
                if (cmd == "DUMP")
                    set_dump_override(things[i].p, sub2);
                else if (cmd == "LOAD")
                    set_load_override(things[i].p, sub2);
                else
                    set_save_override(things[i].p, sub2);
            }
        }
        else
        {
            cerr << "Unknown command `" << token << "'" << pos;
            exit(1);
        }
    }
    in.close();
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
    libc << "static bool reentrant = false;\n";
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

    utilhead << make_type_dumper(true) << "\n";
    utilc << make_type_dumper(false) << "\n";
    utilhead << get_type(true);
    utilc << get_type(false);
    utilhead << get_length(true);
    utilc << get_length(false);
    type_converter(true, utilhead);
    type_converter(false, utilc);
}

// make function pointers for the true functions
static void handle_functions()
{
    // declare the interceptor (user-defined)
    libhead << "void interceptor(function_call *);\n";

    // make function pointers for the true functions
    function_pointers(true, utilhead);
    function_pointers(false, utilc);
    // make specific call types for each function
    function_types(typehead);

    library_init();

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
