/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2007  Bruce Merry
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

/* budgie is a utility to generate code for API interception. It is intended
 * to be able to stand alone from bugle, and in particular has no explicit
 * knowledge about OpenGL. However, the design was created with bugle in
 * mind, so it may be difficult to adapt for other APIs.
 */

#include "tree.h"
#include "treeutils.h"
#include "budgie.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <set>
#include <list>
#include <vector>
#include <utility>
#include <algorithm>
#include <map>
#include <cstdio>
#include <cassert>
#include <memory>
#include <regex.h>
#include <unistd.h>

using namespace std;

/* Raw text */
struct Override
{
    override_type mode;
    string functions;
    int param;
    string type;
    string code;
};

struct Type
{
    int index;
    tree_node_p node;
    tree_node_p unconst;
    map<override_type, string> overrides;

    string type_name() const;  /* e.g. "int *" */
    string name() const;
    string define() const;

    bool operator <(const Type &b) const { return name() < b.name(); }
};

struct Bitfield
{
    string new_type;
    string old_type;
    list<string> bits;

    tree_node_p new_node;
};

struct Parameter
{
    list<Type>::iterator type;
    map<override_type, string> overrides;
};

struct Group;

struct Function
{
    int index;
    tree_node_p node;
    list<Function>::iterator parent; // used while building groups
    list<Group>::iterator group;

    string name() const;
    string define() const;
    string group_define() const;

    bool operator <(const Function &b) const
    { return name() < b.name(); }
};

struct Group
{
    typedef list<Function>::iterator FunctionIterator;
    typedef list<FunctionIterator>::iterator iterator;

    int index;
    list<FunctionIterator> functions;
    vector<Parameter> parameters;
    bool has_retn;
    Parameter retn;

    Parameter &parameter(int);
    const Parameter &parameter(int) const;

    FunctionIterator canonical() const;
    string name() const;
    string define() const;
};

/* Core data structures */
static tree T;
static list<Group> groups;
static list<Function> functions;
static list<Type> types;

/* Configuration and command line stuff */
static string tufile, typesbase, utilbase, namebase, libbase;
static string limit = "";
static list<string> headers, libraries;
static list<Override> overrides;
static list<pair<string, string> > aliases;
static list<string> extra_types;
static list<Bitfield> bitfields;

/* Yacc stuff */
extern FILE *bc_yyin;
extern int yyparse();

/* File handles */
static FILE *name_h, *util_h, *types_h, *lib_h;
static FILE *name_c, *util_c, *types_c, *lib_c;

/* Utility data */
static map<string, list<Function>::iterator> function_map;
static map<tree_node_p, list<Type>::iterator> type_map;

/* Utility functions */
static string search_replace(const string &s, const string &old, const string &nu)
{
    string ans = s;
    string::size_type pos = 0;

    while ((pos = ans.find(old, pos)) != string::npos)
    {
        ans.replace(pos, old.length(), nu);
        pos += nu.length(); /* avoid infinite loops */
    }
    return ans;
}

static void die(const string &error)
{
    cerr << error;
    exit(1);
}

static void pdie(const string &error)
{
    perror(error.c_str());
    exit(1);
}

static void die_regex(int errcode, const regex_t *preg)
{
    size_t sz = regerror(errcode, preg, NULL, 0);
    char *buffer = new char[sz + 1];
    auto_ptr<char> buffer_wrapper(buffer);
    sz = regerror(errcode, preg, buffer, sz); /* sz includes the NULL */
    buffer[sz] = '\0';
    buffer[sz - 1] = '\n';
    die(buffer);
}

static tree_node_p get_type_node_test(const string &type)
{
    tree_node_p ans = find_by_name(T, type);
    if (ans == NULL_TREE) return ans;
    else if (TREE_CODE(ans) == TYPE_DECL)
        return TREE_TYPE(ans);
    else
        return ans;
}

tree_node_p get_type_node(const string &type)
{
    tree_node_p ans = get_type_node_test(type);
    if (ans == NULL_TREE)
    {
        die("Could not find type '" + type + "'\n");
        return NULL;  // never executed, but suppresses warning
    }
    else
        return ans;
}

string parameter_number(int i)
{
    ostringstream s;

    if (i == -1) s << "retn"; else s << i;
    return s.str();
}

/* Real code */

static void process_args(int argc, char * const argv[])
{
    int opt;

    while ((opt = getopt(argc, argv, "n:t:T:o:l:")) != -1)
    {
        switch (opt)
        {
        case 't':
            tufile = optarg;
            break;
        case 'T':
            typesbase = optarg;
            break;
        case 'o':
            utilbase = optarg;
            break;
        case 'n':
            namebase = optarg;
            break;
        case 'l':
            libbase = optarg;
            break;
        case '?':
        case ':':
            exit(1);
        }
    }
    if (optind + 1 != argc) die("Must specify a config file\n");
}

static void load_config(const string &name)
{
    bc_yyin = fopen(name.c_str(), "r");

    if (!bc_yyin)
        die("Could not open config file " + name + "\n");
    if (yyparse() != 0) die("Unknown parse error\n");

    fclose(bc_yyin);
}

static bool dumpable(tree_node_p type)
{
    switch (TREE_CODE(type))
    {
    case INTEGER_TYPE:
    case REAL_TYPE:
    case BOOLEAN_TYPE:
    case POINTER_TYPE:
    case REFERENCE_TYPE:
        return true;
    case ENUMERAL_TYPE:
    case RECORD_TYPE:
    case UNION_TYPE:
        // anonymous types are impossible to name for dumping, and
        // if the types are undefined then there is nothing to dump
        if ((DECL_NAME(type) || TYPE_NAME(type)) && TYPE_VALUES(type)) return true;
        else return false;
    case ARRAY_TYPE:
        // unspecified array sizes make dumping impossible
        if (TYPE_DOMAIN(type) != NULL_TREE) return true;
        else return false;
    default:
        return false;
    }
}


/* Identify the nodes that are required. We call set_flags on the
 * type of the function to recursively tag the parameter and return
 * types. We do not call set_flags directly on the declaration as this
 * may drag in other, irrelevant nodes.
 */
static void identify()
{
    map<string, tree_node_p> seen_types;
    bool have_limit = false;
    regex_t limit_regex;

    if (limit != "")
    {
        int result = regcomp(&limit_regex, limit.c_str(), REG_NOSUB | REG_EXTENDED);
        if (result != 0) die_regex(result, &limit_regex);
        have_limit = true;
    }

    for (size_t i = 0; i < T.size(); i++)
        if (T.exists(i))
        {
            tree_node_p node = T[i];
            tree_code code = TREE_CODE(node);
            if (code != FUNCTION_DECL) continue;
            // try to eliminate various builtins.
            string name = IDENTIFIER_POINTER(DECL_NAME(node));
            if (code == FUNCTION_DECL
                && DECL_SOURCE_FILE(node) != "<built-in>"
                && name.substr(0, 9) != "__builtin"
                && (!have_limit
                    || regexec(&limit_regex, name.c_str(),
                               0, NULL, 0) == 0))
            {
                functions.push_back(Function());
                functions.back().node = node;
                functions.back().parent = functions.end();
                functions.back().parent--;

                set_flags(TREE_TYPE(node), FLAG_USE);
                node->user_flags |= FLAG_USE;
            }
        }

    if (have_limit) regfree(&limit_regex);

    /* Add extra types that were explicitly requested */
    for (list<string>::iterator i = extra_types.begin(); i != extra_types.end(); i++)
    {
        set_flags(get_type_node(*i), FLAG_USE);
    }

    // capture these types
    /* Note: GCC 4.1 seems to cause some types to be duplicated. We keep
     * track of types that have already been seen and only push them once.
     * We also construct the type map here so that duplicates can still
     * be examined through the type map.
     */
    size_t size = T.size();
    for (size_t i = 0; i < size; i++)
        if (T.exists(i) && (T[i]->user_flags & FLAG_USE))
        {
            tree_node_p node = T[i];
            if (dumpable(node))
            {
                string name = type_to_id(node);
                // FIXME: add an IGNORETYPE command
                if (name == "GLubyte")
                    cout << "";
                if (seen_types.count(name))
                {
                    assert(type_to_id(seen_types[name]) == name);
                    type_map[node] = type_map[seen_types[name]];
                    continue;
                }
                seen_types[name] = node;

                tree_node_p unconst = make_unconst(node);
                types.push_back(Type());
                types.back().node = node;
                types.back().unconst = unconst;
                type_map[node] = types.end(); type_map[node]--;
                if (unconst->user_flags & FLAG_TEMPORARY)
		{
                    unconst->user_flags &= ~FLAG_TEMPORARY;
                    insert_tree(T, unconst);
                    types.push_back(Type());
                    types.back().node = unconst;
                    types.back().unconst = unconst;
                    type_map[unconst] = types.end(); type_map[unconst]--;
                }
            }
        }
    types.sort();
}

static void make_function_map()
{
    functions.sort();
    for (list<Function>::iterator i = functions.begin(); i != functions.end(); i++)
        function_map[i->name()] = i;
}

static list<Type>::iterator get_type_map(tree_node_p node)
{
    assert(type_map.count(node));
    return type_map[node];
}

static list<Function>::iterator get_function_root(list<Function>::iterator f)
{
    while (f != f->parent) f = f->parent;
    return f;
}

static void make_groups()
{
    for (list<pair<string, string> >::iterator i = aliases.begin(); i != aliases.end(); i++)
    {
        string aname = i->first;
        string bname = i->second;
        if (!function_map.count(aname))
        {
            cerr << "Warning: no such function '" << aname << "'\n";
            continue;
        }
        if (!function_map.count(bname))
        {
            cerr << "Warning: no such function '" << bname << "'\n";
            continue;
        }
        list<Function>::iterator aroot = get_function_root(function_map[aname]);
        list<Function>::iterator broot = get_function_root(function_map[bname]);
        if (aroot != broot) aroot->parent = broot;
    }

    for (list<Function>::iterator i = functions.begin(); i != functions.end(); i++)
    {
        if (i == i->parent)
        {
            groups.push_back(Group());
            i->group = groups.end();
            i->group--;
            groups.back().functions.push_back(i);
        }
    }

    for (list<Function>::iterator i = functions.begin(); i != functions.end(); i++)
    {
        if (i != i->parent)
        {
            i->group = get_function_root(i)->group;
            i->group->functions.push_back(i);
        }
    }

    for (list<Group>::iterator i = groups.begin(); i != groups.end(); i++)
    {
        int count = 0;
        tree_node_p type = TREE_TYPE(i->canonical()->node);
        tree_node_p cur = TREE_ARG_TYPES(type);
        while (cur != NULL_TREE && TREE_CODE(cur->value) != VOID_TYPE)
        {
            cur = TREE_CHAIN(cur);
            count++;
        }
        i->parameters.reserve(count);

        count = 0;
        cur = TREE_ARG_TYPES(type);
        while (cur != NULL_TREE && TREE_CODE(cur->value) != VOID_TYPE)
        {
            i->parameters.push_back(Parameter());
            i->parameters.back().type = get_type_map(TREE_VALUE(cur));
            cur = TREE_CHAIN(cur);
            count++;
        }
        if (TREE_CODE(TREE_TYPE(type)) != VOID_TYPE)
        {
            i->has_retn = true;
            i->retn.type = get_type_map(TREE_TYPE(type));
        }
        else
            i->has_retn = false;
    }
}

// applies the backreferences and $-substitutions
static string group_substitute(const Override &o,
                               const Group &g,
                               const string &text,
                               size_t nmatch,
                               const regmatch_t *pmatch)
{
    string ans = o.code;
    string::size_type pos = 0;

    while ((pos = ans.find("\\", pos)) < ans.length())
    {
        string::size_type pos2 = pos + 1;
        while (pos2 < ans.length()
               && ans[pos2] >= '0'
               && ans[pos2] <= '9') pos2++;
        int num = atoi(ans.substr(pos + 1, pos2 - pos - 1).c_str());
        if (pos2 == pos + 1 || num < 0 || num >= (int) nmatch)
        {
            pos++;
            continue;
        }
        string subst = text.substr(pmatch[num].rm_so,
                                   pmatch[num].rm_eo - pmatch[num].rm_so);
        ans.replace(pos, pos2 - pos, subst);
        // skip over the replacement, in case it somehow has \'s
        pos += subst.length();
    }

    ans = search_replace(ans, "$f", "(" + g.define() + ")");
    ans = search_replace(ans, "$l", "(length)");
    if (o.mode == OVERRIDE_DUMP)
        ans = search_replace(ans, "$F", "(out)");

    /* We count downwards because there may be more than 10 parameters,
     * in which case we must do the multi-digit ones first.
     */
    int start = (int) g.parameters.size() - 1;
    int end = (g.has_retn ? -1 : 0);
    for (int j = start; j >= end; j--)
    {
        tree_node_p tmp;
        const Parameter &p = g.parameter(j);
        const string n = parameter_number(j);
        const string define = p.type->define();

        tmp = make_pointer(make_const(p.type->node));
        string type = type_to_string(tmp, "", false);
        if (j >= 0)
        {
            ans = search_replace(ans, "$" + n, "(*(" + type + ") call->args[" + n + "])");
            ans = search_replace(ans, "$t" + n, "(" + define + ")");
            if (o.param == j)
            {
                ans = search_replace(ans, "$$", "(*(" + type + ") call->args[" + n + "])");
                ans = search_replace(ans, "$t$", "(" + define + ")");
            }
        }
        else
        {
            ans = search_replace(ans, "$r", "(*(" + type + ") call->retn)");
            ans = search_replace(ans, "$tr", "(" + define + ")");
            if (o.param == j)
            {
                ans = search_replace(ans, "$$", "(*(" + type + ") call->retn)");
                ans = search_replace(ans, "$t$", "(" + define + ")");
            }
        }

        destroy_temporary(tmp);
    }

    return ans;
}

static void make_overrides()
{
    for (list<Override>::iterator i = overrides.begin(); i != overrides.end(); i++)
    {
        if (!i->functions.empty())
        {
            regex_t regex;
            int result;
            bool any_matches = false;

            result = regcomp(&regex, i->functions.c_str(), REG_EXTENDED);
            if (result != 0) die_regex(result, &regex);

            for (list<Group>::iterator j = groups.begin(); j != groups.end(); j++)
            {
                bool matches = false;
                list<Function>::iterator match;
                size_t nmatch = regex.re_nsub + 1;
                vector<regmatch_t> pmatch(nmatch);
                for (Group::iterator k = j->functions.begin(); k != j->functions.end(); k++)
                {
                    string name = (*k)->name();
                    result = regexec(&regex, name.c_str(),
                                     nmatch, &pmatch[0], 0);
                    if (result != 0 && result != REG_NOMATCH)
                        die_regex(result, &regex);
                    if (result == 0 && pmatch[0].rm_so == 0
                        && pmatch[0].rm_eo == (regoff_t) name.length())
                    {
                        matches = true;
                        match = *k;
                        break;
                    }
                }
                if (matches)
                {
                    string subst = group_substitute(*i, *j, match->name(), nmatch, &pmatch[0]);
                    if (i->param == -1)
                        j->retn.overrides[i->mode] = subst;
                    else
                    {
                        if (i->param < -1 || i->param >= (int) j->parameters.size())
                            die("Parameter out of range");
                        j->parameters[i->param].overrides[i->mode] = subst;
                    }
                    // FIXME: substitutions
                    any_matches = true;
                }
            }
            if (!any_matches)
                cerr << "Warning: no functions matched the regex '" << i->functions << "'\n";
            regfree(&regex);
        }
        else
        {
            tree_node_p node = get_type_node_test(i->type);
            if (!node)
            {
                cerr << "Warning: no such type '" << i->type << "'\n";
                continue;
            }
            if (!type_map.count(node))
            {
                cerr << "Type '" << i->type << "' is unused\n";
                continue;
            }
            list<Type>::iterator j = get_type_map(node);
            j->overrides[i->mode] = i->code;
            // FIXME: substitutions
        }
    }
}

static void make_indices()
{
    int index = 0;
    for (list<Function>::iterator i = functions.begin(); i != functions.end(); i++)
        i->index = index++;

    index = 0;
    for (list<Group>::iterator i = groups.begin(); i != groups.end(); i++)
        i->index = index++;

    index = 0;
    for (list<Type>::iterator i = types.begin(); i != types.end(); i++)
        i->index = index++;
}

static void make_bitfields()
{
    for (list<Bitfield>::iterator i = bitfields.begin(); i != bitfields.end(); i++)
    {
        tree_node_p old_node = get_type_node(i->old_type);
        tree_node_p type_node = new tree_node; // underlying integer type
        tree_node_p new_node = new tree_node;  // the new type
        tree_node_p id_node = new tree_node;   // the name for the type

        *type_node = *TYPE_NAME(old_node);
        *new_node = *old_node;
        TREE_TYPE(type_node) = new_node;
        DECL_NAME(type_node) = NULL_TREE;
        TYPE_NAME(new_node) = type_node;

        type_node->number = -1;   // not yet in tree
        new_node->number = -1;
        type_node->user_flags |= FLAG_ARTIFICIAL;
        new_node->user_flags |= FLAG_ARTIFICIAL;

        TREE_CODE(id_node) = IDENTIFIER_NODE;
        IDENTIFIER_POINTER(id_node) = i->new_type;
        id_node->user_flags |= FLAG_ARTIFICIAL;
        DECL_NAME(TYPE_NAME(new_node)) = id_node;
        insert_tree(T, new_node);
        set_flags(new_node, FLAG_USE);

        i->new_node = new_node;
    }
}

/* Prevents the user-included headers from defining the functions that we
 * wish to override, as that interferes with the ELF visibility attributes.
 */
static void write_redirects(FILE *f, bool set)
{
    /* We need the user-specified headers in here to get the types, but
     * we don't want the function defined since we redefine them with
     * protected visibility. Redirect them to harmless names.
     */
    fprintf(f, "#ifdef BUDGIE_REDIRECT_FUNCTIONS\n");
    for (list<Function>::iterator i = functions.begin(); i != functions.end(); i++)
        if (set)
            fprintf(f, "# define %s _budgie_redirect_%s\n",
                    i->name().c_str(), i->name().c_str());
        else
            fprintf(f, "# undef %s\n", i->name().c_str());
    fprintf(f, "#endif /* BUDGIE_REDIRECT_FUNCTIONS */\n");
}

static void write_headers()
{
    lib_c = fopen((libbase + ".c").c_str(), "w");
    if (!lib_c) pdie("Failed to open " + libbase + ".c");
    lib_h = fopen((libbase + ".h").c_str(), "w");
    if (!lib_h) pdie("Failed to open " + libbase + ".h");
    util_c = fopen((utilbase + ".c").c_str(), "w");
    if (!util_c) pdie("Failed to open " + utilbase + ".c");
    util_h = fopen((utilbase + ".h").c_str(), "w");
    if (!util_h) pdie("Failed to open " + utilbase + ".h");
    types_c = fopen((typesbase + ".c").c_str(), "w");
    if (!types_c) pdie("Failed to open " + typesbase + ".c");
    types_h = fopen((typesbase + ".h").c_str(), "w");
    if (!types_h) pdie("Failed to open " + typesbase + ".h");
    name_c = fopen((namebase + ".c").c_str(), "w");
    if (!name_c) pdie("Failed to open " + namebase + ".c");
    name_h = fopen((namebase + ".h").c_str(), "w");
    if (!name_h) pdie("Failed to open " + namebase + ".h");

    fprintf(util_c,
            "#include \"%s.h\"\n"
            "#include \"%s.h\"\n"
            "#include <assert.h>\n"
            "#include <dlfcn.h>\n"
            "#include <stddef.h>\n"   // for offsetof
            "#include <budgie/budgieutils.h>\n"
            "\n",
            utilbase.c_str(),
            typesbase.c_str());
    fprintf(util_h,
            "#ifndef UTILS_H\n"
            "#define UTILS_H\n"
            "\n"
            "#include <stdio.h>\n"
            "#include <stdbool.h>\n"
            "#include <stdlib.h>\n"
            "#include <string.h>\n"
            "\n");
    fprintf(types_c,
            "#include \"%s.h\"\n"
            "#include <stddef.h>\n"
            "#include <inttypes.h>\n"
            "\n",
            typesbase.c_str());
    fprintf(types_h,
            "#ifndef TYPES_H\n"
            "#define TYPES_H\n"
            "\n"
            "#include <stdio.h>\n"
            "#include <stdbool.h>\n"
            "#include <stdlib.h>\n"
            "#include <string.h>\n"
            "\n");
    fprintf(lib_c,
            "#include \"%s.h\"\n"
            "#include \"%s.h\"\n"
            "#include <budgie/budgieutils.h>\n"
            "\n",
            libbase.c_str(), utilbase.c_str());
    fprintf(lib_h,
            "#ifndef LIB_H\n"
            "#define LIB_H\n"
            "\n"
            "#define BUDGIE_REDIRECT_FUNCTIONS\n"
            "#include \"%s.h\"\n"
            "\n",
            utilbase.c_str());
    fprintf(name_c,
            "#include \"%s.h\"\n"
            "\n",
            namebase.c_str());
    fprintf(name_h,
            "#ifndef NAMES_H\n"
            "#define NAMES_H\n"
            "\n");
}

static void write_trailers()
{
    fprintf(name_h, "#endif /* !NAMES_H */\n");
    fprintf(util_h, "#endif /* !UTILS_H */\n");
    fprintf(types_h, "#endif /* !TYPES_H */\n");
    fprintf(lib_h, "#endif /* !LIB_H */\n");

    fclose(name_c);
    fclose(name_h);
    fclose(lib_c);
    fclose(lib_h);
    fclose(util_c);
    fclose(util_h);
    fclose(types_c);
    fclose(types_h);
}

static void write_enums()
{
    for (list<Function>::iterator i = functions.begin(); i != functions.end(); i++)
        fprintf(util_h, "#define %s %d\n", i->define().c_str(), i->index);
    fprintf(util_h,
            "#define NUMBER_OF_FUNCTIONS %d\n"
            "extern int budgie_number_of_functions;\n"
            "\n", (int) functions.size());
    fprintf(util_c,
            "int budgie_number_of_functions = NUMBER_OF_FUNCTIONS;\n");

    for (list<Function>::iterator i = functions.begin(); i != functions.end(); i++)
        fprintf(util_h, "#define %s %d\n", i->group_define().c_str(), i->group->index);
    fprintf(util_h,
            "#define NUMBER_OF_GROUPS %d\n"
            "extern int budgie_number_of_groups;\n"
            "\n", (int) groups.size());
    fprintf(util_c,
            "int budgie_number_of_groups = NUMBER_OF_GROUPS;\n");

    for (list<Type>::iterator i = types.begin(); i != types.end(); i++)
    {
        string name = i->type_name();
        string define = i->define();
        fprintf(types_h,
                "/* %s */\n"
                "#define %s %d\n",
                name.c_str(), define.c_str(), i->index);
    }
    fprintf(types_h,
            "#define NUMBER_OF_TYPES %d\n"
            "extern int budgie_number_of_types;\n"
            "\n", (int) types.size());
    fprintf(types_c,
            "int budgie_number_of_types = NUMBER_OF_TYPES;\n"
            "\n");
}

static void write_function_to_group_table()
{
    fprintf(util_h,
            "extern const int budgie_function_to_group[NUMBER_OF_FUNCTIONS];\n");
    fprintf(util_c,
            "const int budgie_function_to_group[NUMBER_OF_FUNCTIONS] =\n"
            "{\n");
    for (list<Function>::iterator i = functions.begin(); i != functions.end(); i++)
    {
        string group = i->group_define();
        if (i != functions.begin()) fprintf(util_c, ",\n");
        fprintf(util_c, "    %s", group.c_str());
    }
    fprintf(util_c, "\n};\n\n");
}

static void write_includes()
{
    write_redirects(util_h, true);
    write_redirects(types_h, true);
    for (list<string>::iterator i = headers.begin(); i != headers.end(); i++)
    {
        fprintf(util_h, "#include \"%s\"\n", i->c_str());
        fprintf(types_h, "#include \"%s\"\n", i->c_str());
    }
    write_redirects(util_h, false);
    write_redirects(types_h, false);
}

static void write_typedefs()
{
    for (list<Bitfield>::iterator i = bitfields.begin(); i != bitfields.end(); i++)
        fprintf(types_h, "typedef %s %s;\n",
                i->old_type.c_str(), i->new_type.c_str());
    fprintf(types_h, "\n");
}

static void write_function_table()
{
    fprintf(util_h, "extern function_data budgie_function_table[NUMBER_OF_FUNCTIONS];\n");
    fprintf(util_c,
            "function_data budgie_function_table[NUMBER_OF_FUNCTIONS] =\n"
            "{\n");
    for (list<Function>::iterator i = functions.begin(); i != functions.end(); i++)
    {
        string name = i->name();
        string group = i->group_define();
        if (i != functions.begin()) fprintf(util_c, ",\n");
        fprintf(util_c,
                "    { \"%s\", NULL, %s }",
                name.c_str(), group.c_str());
    }
    fprintf(util_c, "\n};\n\n");
}

static void write_parameter_entry(FILE *out, const Group &g,
                                  const Parameter &p,
                                  int index)
{
    string type = p.type->define();
    string dumper = "NULL", get_type = "NULL", get_length = "NULL";
    string ind = parameter_number(index);

    if (p.overrides.count(OVERRIDE_DUMP))
        dumper = "budgie_dump_" + ind + "_" + g.define();
    if (p.overrides.count(OVERRIDE_TYPE))
        get_type = "budgie_get_type_" + ind + "_" + g.define();
    if (p.overrides.count(OVERRIDE_LENGTH))
        get_length = "budgie_get_length_" + ind + "_" + g.define();
    fprintf(out, "{ %s, %s, %s, %s }",
            type.c_str(), dumper.c_str(), get_type.c_str(), get_length.c_str());
}

static void write_group_table()
{
    for (list<Group>::iterator i = groups.begin(); i != groups.end(); i++)
    {
        string name = i->name();
        if (i->parameters.empty()) continue;
        fprintf(util_c,
                "static const group_parameter_data parameters_%s[] =\n"
                "{\n",
                name.c_str());
        for (size_t j = 0; j < i->parameters.size(); j++)
        {
            if (j) fprintf(util_c, ",\n");
            fprintf(util_c, "    ");
            write_parameter_entry(util_c, *i, i->parameters[j], j);
        }
        fprintf(util_c, "\n};\n\n");
    }

    fprintf(util_h, "extern const group_data budgie_group_table[NUMBER_OF_GROUPS];\n");
    fprintf(util_c,
            "const group_data budgie_group_table[NUMBER_OF_GROUPS] =\n"
            "{\n");
    for (list<Group>::iterator i = groups.begin(); i != groups.end(); i++)
    {
        string name = i->name();
        if (i != groups.begin()) fprintf(util_c, ",\n");
        fprintf(util_c, "    { %d, ", (int) i->parameters.size());
        if (!i->parameters.empty())
            fprintf(util_c, "parameters_%s, ", name.c_str());
        else
            fprintf(util_c, "NULL, ");
        if (i->has_retn)
            write_parameter_entry(util_c, *i, i->retn, -1);
        else
            fprintf(util_c, "{ NULL_TYPE, NULL, NULL, NULL }");
        fprintf(util_c, i->has_retn ? ", true }" : ", false }");
    }
    fprintf(util_c, "\n};\n\n");
}

static void write_type_table()
{
    map<tree_node_p, tree_node_p> inverse_pointer;

    // build inverse of the pointer mapping
    for (list<Type>::iterator i = types.begin(); i != types.end(); i++)
        if (TREE_CODE(i->node) == POINTER_TYPE)
            inverse_pointer[TREE_TYPE(i->node)] = i->node;

    // generate arrays for record types
    for (list<Type>::iterator i = types.begin(); i != types.end(); i++)
    {
        if ((TREE_CODE(i->node) == UNION_TYPE
             || TREE_CODE(i->node) == RECORD_TYPE)
            && TYPE_FIELDS(i->node) != NULL_TREE)
        {
            string name = i->name();
            string type = i->type_name();
            fprintf(types_c,
                    "static const type_record_data fields_%s[] =\n"
                    "{\n",
                    name.c_str());

            tree_node_p cur = TYPE_FIELDS(i->node);
            while (cur != NULL)
            {
                if (TREE_CODE(cur) == FIELD_DECL)
                {
                    string field_type = get_type_map(TREE_TYPE(cur))->define();
                    string field = IDENTIFIER_POINTER(DECL_NAME(cur));
                    fprintf(types_c,
                            "    { %s, offsetof(%s, %s) },\n",
                            field_type.c_str(), type.c_str(), field.c_str());
                }
                cur = TREE_CHAIN(cur);
            }
            fprintf(types_c,
                    "    { NULL_TYPE, -1 }\n"
                    "};\n\n");
        }
    }

    // main type table
    fprintf(types_h, "extern const type_data budgie_type_table[NUMBER_OF_TYPES];\n");
    fprintf(types_c,
            "const type_data budgie_type_table[NUMBER_OF_TYPES] =\n"
            "{\n");
    for (list<Type>::iterator i = types.begin(); i != types.end(); i++)
    {
        string type = i->type_name();
        string name = i->name();
        string define = i->define();
        string base_type = "NULL_TYPE";
        const char *code;
        string ptr = "NULL_TYPE";
        string fields = "NULL";
        ptrdiff_t length = -1;
        string get_type = "NULL", get_length = "NULL";

        switch (TREE_CODE(i->node))
        {
        case COMPLEX_TYPE: code = "CODE_COMPLEX"; break;
        case ENUMERAL_TYPE: code = "CODE_ENUMERAL"; break;
        case INTEGER_TYPE: code = "CODE_INTEGRAL"; break;
        case REAL_TYPE: code = "CODE_FLOAT"; break;
        case RECORD_TYPE:
        case UNION_TYPE: code = "CODE_RECORD"; break;
        case ARRAY_TYPE: code = "CODE_ARRAY"; break;
        case POINTER_TYPE: code = "CODE_POINTER"; break;
        default: code = "CODE_OTHER";
        }
        if (TREE_TYPE(i->node) != NULL_TREE
            && type_map.count(TREE_TYPE(i->node)))
            base_type = get_type_map(TREE_TYPE(i->node))->define();
        if (inverse_pointer.count(i->node))
            ptr = get_type_map(inverse_pointer[i->node])->define();
        if ((TREE_CODE(i->node) == UNION_TYPE
             || TREE_CODE(i->node) == RECORD_TYPE)
            && TYPE_FIELDS(i->node) != NULL_TREE)
            fields = "fields_" + name;
        if (TREE_CODE(i->node) == ARRAY_TYPE)
        {
            if (TYPE_DOMAIN(i->node) != NULL_TREE
                && TYPE_MAX_VALUE(TYPE_DOMAIN(i->node)) != NULL_TREE)
                length = TREE_INT_CST_LOW(TYPE_MAX_VALUE(TYPE_DOMAIN(i->node))) + 1;
            else
                length = -1;
        }
        else
            length = 1; // FIXME: should this perhaps be -1?
        if (i->overrides.count(OVERRIDE_TYPE))
            get_type = "budgie_get_type_" + define;
        if (i->overrides.count(OVERRIDE_LENGTH))
            get_length = "budgie_get_length_" + define;

        if (i != types.begin()) fprintf(types_c, ",\n");
        fprintf(types_c,
                "    { %s, %s, %s, %s, sizeof(%s), %d,\n"
                "      (type_dumper) budgie_dump_%s,\n"
                "      (type_get_type) %s,\n"
                "      (type_get_length) %s }",
                code, base_type.c_str(), ptr.c_str(), fields.c_str(),
                type.c_str(), (int) length, define.c_str(),
                get_type.c_str(), get_length.c_str());
    }
    fprintf(types_c, "\n};\n\n");
}

static void write_library_table()
{
    fprintf(util_c,
            "const char * const library_names[] =\n"
            "{\n");
    for (list<string>::iterator i = libraries.begin(); i != libraries.end(); i++)
    {
        if (i != libraries.begin()) fprintf(util_c, ",\n");
        fprintf(util_c, "\"%s\"", i->c_str());
    }
    fprintf(util_c,
            "};\n"
            "int budgie_number_of_libraries = %d;\n"
            "\n",
            (int) libraries.size());
}

static void write_type_dumpers()
{
    map<tree_node_p, list<Bitfield>::iterator> bitfield_map;

    // Build map from types to bitfields.
    for (list<Bitfield>::iterator i = bitfields.begin(); i != bitfields.end(); i++)
        bitfield_map[i->new_node] = i;

    for (list<Type>::iterator i = types.begin(); i != types.end(); i++)
    {
        tree_node_p ptr = make_pointer(make_const(i->node));
        string define = i->define();
        string arg = type_to_string(ptr, "value", false);
        string custom_code;
        destroy_temporary(ptr);

        fprintf(types_h,
                "void budgie_dump_%s(%s, int count, FILE *out);\n",
                define.c_str(), arg.c_str());
        fprintf(types_c,
                "void budgie_dump_%s(%s, int count, FILE *out)\n"
                "{\n",
                define.c_str(), arg.c_str());

        // build handler for override, if any
        if (i->overrides.count(OVERRIDE_DUMP))
            custom_code = "    if (" + i->overrides[OVERRIDE_DUMP] + ") return;\n";

        // handle bitfields
        if (bitfield_map.count(i->node))
        {
            list<Bitfield>::iterator b = bitfield_map[i->node];
            fprintf(types_c,
                    "    static const bitfield_pair tokens[] =\n"
                    "    {\n");
            for (list<string>::iterator j = b->bits.begin(); j != b->bits.end(); j++)
            {
                if (j != b->bits.begin()) fprintf(types_c, ",\n");
                fprintf(types_c,
                        "        { %s, \"%s\" }",
                        j->c_str(), j->c_str());
            }
            fprintf(types_c,
                    "\n"
                    "    };\n"
                    "%s"
                    "    budgie_dump_bitfield(*value, out, tokens, %d);\n"
                    "}\n"
                    "\n",
                    custom_code.c_str(), (int) b->bits.size());
            continue;
        }

        string name;
        // FIXME: should store both low and high
        set<long> seen_enums; // to get around duplicate enums
        long value;
        tree_node_p tmp, child;

        switch (TREE_CODE(i->node))
        {
        case ENUMERAL_TYPE:
            fprintf(types_c,
                    "%s"
                    "    switch (*value)\n"
                    "    {",
                    custom_code.c_str());
            tmp = TYPE_VALUES(i->node);
            while (tmp != NULL_TREE)
            {
                name = IDENTIFIER_POINTER(TREE_PURPOSE(tmp));
                value = TREE_INT_CST_LOW(TREE_VALUE(tmp));
                if (!seen_enums.count(value))
                {
                    fprintf(types_c,
                            "    case %s: fputs(\"%s\", out); break;\n",
                            name.c_str(), name.c_str());
                    seen_enums.insert(value);
                }
                tmp = TREE_CHAIN(tmp);
            }
            fprintf(types_c,
                    "    default: fprintf(out, \"%%ld\", (long) *value);\n"
                    "    }\n");
            break;
        case INTEGER_TYPE:
            fprintf(types_c,
                    "%s"
                    "    fprintf(out, \"%%\" PRI%c%ld, (%sint%ld_t) *value);\n",
                    custom_code.c_str(),
                    (i->node->flag_unsigned ? 'u' : 'd'),
                    i->node->size->low,
                    (i->node->flag_unsigned ? "u" : ""),
                    i->node->size->low);
            break;
        case REAL_TYPE:
            // FIXME: long double
#if HAVE_LONG_DOUBLE
            fprintf(types_c,
                    "%s"
                    "    fprintf(out, \"%%Lg\", (long double) *value);\n",
                    custom_code.c_str());
#else
            fprintf(types_c,
                    "%s"
                    "    fprintf(out, \"%%g\", (double) *value);\n",
                    custom_code.c_str());
#endif
            break;
        case ARRAY_TYPE:
            fprintf(types_c,
                    "    long size;\n"
                    "    long i;\n"
                    "%s",
                    custom_code.c_str());
            if (TYPE_DOMAIN(i->node) != NULL_TREE) // find size
            {
                long size = TREE_INT_CST_LOW(TYPE_MAX_VALUE(TYPE_DOMAIN(i->node))) + 1;
                fprintf(types_c, "    size = %ld;\n", size);
            }
            else
                fprintf(types_c, "    size = count;\n");
            child = TREE_TYPE(i->node); // array element type
            fprintf(types_c,
                    "    fputs(\"{ \", out);\n"
                    "    for (i = 0; i < size; i++)\n"
                    "    {\n");
            if (type_map.count(child))
            {
                define = get_type_map(child)->define();
                fprintf(types_c,
                        "        budgie_dump_any_type(%s, &(*value)[i], -1, out);\n",
                        define.c_str());
            }
            else
                fprintf(types_c,
                        "        fputs(\"<unknown>\", out);\n");
            fprintf(types_c,
                    "        if (i < size - 1) fputs(\", \", out);\n"
                    "    }\n"
                    "    if (size < 0) fputs(\"<unknown size array>\", out);\n"
                    "    fputs(\" }\", out);\n");
            break;
        case POINTER_TYPE:
            child = TREE_TYPE(i->node); // pointed to type
            if (type_map.count(child))
                fprintf(types_c, "    int i;\n");
            fprintf(types_c,
                    "%s"
                    "    fprintf(out, \"%%p\", (void *) *value);\n",
                    custom_code.c_str());
            if (type_map.count(child))
            {
                string define = get_type_map(child)->define();
                fprintf(types_c,
                        "    if (*value)\n"
                        "    {\n"
                        "        fputs(\" -> \", out);\n"
                        // -1 means a simple pointer, not pointer to array
                        "        if (count < 0)\n"
                        "            budgie_dump_any_type(%s, *value, -1, out);\n"
                        "        else\n"
                        "        {\n"
                        // pointer to array
                        "            fputs(\"{ \", out);\n"
                        "            for (i = 0; i < count; i++)\n"
                        "            {\n"
                        "                budgie_dump_any_type(%s, &(*value)[i], -1, out);\n"
                        "                if (i + 1 < count) fputs(\", \", out);\n"
                        "            }\n"
                        "            fputs(\" }\", out);\n"
                        "        }\n"
                        "    }\n",
                        define.c_str(), define.c_str());
            }
            break;
        case RECORD_TYPE:
        case UNION_TYPE:
            { // block to allow "first" to be declared in this scope
                fprintf(types_c,
                        "%s"
                        "    fputs(\"{ \", out);\n",
                        custom_code.c_str());
                tmp = TYPE_FIELDS(i->node);
                bool first = true;
                while (tmp != NULL_TREE)
                {
                    if (TREE_CODE(tmp) == FIELD_DECL)
                    {
                        name = IDENTIFIER_POINTER(DECL_NAME(tmp));
                        if (!first)
                            fprintf(types_c,
                                    "    fputs(\", \", out);\n");
                        else first = false;
                        child = TREE_TYPE(tmp);
                        define = get_type_map(child)->define();
                        fprintf(types_c,
                                "    budgie_dump_any_type(%s, &value->%s, -1, out);\n",
                                define.c_str(), name.c_str());
                    }
                    tmp = TREE_CHAIN(tmp);
                }
                fprintf(types_c,
                        "    fputs(\" }\", out);\n");
            }
            break;
        default:
            fprintf(types_c,
                    "%s"
                    "    fputs(\"<unknown>\", out);\n",
                    custom_code.c_str());
        }
        fprintf(types_c, "\n}\n\n");
    }
}

static void write_type_get_types()
{
    for (list<Type>::iterator i = types.begin(); i != types.end(); i++)
    {
        if (!i->overrides.count(OVERRIDE_TYPE)) continue;
        tree_node_p ptr = make_pointer(make_const(i->node));
        string define = i->define();
        string type = type_to_string(ptr, "", false);
        string subst = search_replace(i->overrides[OVERRIDE_TYPE],
                                      "$$", "(*(" + type + ") value)");

        fprintf(types_h,
                "budgie_type budgie_get_type_%s(const void *value);\n",
                define.c_str());
        fprintf(types_c,
                "budgie_type budgie_get_type_%s(const void *value)"
                "{\n"
                "    return (%s);\n"
                "}\n"
                "\n",
                define.c_str(), subst.c_str());
    }
}

static void write_type_get_lengths()
{
    for (list<Type>::iterator i = types.begin(); i != types.end(); i++)
    {
        if (!i->overrides.count(OVERRIDE_LENGTH)) continue;
        tree_node_p ptr = make_pointer(make_const(i->node));
        string define = i->define();
        string type = type_to_string(ptr, "", false);
        string subst = search_replace(i->overrides[OVERRIDE_LENGTH],
                                      "$$", "(*(" + type + ") value)");

        fprintf(types_h,
                "int budgie_get_length_%s(const void *value);\n",
                define.c_str());
        fprintf(types_c,
                "int budgie_get_length_%s(const void *value)"
                "{\n"
                "    return (%s);\n"
                "}\n"
                "\n",
                define.c_str(), subst.c_str());
    }
}

static void write_parameter_overrides()
{
    for (list<Group>::iterator i = groups.begin(); i != groups.end(); i++)
    {
        string define = i->define();
        for (int j = (i->has_retn) ? -1 : 0; j < (int) i->parameters.size(); j++)
        {
            string ind = parameter_number(j);
            Parameter &p = i->parameter(j);

            if (p.overrides.count(OVERRIDE_DUMP))
            {
                fprintf(util_h,
                        "bool budgie_dump_%s_%s(const generic_function_call *call, int arg, const void *value, int length, FILE *out);\n",
                        ind.c_str(), define.c_str());
                fprintf(util_c,
                        "bool budgie_dump_%s_%s(const generic_function_call *call, int arg, const void *value, int length, FILE *out)\n"
                        "{\n"
                        "    return (%s);\n"
                        "}\n"
                        "\n",
                        ind.c_str(), define.c_str(), p.overrides[OVERRIDE_DUMP].c_str());
            }

            if (p.overrides.count(OVERRIDE_TYPE))
            {
                fprintf(util_h,
                        "budgie_type budgie_get_type_%s_%s(const generic_function_call *call, int arg, const void *value);\n",
                        ind.c_str(), define.c_str());
                fprintf(util_c,
                        "budgie_type budgie_get_type_%s_%s(const generic_function_call *call, int arg, const void *value)\n"
                        "{\n"
                        "    return (%s);\n"
                        "}\n"
                        "\n",
                        ind.c_str(), define.c_str(), p.overrides[OVERRIDE_TYPE].c_str());
            }

            if (p.overrides.count(OVERRIDE_LENGTH))
            {
                fprintf(util_h,
                        "int budgie_get_length_%s_%s(const generic_function_call *call, int arg, const void *value);\n",
                        ind.c_str(), define.c_str());
                fprintf(util_c,
                        "int budgie_get_length_%s_%s(const generic_function_call *call, int arg, const void *value)\n"
                        "{\n"
                        "    return (%s);\n"
                        "}\n"
                        "\n",
                        ind.c_str(), define.c_str(), p.overrides[OVERRIDE_LENGTH].c_str());
            }
        }
    }
}

static void write_converter()
{
    tree_node_p tmp;

    fprintf(types_h,
            "void budgie_type_convert(void *out, budgie_type out_type, const void *in, budgie_type in_type, size_t count);\n");
    fprintf(types_c,
            "void budgie_type_convert(void *out, budgie_type out_type, const void *in, budgie_type in_type, size_t count)\n"
            "{\n"
            "    long double value;\n"
            "    size_t i;\n"
            "    if (in_type == out_type\n"
            "        || (budgie_type_table[in_type].code == budgie_type_table[out_type].code\n"
            "            && budgie_type_table[in_type].size == budgie_type_table[out_type].size))\n"
            "    {\n"
            "        memcpy(out, in, budgie_type_table[in_type].size * count);\n"
            "        return;\n"
            "    }\n"
            "    for (i = 0; i < count; i++)\n"
            "    {\n"
            "        switch (in_type)\n"
            "        {\n");

    for (list<Type>::iterator i = types.begin(); i != types.end(); i++)
    {
        string define = i->define();
        string cast;
        switch (TREE_CODE(i->node))
        {
        case ENUMERAL_TYPE:
        case REAL_TYPE:
        case INTEGER_TYPE:
            tmp = make_pointer(make_const(i->node));
            cast = type_to_string(tmp, "", false);
            fprintf(types_c,
                    "        case %s: value = (long double) ((%s) in)[i]; break;\n",
                    define.c_str(), cast.c_str());
            destroy_temporary(tmp);
            break;
        default: ;
        }
    }

    fprintf(types_c,
            "        default: abort();\n"
            "        }\n"
            "        switch (out_type)\n"
            "        {\n");

    for (list<Type>::iterator i = types.begin(); i != types.end(); i++)
    {
        if (CP_TYPE_CONST_P(i->node)) continue;
        string define = i->define();
        string type = i->type_name();
        string cast;
        switch (TREE_CODE(i->node))
        {
        case ENUMERAL_TYPE:
        case REAL_TYPE:
        case INTEGER_TYPE:
            tmp = make_pointer(i->node);
            cast = type_to_string(tmp, "", false);
            fprintf(types_c,
                    "        case %s: ((%s) out)[i] = (%s) value; break;\n",
                    define.c_str(), cast.c_str(), type.c_str());
            destroy_temporary(tmp);
            break;
        default: ;
        }
    }

    fprintf(types_c,
            "        default: abort();\n"
            "        }\n"
            "    }\n"
            "}\n\n");
}

static void write_call_wrappers()
{
    for (list<Function>::iterator i = functions.begin(); i != functions.end(); i++)
    {
        tree_node_p ptr = make_pointer(TREE_TYPE(i->node));

        string name = i->name();
        string type = type_to_string(ptr, "", false);
        string define = i->define();
        fprintf(util_h,
                "#define CALL_%s(", name.c_str());
        for (size_t j = 0; j < i->group->parameters.size(); j++)
        {
            if (j) fprintf(util_h, ", ");
            fprintf(util_h, "arg%d", (int) j);
        }
        fprintf(util_h,
                ") ((*(%s) budgie_function_table[%s].real)(",
                type.c_str(), define.c_str());
        for (size_t j = 0; j < i->group->parameters.size(); j++)
        {
            if (j) fprintf(util_h, ", ");
            fprintf(util_h, "arg%d", (int) j);
        }
        fprintf(util_h, "))\n");

        destroy_temporary(ptr);
    }
}

static void write_call_structs()
{
    size_t max_args = 0;

    for (list<Function>::iterator i = functions.begin(); i != functions.end(); i++)
    {
        // avoid empty structs
        if (i->group->parameters.empty() && !i->group->has_retn) continue;
        string name = i->name();
        fprintf(util_h,
                "typedef struct\n"
                "{\n");
        for (size_t j = 0; j < i->group->parameters.size(); j++)
        {
            ostringstream arg;
            tree_node_p ptr;

            ptr = make_pointer(i->group->parameters[j].type->node);
            arg << "arg" << j;
            string p = type_to_string(ptr, arg.str(), true);
            fprintf(util_h, "    %s;\n", p.c_str());

            destroy_temporary(ptr);
        }
        if (i->group->has_retn)
        {
            tree_node_p ptr = make_pointer(i->group->retn.type->node);
            string p = type_to_string(ptr, "retn", true);
            fprintf(util_h, "    %s;\n", p.c_str());
            destroy_temporary(ptr);
        }
        fprintf(util_h, "} budgie_call_struct_%s;\n\n", name.c_str());

        max_args = max(max_args, i->group->parameters.size());
    }

    fprintf(util_h,
            "typedef struct function_call_s\n"
            "{\n"
            "    generic_function_call generic;\n"
            "    const void *args[%d];\n"
            "    union\n"
            "    {\n",
            (int) max_args);
    for (list<Function>::iterator i = functions.begin(); i != functions.end(); i++)
    {
        if (i->group->parameters.empty() && !i->group->has_retn) continue;
        string name = i->name();
        fprintf(util_h, "        budgie_call_struct_%s %s;\n",
                name.c_str(), name.c_str());
    }
    fprintf(util_h,
            "    } typed;\n"
            "} function_call;\n"
            "\n");
}

static void write_invoke()
{
    // the prototype isn't part of the invoker, but fits in best here
    fprintf(util_h,
            "void budgie_interceptor(function_call *call);\n"
            "\n");

    fprintf(util_h, "void budgie_invoke(function_call *call);\n");
    fprintf(util_c,
            "void budgie_invoke(function_call *call)\n"
            "{\n"
            "    switch (call->generic.id)\n"
            "    {\n");

    for (list<Function>::iterator i = functions.begin(); i != functions.end(); i++)
    {
        string name = i->name();
        string define = i->define();
        fprintf(util_c,
                "    case %s:\n"
                "        ",
                define.c_str());
        if (i->group->has_retn)
            fprintf(util_c, "*call->typed.%s.retn = ", name.c_str());
        fprintf(util_c, "CALL_%s(", name.c_str());
        for (size_t j = 0; j < i->group->parameters.size(); j++)
        {
            if (j) fprintf(util_c, ", ");
            fprintf(util_c, "*call->typed.%s.arg%d", name.c_str(), (int) j);
        }
        fprintf(util_c,
                ");\n"
                "        break;\n");
    }
    fprintf(util_c,
            "    default:\n"
            "        abort();\n"
            "    }\n"
            "}\n"
            "\n");
}

/* What's with all the "hidden alias" stuff? Start by taking a look at
 * http://people.redhat.com/drepper/dsohowto.pdf. Briefly, it's a symbol
 * which maps to the same function, but which doesn't appear in the
 * symbol table for the DSO. It's a bit more efficient for some applications,
 * but that's not why we use it. Some extension loaders (I think just ones
 * that people cobble together, rather than the major ones like GLEW and GLEE)
 * work by having a function called, say, glMapBuffer, which uses
 * glXGetProcAddressARB("glMapBuffer") to get the pointer it wants before
 * calling it. The catch is that if our implementation of glXGetProcAddressARB
 * just returns &glMapBuffer, it's going to return the extension wrapper,
 * rather than our interception. But if we return the alias, it is always
 * going to map to the correct function.
 */
static void write_interceptors()
{
    fprintf(lib_h, "extern bool budgie_bypass[NUMBER_OF_FUNCTIONS];\n\n");
    fprintf(lib_c, "bool budgie_bypass[NUMBER_OF_FUNCTIONS];\n\n");
    for (list<Function>::iterator i = functions.begin(); i != functions.end(); i++)
    {
        string name = i->name();
        string define = i->define();
        string group = i->group_define();
        string proto = function_type_to_string(TREE_TYPE(i->node), i->name(),
                                               false, "arg");
        fprintf(lib_h, "%s;BUGLE_GCC_DECLARE_HIDDEN_ALIAS(%s)\n",
                proto.c_str(), name.c_str());
        fprintf(lib_c,
                "%s\n"
                "{\n"
                "    function_call call;\n",
                proto.c_str());
        if (i->group->has_retn)
        {
            string ret_var = type_to_string(i->group->retn.type->node,
                                            "retn", false);
            fprintf(lib_c, "    %s;\n", ret_var.c_str());
        }
        /* The fputs is commented out because it is not always possible to
         * avoid the re-entrance. In bugle, this occurs because gl2ps is
         * used unmodified and hence calls to OpenGL functions. Note that
         * this code can only be entered after calling initialise_real, so
         * the CALL_ will work.
         */
        fprintf(lib_c,
                "    if (budgie_bypass[FUNC_%s] || !check_set_reentrance())\n"
                "    {\n"
//                "        fputs(\"Warning: %s was re-entered\\n\", stderr);\n"
                "        ", name.c_str()); //, name.c_str());
        if (i->group->has_retn)
            fprintf(lib_c, "return ");
        fprintf(lib_c, "CALL_%s(", name.c_str());
        for (size_t j = 0; j < i->group->parameters.size(); j++)
        {
            if (j) fprintf(lib_c, ", ");
            fprintf(lib_c, "arg%d", (int) j);
        }
        fprintf(lib_c, ");\n");
        if (!i->group->has_retn) fprintf(lib_c, "        return;\n");

        fprintf(lib_c,
                "    }\n"
                "    call.generic.id = %s;\n"
                "    call.generic.group = %s;\n"
                "    call.generic.args = call.args;\n"
                "    call.generic.retn = %s;\n"
                "    call.generic.num_args = %d;\n",
                define.c_str(), group.c_str(),
                i->group->has_retn ? "&retn" : "NULL",
                (int) i->group->parameters.size());

        for (size_t j = 0; j < i->group->parameters.size(); j++)
        {
            fprintf(lib_c,
                    "    call.args[%d] = &arg%d;\n"
                    "    call.typed.%s.arg%d = &arg%d;\n",
                    (int) j, (int) j, name.c_str(), (int) j, (int) j);
        }
        if (i->group->has_retn)
            fprintf(lib_c,
                    "    call.typed.%s.retn = &retn;\n",
                    name.c_str());

        fprintf(lib_c,
                "    budgie_interceptor(&call);\n"
                "    clear_reentrance();\n"
                "%s"
                "}\n"
                "BUGLE_GCC_DEFINE_HIDDEN_ALIAS(%s)\n\n",
                i->group->has_retn ? "    return retn;\n" : "", name.c_str());
    }
}

static void write_function_name_table()
{
    fprintf(lib_h, "extern function_name_data budgie_function_name_table[NUMBER_OF_FUNCTIONS];\n");
    fprintf(lib_c,
            "function_name_data budgie_function_name_table[NUMBER_OF_FUNCTIONS] =\n"
            "{\n");
    vector<string> funcs;
    for (list<Function>::iterator i = functions.begin(); i != functions.end(); i++)
        funcs.push_back(i->name());
    sort(funcs.begin(), funcs.end());
    for (vector<string>::iterator i = funcs.begin(); i != funcs.end(); i++)
    {
        if (i != funcs.begin()) fprintf(lib_c, ",\n");
        string name = *i;
        fprintf(lib_c,
                "    { \"%s\", (void (*)(void)) BUGLE_GCC_HIDDEN_ALIAS(%s) }",
                name.c_str(), name.c_str());
    }
    fprintf(lib_c, "\n};\n\n");
}

static void write_name_table()
{
    fprintf(name_h,
            "#define NUMBER_OF_FUNCTIONS %d\n"
            "extern const char * const budgie_function_names[NUMBER_OF_FUNCTIONS];\n"
            "\n",
            (int) functions.size());
    fprintf(name_c,
            "const char * const budgie_function_names[NUMBER_OF_FUNCTIONS] =\n"
            "{\n");
    for (list<Function>::iterator i = functions.begin(); i != functions.end(); i++)
    {
        string name = i->name();
        if (i != functions.begin()) fprintf(name_c, ",\n");
        fprintf(name_c, "    \"%s\"", name.c_str());
    }
    fprintf(name_c, "\n};\n\n");
}

int main(int argc, char * const argv[])
{
    process_args(argc, argv);
    T.load(tufile);
    for (int i = optind; i < argc; i++)
        load_config(argv[i]);

    make_bitfields();
    identify();
    make_function_map();
    make_groups();
    make_overrides();
    make_indices();

    write_headers();
    write_enums();
    write_function_to_group_table();
    write_includes();
    write_typedefs();
    write_function_table();
    write_group_table();
    write_type_table();
    write_library_table();
    write_type_dumpers();
    write_type_get_types();
    write_type_get_lengths();
    write_parameter_overrides();
    write_converter();
    write_call_wrappers();
    write_call_structs();
    write_invoke();
    write_interceptors();
    write_function_name_table();
    write_name_table();
    write_trailers();
}

/* Parser callbacks */

void parser_limit(const string &l)
{
    limit = "^(" + l + ")$";
}

void parser_header(const string &h)
{
    headers.push_back(h);
}

void parser_library(const string &l)
{
    libraries.push_back(l);
}

void parser_alias(const string &a, const string &b)
{
    aliases.push_back(make_pair(a, b));
}

/* We don't wrap the regex in ^,$. We would like to, but unfortunately these
 * have lower precedence than |, so we would need to group the interior.
 * But that in turn would mess up the counting of substrings, because all
 * groups become backreferences. Instead, we check that the match is against
 * the entire string. This should be safe because regexes are greedy.
 */
void parser_param(override_type mode, const string &regex,
                  int param, const string &code)
{
    Override o;

    o.mode = mode;
    o.functions = regex;
    o.param = param;
    o.type = "";
    o.code = code;
    overrides.push_back(o);
}

void parser_type(override_type mode, const string &type, const string &code)
{
    Override o;

    o.mode = mode;
    o.functions = "";
    o.param = -2;
    o.type = type;
    o.code = code;

    switch (mode)
    {
    case OVERRIDE_TYPE:
    case OVERRIDE_LENGTH:
        /* These are substituted on use, since they need type information */
        break;
    case OVERRIDE_DUMP:
        o.code = search_replace(o.code, "$$", "(*value)");
        o.code = search_replace(o.code, "$F", "(out)");
        o.code = search_replace(o.code, "$l", "(count)");
        break;
    }

    overrides.push_back(o);
}

void parser_extra_type(const string &type)
{
    extra_types.push_back(type);
}

void parser_bitfield(const string &new_type, const string &old_type,
                     const list<string> &bits)
{
    Bitfield b;

    b.new_type = new_type;
    b.old_type = old_type;
    b.bits = bits;
    bitfields.push_back(b);
}

/* Other functions declared at the top */

string Function::name() const
{
    return IDENTIFIER_POINTER(DECL_NAME(node));
}

string Function::define() const
{
    return "FUNC_" + name();
}

string Function::group_define() const
{
    return "GROUP_" + name();
}

string Type::type_name() const
{
    return type_to_string(node, "", false);
}

string Type::name() const
{
    return type_to_id(node);
}

string Type::define() const
{
    return "TYPE_" + name();
}

Group::FunctionIterator Group::canonical() const
{
    return functions.front();
}

string Group::name() const
{
    return canonical()->name();
}

string Group::define() const
{
    return "GROUP_" + name();
}

Parameter &Group::parameter(int p)
{
    if (p == -1)
    {
        assert(has_retn);
        return retn;
    }
    else
    {
        assert(p >= -1 && p < (int) parameters.size());
        return parameters[p];
    }
}

const Parameter &Group::parameter(int p) const
{
    if (p == -1)
    {
        assert(has_retn);
        return retn;
    }
    else
    {
        assert(p >= -1 && p < (int) parameters.size());
        return parameters[p];
    }
}
