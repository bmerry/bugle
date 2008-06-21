/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2008  Bruce Merry
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
 *
 * The layout of the headers and C files is a bit confusing, because each
 * file is
 * - public or private (for headers)
 * - hand-written or generated
 * - part of libbugle or of libbugleutils
 * As a result, prototypes defines in one header may be for functions in
 * an apparently unrelated C file.
 * (libbugleutils contains (almost) all the reflection information, but does
 * not need to be linked to the original library. Public headers go to
 * include/budgie/, private ones go to budgielib/. The list of files is
 *
 *                    public?     generated?      libbugle?
 * addresses.h          Y             N               Y
 * types.h              Y             N               ? (no functions)
 * reflect.h            Y             N               N
 * types2.h             Y             Y               Y (no functions)
 * calls.h              Y             Y               Y
 * internal.h           N             N               N
 * lib.h                N             N               Y
 *
 * addresses.c                        N               Y
 * internal.c                         N               N
 * reflect.c                          N               N
 * tables.c                           Y               N
 * lib.c                              Y               Y
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

typedef enum
{
    FILE_CALL_H,
    FILE_TYPES2_H,
    FILE_DEFINES_H,
    FILE_LIB_C,
    FILE_TABLES_C,
    FILE_COUNT
};

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
static string limit = "";
static string call_api = "";
static list<string> headers, libraries;
static list<Override> overrides;
static list<pair<string, string> > aliases;
static list<string> extra_types;
static list<Bitfield> bitfields;

/* Yacc stuff */
extern FILE *bc_yyin;
extern int yyparse();
vector<string> include_paths(1, ".");

/* File handles */
static string tufile;
static string filenames[FILE_COUNT];
static FILE *files[FILE_COUNT];

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

    filenames[FILE_CALL_H] = "include/budgie/call.h";
    filenames[FILE_TYPES2_H] = "include/budgie/types2.h";
    filenames[FILE_DEFINES_H] = "budgielib/defines.h";
    filenames[FILE_TABLES_C] = "budgielib/tables.c";
    filenames[FILE_LIB_C] = "budgielib/lib.c";
    while ((opt = getopt(argc, argv, "T:c:2:t:l:d:I:")) != -1)
    {
        switch (opt)
        {
        case 'T':
            tufile = optarg;
            break;
        case 'c':
            filenames[FILE_CALL_H] = optarg;
            break;
        case '2':
            filenames[FILE_TYPES2_H] = optarg;
            break;
        case 'l':
            filenames[FILE_LIB_C] = optarg;
            break;
        case 't':
            filenames[FILE_TABLES_C] = optarg;
            break;
        case 'd':
            filenames[FILE_DEFINES_H] = optarg;
            break;
        case 'I':
            include_paths.push_back(optarg);
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
        string anchored = "^" + limit + "$";
        int result = regcomp(&limit_regex, anchored.c_str(), REG_NOSUB | REG_EXTENDED);
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
    {
        ans = search_replace(ans, "$B", "(buffer)");
        ans = search_replace(ans, "$S", "(size)");
    }

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
    for (int i = 0; i < (int) FILE_COUNT; i++)
    {
        files[i] = fopen(filenames[i].c_str(), "w");
        if (!files[i]) pdie("Failed to open " + filenames[i]);
    }

    fprintf(files[FILE_CALL_H],
            "#ifndef BUDGIE_CALL_H\n"
            "#define BUDGIE_CALL_H\n"
            "#if HAVE_CONFIG_H\n"
            "# include <config.h>\n"
            "#endif\n"
            "#include <budgie/types.h>\n"
            "#include <budgie/reflect.h>\n"
            "#include <budgie/addresses.h>\n");
    fprintf(files[FILE_DEFINES_H],
            "#ifndef BUDGIE_DEFINES_H\n"
            "#define BUDGIE_DEFINES_H\n"
            "#if HAVE_CONFIG_H\n"
            "# include <config.h>\n"
            "#endif\n");
    fprintf(files[FILE_TYPES2_H],
            "#ifndef BUDGIE_TYPES2_H\n"
            "#define BUDGIE_TYPES2_H\n"
            "#if HAVE_CONFIG_H\n"
            "# include <config.h>\n"
            "#endif\n"
            "#include <budgie/types.h>\n"
            "#define BUDGIEAPI %s\n"
            "typedef void BUDGIEAPI (*BUDGIEAPIPROC)(void);\n",
            call_api.c_str());
    fprintf(files[FILE_TABLES_C],
            "#if HAVE_CONFIG_H\n"
            "# include <config.h>\n"
            "#endif\n"
            "#include <stdio.h>\n"
            "#include <stdlib.h>\n"
            "#include <string.h>\n"
            "#include <stddef.h>\n"   // for offsetof
            "#include <inttypes.h>\n" // for PRIxxx
            "#include <budgie/types.h>\n"
            "#include <budgie/reflect.h>\n"
            "#include \"%s\"\n"
            "#include \"budgielib/internal.h\"\n",
            filenames[FILE_DEFINES_H].c_str());
    fprintf(files[FILE_LIB_C],
            "#if HAVE_CONFIG_H\n"
            "# include <config.h>\n"
            "#endif\n"
            "#include <stdlib.h>\n"
            "#include <budgie/types.h>\n"
            "#include <budgie/call.h>\n"
            "#include \"budgielib/defines.h\"\n"
            "#include \"budgielib/internal.h\"\n"
            "#include \"budgielib/lib.h\"\n");
}

static void write_trailers()
{
    fprintf(files[FILE_CALL_H], "#endif /* !BUDGIE_CALL_H */\n");
    fprintf(files[FILE_DEFINES_H], "#endif /* !BUDGIE_DEFINES_H */\n");
    fprintf(files[FILE_TYPES2_H], "#endif /* !BUDGIE_TYPES2_H */\n");

    for (int i = 0; i < (int) FILE_COUNT; i++)
        fclose(files[i]);
}

static void write_defines(FILE *f)
{
    for (list<Function>::iterator i = functions.begin(); i != functions.end(); i++)
        fprintf(f, "#define %s %d\n", i->define().c_str(), i->index);
    fprintf(f,
            "#define FUNCTION_COUNT %d\n"
            "\n", (int) functions.size());

    for (list<Function>::iterator i = functions.begin(); i != functions.end(); i++)
        fprintf(f, "#define %s %d\n", i->group_define().c_str(), i->group->index);
    fprintf(f,
            "#define GROUP_COUNT %d\n"
            "\n", (int) groups.size());

    for (list<Type>::iterator i = types.begin(); i != types.end(); i++)
    {
        string name = i->type_name();
        string define = i->define();
        fprintf(f,
                "/* %s */\n"
                "#define %s %d\n",
                name.c_str(), define.c_str(), i->index);
    }
    fprintf(f,
            "#define TYPE_COUNT %d\n"
            "\n", (int) types.size());
}

static void write_includes()
{
    write_redirects(files[FILE_TYPES2_H], true);
    for (list<string>::iterator i = headers.begin(); i != headers.end(); i++)
        fprintf(files[FILE_TYPES2_H], "#include \"%s\"\n", i->c_str());
    write_redirects(files[FILE_TYPES2_H], false);
}

static void write_typedefs(FILE *f)
{
    for (list<Bitfield>::iterator i = bitfields.begin(); i != bitfields.end(); i++)
        fprintf(f, "typedef %s %s;\n",
                i->old_type.c_str(), i->new_type.c_str());
    fprintf(f, "\n");
}

static void write_function_table(FILE *f)
{
    fprintf(f,
            "const function_data _budgie_function_table[FUNCTION_COUNT] =\n"
            "{\n");
    for (list<Function>::iterator i = functions.begin(); i != functions.end(); i++)
    {
        string name = i->name();
        string group = i->group_define();
        if (i != functions.begin()) fprintf(f, ",\n");
        fprintf(f,
                "    { \"%s\", %s }",
                name.c_str(), group.c_str());
    }
    fprintf(f, "\n};\n\n");
}

static void write_group_table(FILE *f)
{
    for (list<Group>::iterator i = groups.begin(); i != groups.end(); i++)
    {
        string name = i->name();
        if (i->parameters.empty()) continue;
        fprintf(f,
                "static const budgie_type parameters_%s[] =\n"
                "{\n",
                name.c_str());
        for (size_t j = 0; j < i->parameters.size(); j++)
        {
            if (j) fprintf(f, ",\n");
            fprintf(f, "    %s", i->parameters[j].type->define().c_str());
        }
        fprintf(f, "\n};\n\n");
    }

    fprintf(f,
            "const group_data _budgie_group_table[GROUP_COUNT] =\n"
            "{\n");
    for (list<Group>::iterator i = groups.begin(); i != groups.end(); i++)
    {
        string name = i->name();
        if (i != groups.begin()) fprintf(f, ",\n");
        fprintf(f, "    { %d, ", (int) i->parameters.size());
        if (!i->parameters.empty())
            fprintf(f, "parameters_%s, ", name.c_str());
        else
            fprintf(f, "NULL, ");
        if (i->has_retn)
            fprintf(f, "%s, true }", i->retn.type->define().c_str());
        else
            fprintf(f, "NULL_TYPE, false }");
    }
    fprintf(f, "\n};\n\n");
}

static void write_dump_parameter_entry(FILE *out, const Group &g,
                                       const Parameter &p,
                                       int index)
{
    string type = p.type->define();
    string dumper = "NULL", get_type = "NULL", get_length = "NULL";
    string ind = parameter_number(index);

    if (p.overrides.count(OVERRIDE_DUMP))
        dumper = "_budgie_dump_" + ind + "_" + g.define();
    if (p.overrides.count(OVERRIDE_TYPE))
        get_type = "_budgie_get_type_" + ind + "_" + g.define();
    if (p.overrides.count(OVERRIDE_LENGTH))
        get_length = "_budgie_get_length_" + ind + "_" + g.define();
    fprintf(out, "{ %s, %s, %s, %s }",
            type.c_str(), dumper.c_str(), get_type.c_str(), get_length.c_str());
}

static void write_group_dump_table(FILE *f)
{
    for (list<Group>::iterator i = groups.begin(); i != groups.end(); i++)
    {
        string name = i->name();
        if (i->parameters.empty()) continue;
        fprintf(f,
                "static const group_dump_parameter parameters_%s[] =\n"
                "{\n",
                name.c_str());
        for (size_t j = 0; j < i->parameters.size(); j++)
        {
            if (j) fprintf(f, ",\n");
            fprintf(f, "    ");
            write_dump_parameter_entry(f, *i, i->parameters[j], j);
        }
        fprintf(f, "\n};\n\n");
    }

    fprintf(f,
            "const group_dump _budgie_group_dump_table[GROUP_COUNT] =\n"
            "{\n");
    for (list<Group>::iterator i = groups.begin(); i != groups.end(); i++)
    {
        string name = i->name();
        if (i != groups.begin()) fprintf(f, ",\n");
        fprintf(f, "    { %d, ", (int) i->parameters.size());
        if (!i->parameters.empty())
            fprintf(f, "parameters_%s, ", name.c_str());
        else
            fprintf(f, "NULL, ");
        if (i->has_retn)
            write_dump_parameter_entry(f, *i, i->retn, -1);
        else
            fprintf(f, "{ NULL_TYPE, NULL, NULL, NULL }");
        fprintf(f, i->has_retn ? ", true }" : ", false }");
    }
    fprintf(f, "\n};\n\n");
}

static void write_type_table(FILE *f)
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
            fprintf(f,
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
                    fprintf(f,
                            "    { %s, offsetof(%s, %s) },\n",
                            field_type.c_str(), type.c_str(), field.c_str());
                }
                cur = TREE_CHAIN(cur);
            }
            fprintf(f,
                    "    { NULL_TYPE, -1 }\n"
                    "};\n\n");
        }
    }

    // main type table
    fprintf(f,
            "const type_data _budgie_type_table[TYPE_COUNT] =\n"
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
            get_type = "_budgie_get_type_" + define;
        if (i->overrides.count(OVERRIDE_LENGTH))
            get_length = "_budgie_get_length_" + define;

        if (i != types.begin()) fprintf(f, ",\n");
        fprintf(f,
                "    { \"%s\", \"%s\", %s, %s, %s, %s, sizeof(%s), %d,\n"
                "      (type_dumper) _budgie_dump_%s,\n"
                "      (type_get_type) %s,\n"
                "      (type_get_length) %s }",
                type.c_str(), name.c_str(), code, base_type.c_str(), ptr.c_str(), fields.c_str(),
                type.c_str(), (int) length, define.c_str(),
                get_type.c_str(), get_length.c_str());
    }
    fprintf(f, "\n};\n\n");
}

static void write_library_table(FILE *f)
{
    fprintf(f,
            "const char * const _budgie_library_names[] =\n"
            "{\n");
    for (list<string>::iterator i = libraries.begin(); i != libraries.end(); i++)
    {
        if (i != libraries.begin()) fprintf(f, ",\n");
        fprintf(f, "\"%s\"", i->c_str());
    }
    fprintf(f,
            "\n};\n\n"
            "int _budgie_library_count = %d;\n"
            "\n",
            (int) libraries.size());
}

static void write_type_dumpers(FILE *f)
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

        fprintf(f,
                "static ssize_t _budgie_dump_%s(%s, int count, char **buffer, size_t *size)\n"
                "{\n"
                "    char *orig;\n",
                define.c_str(), arg.c_str());

        custom_code = "    orig = *buffer;\n";
        // build handler for override, if any
        if (i->overrides.count(OVERRIDE_DUMP))
            custom_code +=
                "    if (" + i->overrides[OVERRIDE_DUMP] + ") return *buffer - orig;\n";

        // handle bitfields
        if (bitfield_map.count(i->node))
        {
            list<Bitfield>::iterator b = bitfield_map[i->node];
            fprintf(f,
                    "    static const bitfield_pair tokens[] =\n"
                    "    {\n");
            for (list<string>::iterator j = b->bits.begin(); j != b->bits.end(); j++)
            {
                if (j != b->bits.begin()) fprintf(f, ",\n");
                fprintf(f,
                        "        { %s, \"%s\" }",
                        j->c_str(), j->c_str());
            }
            fprintf(f,
                    "\n"
                    "    };\n"
                    "%s"
                    "    _budgie_dump_bitfield(*value, buffer, size, tokens, %d);\n"
                    "    return *buffer - orig;\n"
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
            fprintf(f,
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
                    fprintf(f,
                            "    case %s: budgie_snputs_advance(buffer, size, \"%s\"); break;\n",
                            name.c_str(), name.c_str());
                    seen_enums.insert(value);
                }
                tmp = TREE_CHAIN(tmp);
            }
            fprintf(f,
                    "    default: budgie_snprintf_advance(buffer, size, \"%%ld\", (long) *value);\n"
                    "    }\n");
            break;
        case INTEGER_TYPE:
            fprintf(f,
                    "%s"
                    "    budgie_snprintf_advance(buffer, size, \"%%\" PRI%c%ld, (%sint%ld_t) *value);\n",
                    custom_code.c_str(),
                    (i->node->flag_unsigned ? 'u' : 'd'),
                    i->node->size->low,
                    (i->node->flag_unsigned ? "u" : ""),
                    i->node->size->low);
            break;
        case REAL_TYPE:
            // FIXME: long double
#if HAVE_LONG_DOUBLE
            fprintf(f,
                    "%s"
                    "    budgie_snprintf_advance(buffer, size, \"%%Lg\", (long double) *value);\n",
                    custom_code.c_str());
#else
            fprintf(f,
                    "%s"
                    "    budgie_snprintf_advance(buffer, size, \"%%g\", (double) *value);\n",
                    custom_code.c_str());
#endif
            break;
        case ARRAY_TYPE:
            fprintf(f,
                    "    long asize;\n"
                    "    long i;\n"
                    "%s",
                    custom_code.c_str());
            if (TYPE_DOMAIN(i->node) != NULL_TREE) // find size
            {
                long size = TREE_INT_CST_LOW(TYPE_MAX_VALUE(TYPE_DOMAIN(i->node))) + 1;
                fprintf(f, "    asize = %ld;\n", size);
            }
            else
                fprintf(f, "    asize = count;\n");
            child = TREE_TYPE(i->node); // array element type
            fprintf(f,
                    "    budgie_snputs_advance(buffer, size, \"{ \");\n"
                    "    for (i = 0; i < asize; i++)\n"
                    "    {\n");
            if (type_map.count(child))
            {
                define = get_type_map(child)->define();
                fprintf(f,
                        "        budgie_dump_any_type(%s, &(*value)[i], -1, buffer, size);\n",
                        define.c_str());
            }
            else
                fprintf(f,
                        "        budgie_snputs_advance(buffer, size, \"<unknown>\");\n");
            fprintf(f,
                    "        if (i < asize - 1)\n"
                    "            budgie_snputs_advance(buffer, size, \", \");\n"
                    "    }\n"
                    "    if (asize < 0)\n"
                    "        budgie_snputs_advance(buffer, size, \"<unknown size array>\");\n"
                    "    budgie_snputs_advance(buffer, size, \" }\");\n");
            break;
        case POINTER_TYPE:
            child = TREE_TYPE(i->node); // pointed to type
            if (type_map.count(child))
                fprintf(f, "    int i;\n");
            fprintf(f,
                    "%s"
                    "    budgie_snprintf_advance(buffer, size, \"%%p\", (void *) *value);\n",
                    custom_code.c_str());
            if (type_map.count(child))
            {
                string define = get_type_map(child)->define();
                fprintf(f,
                        "    if (*value)\n"
                        "    {\n"
                        "        budgie_snputs_advance(buffer, size, \" -> \");\n"
                        // -1 means a simple pointer, not pointer to array
                        "        if (count < 0)\n"
                        "            budgie_dump_any_type(%s, *value, -1, buffer, size);\n"
                        "        else\n"
                        "        {\n"
                        // pointer to array
                        "            budgie_snputs_advance(buffer, size, \"{ \");\n"
                        "            for (i = 0; i < count; i++)\n"
                        "            {\n"
                        "                budgie_dump_any_type(%s, &(*value)[i], -1, buffer, size);\n"
                        "                if (i + 1 < count) budgie_snputs_advance(buffer, size, \", \");\n"
                        "            }\n"
                        "            budgie_snputs_advance(buffer, size, \" }\");\n"
                        "        }\n"
                        "    }\n",
                        define.c_str(), define.c_str());
            }
            break;
        case RECORD_TYPE:
        case UNION_TYPE:
            { // block to allow "first" to be declared in this scope
                fprintf(f,
                        "%s"
                        "    budgie_snputs_advance(buffer, size, \"{ \");\n",
                        custom_code.c_str());
                tmp = TYPE_FIELDS(i->node);
                bool first = true;
                while (tmp != NULL_TREE)
                {
                    if (TREE_CODE(tmp) == FIELD_DECL)
                    {
                        name = IDENTIFIER_POINTER(DECL_NAME(tmp));
                        if (!first)
                            fprintf(f,
                                    "    budgie_snputs_advance(buffer, size, \", \");\n");
                        else first = false;
                        child = TREE_TYPE(tmp);
                        define = get_type_map(child)->define();
                        fprintf(f,
                                "    budgie_dump_any_type(%s, &value->%s, -1, buffer, size);\n",
                                define.c_str(), name.c_str());
                    }
                    tmp = TREE_CHAIN(tmp);
                }
                fprintf(f,
                        "    budgie_snputs_advance(buffer, size, \" }\");\n");
            }
            break;
        default:
            fprintf(f,
                    "%s"
                    "    budgie_snputs_advance(buffer, size, \"<unknown>\");\n",
                    custom_code.c_str());
        }
        fprintf(f, "\n    return *buffer - orig;\n}\n\n");
    }
}

static void write_type_get_types(FILE *f)
{
    for (list<Type>::iterator i = types.begin(); i != types.end(); i++)
    {
        if (!i->overrides.count(OVERRIDE_TYPE)) continue;
        tree_node_p ptr = make_pointer(make_const(i->node));
        string define = i->define();
        string type = type_to_string(ptr, "", false);
        string subst = search_replace(i->overrides[OVERRIDE_TYPE],
                                      "$$", "(*(" + type + ") value)");

        fprintf(f,
                "static budgie_type _budgie_get_type_%s(const void *value)"
                "{\n"
                "    return (%s);\n"
                "}\n"
                "\n",
                define.c_str(), subst.c_str());
    }
}

static void write_type_get_lengths(FILE *f)
{
    for (list<Type>::iterator i = types.begin(); i != types.end(); i++)
    {
        if (!i->overrides.count(OVERRIDE_LENGTH)) continue;
        tree_node_p ptr = make_pointer(make_const(i->node));
        string define = i->define();
        string type = type_to_string(ptr, "", false);
        string subst = search_replace(i->overrides[OVERRIDE_LENGTH],
                                      "$$", "(*(" + type + ") value)");

        fprintf(f,
                "static int _budgie_get_length_%s(const void *value)"
                "{\n"
                "    return (%s);\n"
                "}\n"
                "\n",
                define.c_str(), subst.c_str());
    }
}

static void write_parameter_overrides(FILE *f)
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
                fprintf(f,
                        "static bool _budgie_dump_%s_%s(const generic_function_call *call, int arg, const void *value, int length, char **buffer, size_t *size)\n"
                        "{\n"
                        "    return (%s);\n"
                        "}\n"
                        "\n",
                        ind.c_str(), define.c_str(), p.overrides[OVERRIDE_DUMP].c_str());
            }

            if (p.overrides.count(OVERRIDE_TYPE))
            {
                fprintf(f,
                        "static budgie_type _budgie_get_type_%s_%s(const generic_function_call *call, int arg, const void *value)\n"
                        "{\n"
                        "    return (%s);\n"
                        "}\n"
                        "\n",
                        ind.c_str(), define.c_str(), p.overrides[OVERRIDE_TYPE].c_str());
            }

            if (p.overrides.count(OVERRIDE_LENGTH))
            {
                fprintf(f,
                        "static int _budgie_get_length_%s_%s(const generic_function_call *call, int arg, const void *value)\n"
                        "{\n"
                        "    return (%s);\n"
                        "}\n"
                        "\n",
                        ind.c_str(), define.c_str(), p.overrides[OVERRIDE_LENGTH].c_str());
            }
        }
    }
}

static void write_converter(FILE *f)
{
    tree_node_p tmp;

    fprintf(f,
            "void budgie_type_convert(void *out, budgie_type out_type, const void *in, budgie_type in_type, size_t count)\n"
            "{\n"
            "    long double value;\n"
            "    size_t i;\n"
            "    if (in_type == out_type\n"
            "        || (_budgie_type_table[in_type].code == _budgie_type_table[out_type].code\n"
            "            && _budgie_type_table[in_type].size == _budgie_type_table[out_type].size))\n"
            "    {\n"
            "        memcpy(out, in, _budgie_type_table[in_type].size * count);\n"
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
            fprintf(f,
                    "        case %s: value = (long double) ((%s) in)[i]; break;\n",
                    define.c_str(), cast.c_str());
            destroy_temporary(tmp);
            break;
        default: ;
        }
    }

    fprintf(f,
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
            fprintf(f,
                    "        case %s: ((%s) out)[i] = (%s) value; break;\n",
                    define.c_str(), cast.c_str(), type.c_str());
            destroy_temporary(tmp);
            break;
        default: ;
        }
    }

    fprintf(f,
            "        default: abort();\n"
            "        }\n"
            "    }\n"
            "}\n\n");
}

static void write_call_wrappers(FILE *f)
{
    for (list<Function>::iterator i = functions.begin(); i != functions.end(); i++)
    {
        tree_node_p ptr = make_pointer(TREE_TYPE(i->node));

        string name = i->name();
        string type = type_to_string(ptr, "BUDGIEAPI", false);
        fprintf(f,
                "#define _BUDGIE_HAVE_CALL_%s 1\n"
                "#define _BUDGIE_CALL_%s _BUDGIE_CALL(%s, %s)\n",
                name.c_str(), name.c_str(), name.c_str(), type.c_str());
        destroy_temporary(ptr);
    }
}

static void write_call_structs(FILE *f)
{
    fprintf(f,
            "typedef union\n"
            "{\n"
            "    generic_function_call generic;\n");
    for (list<Function>::iterator i = functions.begin(); i != functions.end(); i++)
    {
        // avoid empty structs
        if (i->group->parameters.empty() && !i->group->has_retn) continue;
        string name = i->name();
        fprintf(f,
                "    struct\n"
                "    {\n"
                "        budgie_group group;\n"
                "        budgie_function id;\n"
                "        int num_args;\n"
                "        void *user_data;\n");
        if (i->group->has_retn)
        {
            tree_node_p ptr = make_pointer(i->group->retn.type->node);
            string p = type_to_string(ptr, "retn", true);
            fprintf(f, "        %s;\n", p.c_str());
            destroy_temporary(ptr);
        }
        else
            fprintf(f, "        void *padding;\n");
        for (size_t j = 0; j < i->group->parameters.size(); j++)
        {
            ostringstream arg;
            tree_node_p ptr;

            ptr = make_pointer(i->group->parameters[j].type->node);
            arg << "arg" << j;
            string p = type_to_string(ptr, arg.str(), true);
            fprintf(f, "        %s;\n", p.c_str());

            destroy_temporary(ptr);
        }
        fprintf(f, "    } %s;\n\n", name.c_str());
    }

    fprintf(f,
            "} function_call;\n"
            "\n");
}

static void write_call_to(FILE *f, list<Function>::iterator func, const char *arg_template)
{
    tree_node_p ptr = make_pointer(TREE_TYPE(func->node));
    string type = type_to_string(ptr, " BUDGIEAPI", false);
    string name = func->name();
    string define = func->define();
    destroy_temporary(ptr);

    fprintf(f, "(*(%s) budgie_function_address_real(%s))(",
            type.c_str(), define.c_str());
    for (size_t j = 0; j < func->group->parameters.size(); j++)
    {
        if (j) fprintf(f, ", ");
        fprintf(f, arg_template, (int) j);
    }
    fprintf(f, ")");
}

static void write_invoke(FILE *f)
{
    fprintf(f,
            "void budgie_invoke(function_call *call)\n"
            "{\n"
            "    switch (call->generic.id)\n"
            "    {\n");

    for (list<Function>::iterator i = functions.begin(); i != functions.end(); i++)
    {
        string name = i->name();
        string define = i->define();
        string arg_template = "*call->" + name + ".arg%d";
        fprintf(f,
                "    case %s:\n"
                "        ",
                define.c_str());
        if (i->group->has_retn)
            fprintf(f, "*call->%s.retn = ", name.c_str());
        write_call_to(f, i, arg_template.c_str());
        fprintf(f,
                ";\n"
                "        break;\n");
    }
    fprintf(f,
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
static void write_interceptors(FILE *f)
{
    fprintf(f, "bool _budgie_bypass[FUNCTION_COUNT];\n\n");
    fprintf(f, 
            "#ifdef DLL_EXPORT\n"
            "# define BUDGIE_DLL_EXPORT __declspec(dllexport)\n"
            "#else\n"
            "# define BUDGIE_DLL_EXPORT\n"
            "#endif\n");
    for (list<Function>::iterator i = functions.begin(); i != functions.end(); i++)
    {
        string name = i->name();
        string define = i->define();
        string group = i->group_define();
        string proto = function_type_to_string(TREE_TYPE(i->node), "BUDGIEAPI " + i->name(),
                                               false, "arg");
        fprintf(f,
                "BUDGIE_DLL_EXPORT %s\n"
                "{\n"
                "    function_call call;\n",
                proto.c_str());
        if (i->group->has_retn)
        {
            string ret_var = type_to_string(i->group->retn.type->node,
                                            "retn", false);
            fprintf(f, "    %s;\n", ret_var.c_str());
        }
        /* The fputs is commented out because it is not always possible to
         * avoid the re-entrance. In bugle, this occurs because gl2ps is
         * used unmodified and hence calls to OpenGL functions. Note that
         * this code can only be entered after calling initialise_real, so
         * the addresses are valid.
         */
        fprintf(f,
                "    if (_budgie_bypass[FUNC_%s] || !_budgie_reentrance_init())\n"
                "    {\n"
//                "        fputs(\"Warning: %s was re-entered\\n\", stderr);\n"
                "        ", name.c_str()); //, name.c_str());
        if (i->group->has_retn)
            fprintf(f, "return ");
        write_call_to(f, i, "arg%d");
        fprintf(f, ";\n");
        if (!i->group->has_retn) fprintf(f, "        return;\n");

        fprintf(f,
                "    }\n"
                "    call.generic.id = %s;\n"
                "    call.generic.group = %s;\n"
                "    call.generic.retn = %s;\n"
                "    call.generic.num_args = %d;\n",
                define.c_str(), group.c_str(),
                i->group->has_retn ? "&retn" : "NULL",
                (int) i->group->parameters.size());

        for (size_t j = 0; j < i->group->parameters.size(); j++)
        {
            fprintf(f,
                    "    call.generic.args[%d] = &arg%d;\n",
                    (int) j, (int) j);
        }
        if (i->group->has_retn)
            fprintf(f,
                    "    call.generic.retn = &retn;\n");

        fprintf(f,
                "    budgie_interceptor(&call);\n"
                "    _budgie_reentrance_clear();\n"
                "%s"
                "}\n"
                "BUGLE_ATTRIBUTE_DECLARE_HIDDEN_ALIAS(%s)\n"
                "BUGLE_ATTRIBUTE_DEFINE_HIDDEN_ALIAS(%s)\n\n",
                i->group->has_retn ? "    return retn;\n" : "",
                name.c_str(), name.c_str());
    }

    fprintf(f,
            "BUDGIEAPIPROC _budgie_function_address_real[FUNCTION_COUNT];\n"
            "BUDGIEAPIPROC _budgie_function_address_wrapper[FUNCTION_COUNT] =\n"
            "{\n");
    for (list<Function>::iterator i = functions.begin(); i != functions.end(); i++)
    {
        if (i != functions.begin())
            fprintf(f, ",\n");
        fprintf(f, "    (BUDGIEAPIPROC) BUGLE_ATTRIBUTE_HIDDEN_ALIAS(%s)",
                i->name().c_str());
    }
    fprintf(f, "\n};\n\n");
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
    write_defines(files[FILE_DEFINES_H]);
    write_includes();

    write_typedefs(files[FILE_TABLES_C]);
    write_type_dumpers(files[FILE_TABLES_C]);
    write_type_get_types(files[FILE_TABLES_C]);
    write_type_get_lengths(files[FILE_TABLES_C]);
    write_parameter_overrides(files[FILE_LIB_C]);

    write_function_table(files[FILE_TABLES_C]);
    write_group_table(files[FILE_TABLES_C]);
    write_group_dump_table(files[FILE_LIB_C]);
    write_type_table(files[FILE_TABLES_C]);
    write_library_table(files[FILE_LIB_C]);
    write_converter(files[FILE_TABLES_C]);

    write_call_wrappers(files[FILE_CALL_H]);
    write_call_structs(files[FILE_TYPES2_H]);
    write_invoke(files[FILE_LIB_C]);
    write_interceptors(files[FILE_LIB_C]);
    write_trailers();
}

/* Parser callbacks */

void parser_limit(const string &l)
{
    if (!limit.empty())
        limit += "|";
    limit += "(" + l + ")";
}

void parser_header(const string &h)
{
    headers.push_back(h);
}

void parser_call_api(const string &api)
{
    call_api = api;
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
        o.code = search_replace(o.code, "$B", "(buffer)");
        o.code = search_replace(o.code, "$S", "(size)");
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
