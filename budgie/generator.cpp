/* $Id: generator.cpp,v 1.6 2004/02/29 16:29:25 bruce Exp $ */
/*! \file generator.cpp
 * \brief Contains all the code that generates other code
 */

#include <map>
#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <cstddef>
#include <cassert>
#include "generator.h"
#include "tree.h"
#include "treeutils.h"

using namespace std;

struct gen_state_tree
{
    int index;
    string name, instance_class;
    vector<gen_state_tree *> children;

    tree_node_p type;
    int count;
    string loader;

    gen_state_tree() : type(NULL_TREE), count(0) {}
    ~gen_state_tree()
    {
        for (size_t i = 0; i < children.size(); i++)
            delete children[i];
    }
};

static map<tree_node_p, vector<string> > bitfield_types;
static map<param_or_type, string> type_overrides;
static map<param_or_type, string> length_overrides, dump_overrides, save_overrides, load_overrides;
static gen_state_tree root_state;

bool param_or_type::operator <(const param_or_type &b) const
{
    if ((func != NULL) ^ (b.func != NULL)) return
        func < b.func;
    if (func != b.func) return func < b.func;
    if (func) return param < b.param;
    else return type < b.type;
}

/*! The first takes just a type. The second takes a \c call object and
 * parameter number. Both return a \c budgie_type.
 * \param prototype True to only return function prototypes.
 */
string get_type(bool prototype)
{
    ostringstream out, par; // par is for parameter get_type

    out << "budgie_type get_type(budgie_type type)";
    par << "budgie_type get_arg_type(const generic_function_call *call, int param)";
    if (prototype)
    {
        out << ";\n";
        par << ";\n";
        return out.str() + par.str();
    }
    out << "\n"
        << "{\n"
        << "    while (1)"
        << "    {\n"
        << "        switch (type)\n"
        << "        {\n";
    par << "\n"
        << "{\n";
    map<param_or_type, string>::const_iterator i;
    for (i = type_overrides.begin(); i != type_overrides.end(); i++)
    {
        if (i->first.type) // type override
        {
            out << "         case TYPE_" << type_to_id(i->first.type) << ":\n";
            string l = "(" + i->second + ")";
            string::size_type pos;
            // In type overrides, $ is replaced by the value
            while ((pos = l.find("$")) != string::npos)
                l.replace(pos, 1, "type");
            out << "            type = " << l << ";\n"
                << "            break;\n";
        }
        else // parameter override
        {
            par << "    if (call->id == FUNC_"
                << IDENTIFIER_POINTER(DECL_NAME(i->first.func))
                << " && param == " << i->first.param << ")\n"
                << "        return (" << i->second << ");\n";
        }
    }
    out << "        default: return type;\n"
        << "        }\n"
        << "    }\n"
        << "}\n";

    // If no parameter override, look for a type override
    par << "    if (param == -1) return get_type(function_table[call->id].retn.type);\n"
        << "    else return get_type(function_table[call->id].parameters[param].type);\n"
        << "}\n";

    return out.str() + par.str();
}

/*! Similar to \c get_type, but generates a \c get_length function. Length
 * is an attribute of a pointer, that says how many elements are being
 * pointed at.
 * \param prototype True to only return function prototypes.
 */
string get_length(bool prototype)
{
    ostringstream out, par; // par is for parameter overrides

    out << "int get_length(budgie_type type, const void *value)";
    par << "int get_arg_length(const generic_function_call *call, int param, const void *value)";
    if (prototype)
    {
        out << ";\n";
        par << ";\n";
        return out.str() + par.str();
    }
    out << "\n"
        << "{\n"
        << "    type = get_type(type);\n"
        << "    switch (type)\n"
        << "    {\n";
    par << "\n"
        << "{\n";
    map<param_or_type, string>::const_iterator i;
    for (i = length_overrides.begin(); i != length_overrides.end(); i++)
    {
        if (i->first.type && TREE_CODE(i->first.type) == POINTER_TYPE)
        { // type override
            tree_node_p base = TREE_TYPE(i->first.type);
            tree_node_p cnst = make_pointer(make_const(make_pointer(make_const(base), false)), false);
            out << "    case TYPE_" << type_to_id(i->first.type) << ":\n";
            string l = "(" + i->second + ")";
            string repl = "(*(" + type_to_string(cnst, "", false)
                + ") value)";
            destroy_temporary(cnst);
            string::size_type pos;
            while ((pos = l.find("$")) != string::npos)
                l.replace(pos, 1, repl);
            out << "         return " << l << ";\n";
        }
        else // parameter override
        {
            par << "    if (call->id == FUNC_"
                << IDENTIFIER_POINTER(DECL_NAME(i->first.func))
                << " && param == " << i->first.param << ")\n"
                << "        return (" << i->second << ");\n";
        }
    }
    out << "    default: return -1;\n";
    out << "    }\n";
    out << "}\n";

    par << "    if (param == -1) return get_length(function_table[call->id].retn.type, value);\n"
        << "    else return get_length(function_table[call->id].parameters[param].type, value);\n"
        << "}\n";
    return out.str() + par.str();
}

/*! \param type The type to check up on
 * \returns True if the type can be saved, false otherwise.
 */
bool dumpable(tree_node_p type)
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

/*! Returns a function that logs a particular type. It takes into account
 * type overrides.
 * \param node The type to work on
 * \param prototype True to only generate a function prototype
 */
static string make_dumper(tree_node_p node, bool prototype)
{
    ostringstream out;
    string name = "dump_type_" + type_to_id(node);
    tree_node_p child;

    // We take a const pointer, to avoid copies
    tree_node_p tmp = make_pointer(make_const(node), false);
    out << "void " << name << "(";
    out << type_to_string(tmp, "value", false);
    destroy_temporary(tmp);
    out << ", int count, FILE *out)";
    if (prototype)
    {
        out << ";";
        return out.str();
    }
    out << "\n{\n";
    // If there is a type override, handle it
    if (dump_overrides.count(node))
        out << "    if (" << dump_overrides[node]
            << "((const void *) value, count, out)) return;\n";
    // Bitfields are special
    if (bitfield_types.count(node))
    {
        const vector<string> &tokens = bitfield_types[node];
        out << "    bitfield_pair tokens[] =\n"
            << "    {\n";
        for (unsigned int i = 0; i < tokens.size(); i++)
        {
            out << "        { "
                << tokens[i] << ", \"" << tokens[i] << "\" }";
            if (i + 1 < tokens.size())
                out << ",";
            out << "\n";
        }
        out << "    };\n"
            << "    dump_bitfield(*value, out, tokens, " << tokens.size() << ");\n";
    }
    else
    {
        string name;
        switch (TREE_CODE(node))
        {
        case ENUMERAL_TYPE:
            out << "    switch (*value)\n"
                << "    {\n";
            tmp = TYPE_VALUES(node);
            while (tmp != NULL_TREE)
            {
                name = IDENTIFIER_POINTER(TREE_PURPOSE(tmp));
                out << "    case " << name << ": fputs(\"" << name << "\", out); break;\n";
                tmp = TREE_CHAIN(tmp);
            }
            out << "    }\n";
            break;
        case INTEGER_TYPE:
            // FIXME: long long types; unsigned types
            out << "    fprintf(out, \"%ld\", (long) *value);\n";
            break;
        case REAL_TYPE:
            // FIXME: long double
            out << "    fprintf(out, \"%f\", (double) *value);\n";
            break;
        case ARRAY_TYPE:
            if (TYPE_DOMAIN(node) != NULL_TREE) // find size
            {
                int size = TREE_INT_CST_LOW(TYPE_MAX_VALUE(TYPE_DOMAIN(node))) + 1;
                out << "    int size = " << size << ";\n";
            }
            else
                out << "    int size = count;\n";
            child = TREE_TYPE(node); // array element type
            if (dumpable(child))
                out << "    int i;\n";
            out << "    fputs(\"{ \", out);\n";
            // allow for type overrides of the elements
            out << "    budgie_type type = get_type(TYPE_" << type_to_id(child) << ");\n";
            out << "    for (i = 0; i < size; i++)\n"
                << "    {\n";
            if (dumpable(child))
                out << "        dump_any_type(type, &(*value)[i], "
                    << "get_length(type, &(*value)[i]), out);\n";
            else
                out << "        fputs(\"<unknown>\", out);\n";
            out << "        if (i < size - 1) fputs(\", \", out);\n"
                << "    }\n"
                << "    if (size < 0) fputs(\"<unknown size array>\", out);\n"
                << "    fputs(\" }\", out);\n";
            break;
        case POINTER_TYPE:
            child = TREE_TYPE(node); // pointed to type
            if (dumpable(child))
                out << "    int i;\n";
            out << "    fprintf(out, \"%p\", *value);\n";
            if (dumpable(child))
            {
                out << "    if (*value)\n"
                    << "    {\n"
                    << "        fputs(\" -> \", out);\n"
                    // handle type overrides
                    << "        budgie_type type = get_type(TYPE_" << type_to_id(child) << ");\n"
                    // -1 means a simple pointer, not pointer to array
                    << "        if (count < 0)\n"
                    << "            dump_any_type(type, value, "
                    << "get_length(type, value), out);\n"
                    << "        else\n"
                    << "        {\n"
                    // pointer to array
                    << "            fputs(\"{ \", out);\n"
                    << "            for (i = 0; i < count; i++)\n"
                    << "            {\n"
                    << "                dump_any_type(type, &(*value)[i], "
                    << "get_length(type, &(*value)[i]), out);\n"
                    << "                if (i + 1 < count) fputs(\", \", out);\n"
                    << "            }\n"
                    << "            fputs(\" }\", out);\n"
                    << "        }\n"
                    << "    }\n";
            }
            break;
        case RECORD_TYPE:
        case UNION_TYPE:
            { // block to allow "first" to be declared in this scope
                out << "    fputs(\"{ \", out);\n"
                    << "    budgie_type type;\n";
                tmp = TYPE_FIELDS(node);
                bool first = true;
                while (tmp != NULL_TREE)
                {
                    if (TREE_CODE(tmp) == FIELD_DECL)
                    {
                        name = IDENTIFIER_POINTER(DECL_NAME(tmp));
                        if (!first) out << "    fputs(\", \", out);\n";
                        else first = false;
                        child = TREE_TYPE(tmp);
                        out << "    type = get_type(TYPE_" << type_to_id(child) << ");\n"
                            << "    dump_any_type(type, &value->" << name
                            << ", get_length(type, &value->" << name << ")"
                            << ", out);\n";
                    }
                    tmp = TREE_CHAIN(tmp);
                }
                out << "    fputs(\" }\", out);\n";
            }
            break;
        default:
            out << "    fputs(\"<unknown>\", out);\n";
        }
    }
    out << "}\n";
    return out.str();
}

// Type handling

//! The output of \c type_to_id on types that have \c budgie_type defines.
/*! The types are #defined, not enumerated, but the term "enum" is still
 * used for historical reasons.
 */
static map<string, tree_node_p> enum_types;
typedef map<string, tree_node_p>::const_iterator enum_types_it;

string type_to_enum(tree_node_p type)
{
    if (type == NULL_TREE) return "NULL_TYPE";
    string ans = type_to_id(type);
    if (enum_types.count(ans)) return "TYPE_" + ans;
    else return "NULL_TYPE";
}

void specify_types(const vector<tree_node_p> &types)
{
    enum_types.clear();
    for (size_t i = 0; i < types.size(); i++)
        enum_types[type_to_id(types[i])] = types[i];
}

string type_table(bool header)
{
    ostringstream out;
    int counter;

    if (header)
    {
        // define #defines for types
        counter = 0;
        for (enum_types_it i = enum_types.begin(); i != enum_types.end(); i++)
            out << "/* " << type_to_string(i->second, "", false) << " */\n"
                << "#define TYPE_" << i->first << " " << counter++ << "\n";
        out << "#define NUMBER_OF_TYPES " << counter << "\n"
            << "\n"
            << "extern const type_data type_table[NUMBER_OF_TYPES];\n"
            << "\n";
    }
    else
    {
        for (enum_types_it i = enum_types.begin(); i != enum_types.end(); i++)
        {
            if ((TREE_CODE(i->second) == UNION_TYPE
                 || TREE_CODE(i->second) == RECORD_TYPE)
                && TYPE_FIELDS(i->second) != NULL_TREE)
            {
                string name = type_to_id(i->second);

                out << "static const type_record_data fields_" << name << "[] =\n"
                    << "{\n";

                tree_node_p cur = TYPE_FIELDS(i->second);
                while (cur != NULL)
                {
                    if (TREE_CODE(cur) == FIELD_DECL)
                    {
                        out << "    { " << type_to_enum(TREE_TYPE(cur)) << ", "
                            << "offsetof(" << type_to_string(i->second, "", false)
                            << ", " << IDENTIFIER_POINTER(DECL_NAME(cur)) << ") },\n";
                    }
                    cur = TREE_CHAIN(cur);
                }
                out << "    { NULL_TYPE, -1 }\n"
                    << "};\n\n";
            }
        }

        out << "const type_data type_table[NUMBER_OF_TYPES] =\n"
            << "{\n";

        for (enum_types_it i = enum_types.begin(); i != enum_types.end(); i++)
        {
            string name = type_to_id(i->second);

            if (i != enum_types.begin()) out << ",\n";
            out << "    { ";
            switch (TREE_CODE(i->second))
            {
            case COMPLEX_TYPE:
                out << "CODE_COMPLEX, ";
                break;
            case ENUMERAL_TYPE:
                out << "CODE_ENUMERAL, ";
                break;
            case INTEGER_TYPE:
                out << "CODE_INTEGRAL, ";
                break;
            case REAL_TYPE:
                out << "CODE_FLOAT, ";
                break;
            case RECORD_TYPE:
            case UNION_TYPE:
                out << "CODE_RECORD, ";
                break;
            case ARRAY_TYPE:
                out << "CODE_ARRAY, ";
                break;
            case POINTER_TYPE:
                out << "CODE_POINTER, ";
                break;
            default:
                out << "CODE_OTHER, ";
            }
            out << type_to_enum(TREE_TYPE(i->second)) << ", ";
            if ((TREE_CODE(i->second) == UNION_TYPE
                 || TREE_CODE(i->second) == RECORD_TYPE)
                && TYPE_FIELDS(i->second) != NULL_TREE)
                out << "fields_" << name << ", ";
            else
                out << "NULL, ";
            out << "sizeof(" << type_to_string(i->second, "", false) << "), ";
            if (TREE_CODE(i->second) == ARRAY_TYPE)
            {
                if (TYPE_DOMAIN(i->second) != NULL_TREE
                    && TYPE_MAX_VALUE(TYPE_DOMAIN(i->second)) != NULL_TREE)
                    out << TREE_INT_CST_LOW(TYPE_MAX_VALUE(TYPE_DOMAIN(i->second))) + 1;
                else
                    out << "-1";
            }
            else
                out << "1";
            out << " }";
        }
        out << "\n};\n\n";
    }
    return out.str();
}

/*! \param prototype True to only return a function prototype
 * \todo Consider using a lookup table
 */
string make_type_dumper(bool prototype)
{
    ostringstream out;

    // type specific dumpers
    for (enum_types_it i = enum_types.begin(); i != enum_types.end(); i++)
        out << make_dumper(i->second, prototype) << "\n";

    out << "void dump_any_type(budgie_type type, const void *value, int length, FILE *out)";
    if (prototype)
    {
        out << ";\n";
        return out.str();
    }
    out << "\n{\n"
        << "    switch (type)\n"
        << "    {\n";

    for (enum_types_it i = enum_types.begin(); i != enum_types.end(); i++)
    {
        string name = type_to_enum(i->second);

        tree_node_p tmp = make_pointer(make_const(i->second), false);
        out << "    case " << name << ":\n"
            << "        dump_type_" << type_to_id(i->second) << "("
            << "(" << type_to_string(tmp, "", false) << ") value"
            << ", length, out);\n"
            << "        break;\n";
        destroy_temporary(tmp);
    }
    out << "    default: abort();\n"
        << "    }\n"
        << "}\n";
    return out.str();
}

// type conversion
void type_converter(bool prototype, ostream &out)
{
    out << "void type_convert(void *out, budgie_type out_type, const void *in, budgie_type in_type, size_t count)";
    if (prototype)
    {
        out << ";\n";
        return;
    }
    out << "{\n"
        << "    long double value;\n"
        << "    size_t i;\n"
        << "    if (in_type == out_type\n"
        << "        || (type_table[in_type].code == type_table[out_type].code\n"
        << "            && type_table[in_type].size == type_table[out_type].size))\n"
        << "    {\n"
        << "        memcpy(out, in, type_table[in_type].size * count);\n"
        << "        return;\n"
        << "    }\n"
        << "    for (i = 0; i < count; i++)\n"
        << "    {\n"
        << "        switch (in_type)\n"
        << "        {\n";

    tree_node_p tmp;
    for (enum_types_it i = enum_types.begin(); i != enum_types.end(); i++)
        switch (TREE_CODE(i->second))
        {
        case ENUMERAL_TYPE:
        case REAL_TYPE:
        case INTEGER_TYPE:
            tmp = make_const(make_pointer(i->second, false));
            out << "        case " << type_to_enum(i->second) << ": "
                << "value = (long double) ((" << type_to_string(tmp, "", false) << ") in)[i]; break;\n";
            destroy_temporary(tmp);
        default: ;
        }
    out << "        default: abort();\n"
        << "        }\n"
        << "        switch (out_type)\n"
        << "        {\n";
    for (enum_types_it i = enum_types.begin(); i != enum_types.end(); i++)
        if (!CP_TYPE_CONST_P(i->second))
            switch (TREE_CODE(i->second))
            {
            case ENUMERAL_TYPE:
            case REAL_TYPE:
            case INTEGER_TYPE:
                tmp = make_pointer(i->second, false);
                out << "        case " << type_to_enum(i->second) << ": "
                    << "((" << type_to_string(tmp, "", false) << ") out)[i] = "
                    << "(" << type_to_string(i->second, "", false) << ") value; "
                    << "break;\n";
                destroy_temporary(tmp);
            default: ;
            }
    out << "        default: abort();\n"
        << "        }\n"
        << "    }\n"
        << "}\n\n";
}

// Function handling

static vector<tree_node_p> functions;

void specify_functions(const vector<tree_node_p> &funcs)
{
    functions = funcs;
}

/* It would be nicer to have this return a string, but under libstdc++3,
 * ostringstreams take O(n^2) time to generate O(n) data.
 */
void function_table(bool header, ostream &out)
{
    int counter = 0;
    if (header)
    {
        for (size_t i = 0; i < functions.size(); i++)
            out << "#define FUNC_" << IDENTIFIER_POINTER(DECL_NAME(functions[i]))
                << " " << counter++ << "\n";
        out << "#define NUMBER_OF_FUNCTIONS " << counter << "\n"
            << "\n"
            << "extern const function_data function_table[NUMBER_OF_FUNCTIONS];\n";
    }
    else
    {
        for (size_t i = 0; i < functions.size(); i++)
        {
            string name = IDENTIFIER_POINTER(DECL_NAME(functions[i]));
            tree_node_p type = TREE_TYPE(functions[i]);
            tree_node_p cur = TREE_ARG_TYPES(type);
            int count = 0;
            while (cur != NULL_TREE && TREE_CODE(cur->value) != VOID_TYPE)
            {
                param_or_type test(functions[i], count);
                string dumper = "NULL";
                if (dump_overrides.count(test))
                    dumper = dump_overrides[test];
                if (count == 0)
                {
                    out << "static function_parameter_data parameters_"
                        << name << "[] =\n"
                        << "{\n";
                }
                else
                    out << ",\n";
                out << "    { "
                    << type_to_enum(TREE_VALUE(cur)) << ", "
                    << dumper << " }";
                count++;
                cur = TREE_CHAIN(cur);
            }
            if (count) out << "\n};\n\n";

        }

        out << "const function_data function_table[NUMBER_OF_FUNCTIONS] =\n"
            << "{\n";
        for (size_t i = 0; i < functions.size(); i++)
        {
            if (i) out << ",\n";
            string name = IDENTIFIER_POINTER(DECL_NAME(functions[i]));
            out << "    { \"" << name << "\", ";
            tree_node_p type = TREE_TYPE(functions[i]);
            tree_node_p cur = TREE_ARG_TYPES(type);
            int count = 0;
            while (cur != NULL_TREE && TREE_CODE(cur->value) != VOID_TYPE)
            {
                count++;
                cur = TREE_CHAIN(cur);
            }
            out << count << ", ";
            if (count) out << "parameters_" << name << ", ";
            else out << "NULL, ";
            if (TREE_CODE(TREE_TYPE(type)) != VOID_TYPE)
            {
                param_or_type test(functions[i], -1);
                string dumper = "NULL";
                if (dump_overrides.count(test))
                    dumper = dump_overrides[test];
                out << "{ " << type_to_enum(TREE_TYPE(type)) << ", " << dumper
                    << " }, true }";
            }
            else
                out << "{ NULL_TYPE, NULL }, false }";
        }
        out << "\n};\n\n";
    }
}

//! Creates a wrapper for a function.
/*! Creates a wrapper for function \arg node, returning it as a \c string.
 * The wrapper does not necessarily invoke the original; rather, it
 * calls the user-supplied interceptor, which may optionally
 * invoke the original. For functions returning values, the value should
 * be written into the pointer given by \c call.generic.ret
 * taken from the \c ret field of the structure passed to interceptor.
 */
string make_wrapper(tree_node_p node, bool prototype)
{
    ostringstream out;
    string name = IDENTIFIER_POINTER(DECL_NAME(node));
    tree_node_p type = TREE_TYPE(node);

    out << function_type_to_string(type, name, false, "arg");
    if (prototype)
    {
        out << ";\n";
        return out.str();
    }
    out << "\n"
        << "{\n"
        << "    function_call call;\n";

    // return value
    tree_node_p ret_type = TREE_TYPE(TREE_TYPE(node));
    if (TREE_CODE(ret_type) != VOID_TYPE)
        out << "    " << type_to_string(ret_type, "ret", false) << ";\n";

    // check for re-entrancy
    out << "    if (reentrant)\n"
        << "    {\n"
        << "        init_real();\n"
        << "        ";
    if (TREE_CODE(ret_type) != VOID_TYPE)
        out << "return ";
    out << "(*" << name << "_real)(";
    tree_node_p cur = type->prms;
    int count = 0;
    while (cur != NULL_TREE && TREE_CODE(cur->value) != VOID_TYPE)
    {
        if (count) out << ", ";
        out << "arg" << count;
        count++;
        cur = TREE_CHAIN(cur);
    }
    out << ");\n";
    if (TREE_CODE(ret_type) == VOID_TYPE)
        out << "        return;\n";
    out << "    }\n"
        << "    reentrant = true;\n";

    /* At this stage we capture only the value of the parameters, with
     * no deep copies. This is so that if the interceptor immediately
     * calls invoke(), then any pointers still point to originals. Capture
     * of the full data (for writing to file) is done by calling
     * complete_call() on the resulting function_call object.
     */
    cur = type->prms;
    count = 0;
    out << "    call.generic.id = FUNC_" << name << ";\n";
    out << "    call.generic.args = call.args;\n";
    if (TREE_CODE(ret_type) != VOID_TYPE)
    {
        out << "    call.generic.ret = &ret;\n";
        out << "    call.typed." << name << ".ret = &ret;\n";
    }
    else
        out << "    call.generic.ret = NULL;\n";

    while (cur != NULL_TREE && TREE_CODE(cur->value) != VOID_TYPE)
    {
        out << "    call.args[" << count << "] = &arg" << count << ";\n"
            << "    call.typed." << name << ".arg" << count << " = &arg" << count << ";\n";
        count++;
        cur = cur->chain;
    }
    out << "    call.generic.num_args = " << count << ";\n";

    // do the actual work
    out << "    interceptor(&call);\n";

    // clear the lock
    out << "    reentrant = false;\n";

    if (TREE_CODE(ret_type) != VOID_TYPE)
        out << "    return ret;\n";
    out << "}\n";
    return out.str();
}

void generics(ostream &out)
{
    out << "#define GENERIC_FUNCTION_ID(f) FUNC_ ## f\n";
    out << "#define GENERIC_FUNCTION_STRUCT(f) FS_ ## f\n";
    out << "#define GENERIC() \\\n";
    for (size_t i = 0; i < functions.size(); i++)
    {
        tree_node_p type = TREE_TYPE(functions[i]);
        tree_node_p cur = TREE_ARG_TYPES(type);
        string name = IDENTIFIER_POINTER(DECL_NAME(functions[i]));
        if (i)
            out << "    GENERIC_CALL_SEPARATOR(" << name
                << ", " << (TREE_CODE(TREE_TYPE(type)) == VOID_TYPE ? 0 : 1)
                << ") \\\n";
        out << "    GENERIC_CALL_BEGIN(" << name
            << ", " << (TREE_CODE(TREE_TYPE(type)) == VOID_TYPE ? 0 : 1)
            << ") \\\n";
        int count = 0;
        while (cur != NULL_TREE && TREE_CODE(cur->value) != VOID_TYPE)
        {
            if (count)
                out << "    GENERIC_CALL_ARGUMENT_SEPARATOR(" << name
                    << ", " << (TREE_CODE(TREE_TYPE(type)) == VOID_TYPE ? 0 : 1)
                    << ", " << count
                    << ", " << type_to_string(TREE_VALUE(cur), "", true)
                    << ", " << type_to_id(TREE_VALUE(cur))
                    << ") \\\n";
            out << "    GENERIC_CALL_ARGUMENT(" << name
                << ", " << (TREE_CODE(TREE_TYPE(type)) == VOID_TYPE ? 0 : 1)
                << ", " << count
                << ", " << type_to_string(TREE_VALUE(cur), "", true)
                << ", " << type_to_id(TREE_VALUE(cur))
                << ") \\\n";
            count++;
            cur = TREE_CHAIN(cur);
        }
        out << "    GENERIC_CALL_END(" << name
            << ", " << (TREE_CODE(TREE_TYPE(type)) == VOID_TYPE ? 0 : 1)
            << ") \\\n";
    }
    out << "\n"; // since the last line has a backslash escape
}

void function_pointers(bool prototype, ostream &out)
{
    for (size_t i = 0; i < functions.size(); i++)
    {
        tree_node_p ptr = make_pointer(TREE_TYPE(functions[i]), false);
        string name = IDENTIFIER_POINTER(DECL_NAME(functions[i])) + "_real";
        if (prototype)
            out << "extern " << type_to_string(ptr, name, false) << ";\n";
        else
            out << type_to_string(ptr, name, false) << " = NULL;\n";
        destroy_temporary(ptr);
    }
}

void function_types(ostream &out)
{
    int max_args = 0;

    for (size_t i = 0; i < functions.size(); i++)
    {
        tree_node_p type = TREE_TYPE(functions[i]);
        tree_node_p cur = TREE_ARG_TYPES(type);
        tree_node_p tmp;
        string name = IDENTIFIER_POINTER(DECL_NAME(functions[i]));

        out << "typedef struct " << name << "\n"
            << "{\n";
        int count = 0;
        while (cur != NULL_TREE && TREE_CODE(cur->value) != VOID_TYPE)
        {
            ostringstream argname;
            argname << "arg" << count;
            tmp = make_pointer(make_const(TREE_VALUE(cur)), false);
            out << "    " << type_to_string(tmp, argname.str(), false) << ";\n";
            count++;
            cur = TREE_CHAIN(cur);
            destroy_temporary(tmp);
        }
        if (TREE_CODE(TREE_TYPE(type)) != VOID_TYPE)
        {
            tmp = make_pointer(TREE_TYPE(type), false);
            out << "    " << type_to_string(tmp, "ret", false) << ";\n";
        }
        out << "} function_call_" << name << ";\n\n";
        if (count > max_args) max_args = count;
    }

    out << "typedef struct\n"
        << "{\n"
        << "    generic_function_call generic;\n"
        << "    const void *args[" << max_args << "];\n"
        << "    union\n"
        << "    {\n";
    for (size_t i = 0; i < functions.size(); i++)
    {
        string name = IDENTIFIER_POINTER(DECL_NAME(functions[i]));
        out << "        function_call_" << name << " " << name << ";\n";
    }
    out << "    } typed;\n"
        << "} function_call;\n\n";
}

void make_invoker(bool prototype, ostream &out)
{
    out << "void invoke(function_call *call)";
    if (prototype)
    {
        out << ";\n";
        return;
    }
    out << "\n"
        << "{\n"
        << "    init_real();\n"
        << "    switch (call->generic.id)\n"
        << "    {\n";
    for (size_t i = 0; i < functions.size(); i++)
    {
        tree_node_p type = TREE_TYPE(functions[i]);
        string name = IDENTIFIER_POINTER(DECL_NAME(functions[i]));
        string fname = "FUNC_" + name;
        out << "    case " << fname << ":\n";
        out << "        ";
        if (TREE_CODE(TREE_TYPE(type)) != VOID_TYPE) // save return val
            out << "*call->typed." << name << ".ret = ";
        out << "(*" << name << "_real)(";
        tree_node_p cur = TREE_ARG_TYPES(type);
        int count = 0;
        while (cur != NULL_TREE && TREE_CODE(cur->value) != VOID_TYPE)
        {
            if (count) out << ", ";
            out << "*call->typed." << name << ".arg" << count;
            count++;
            cur = TREE_CHAIN(cur);
        }
        out << ");\n"
            << "        break;\n";
    }
    out << "    default: abort();\n"
        << "    }\n"
        << "}\n";
}

static void make_state_instance_struct(gen_state_tree *node,
                                       const string &prefix,
                                       int &index,
                                       bool prototype, ostream &out)
{
    ostringstream number;
    string name = prefix;

    if (node->name == "[]") name = prefix + "_I";
    else if (node->name != "")
    {
        number << node->name.length();
        name = prefix + "_" + number.str() + node->name;
    }
    string prefix_r = name;
    if (name == "state") name = "state_root";
    node->instance_class = name;
    for (size_t i = 0; i < node->children.size(); i++)
        make_state_instance_struct(node->children[i], prefix_r, index,
                                   prototype, out);
    node->index = index++;
    if (prototype)
    {
        out << "typedef struct " << name << "_s\n"
            << "{\n"
            << "    state_generic generic;\n";
        int num_children = 0;
        for (size_t i = 0; i < node->children.size(); i++)
        {
            if (node->children[i]->name != "[]")
            {
                out << "    " << node->children[i]->instance_class
                    << " c_" << node->children[i]->name << ";\n";
                num_children++;
            }
        }
        out << "    state_generic *children["
            << num_children + 1 << "];\n";
        if (node->type != NULL_TREE)
        {
            if (node->count == 1)
                out << "    " << type_to_string(node->type, "data", false) << ";\n";
            else
            {
                ostringstream num;
                num << node->count;
                out << "    " << type_to_string(node->type, "data[" + num.str() + "]", false) << ";\n";
            }
        }
        out << "} " << name << ";\n\n";
    }
    else
    {
        out << "void " << name << "_constructor(state_generic *s, state_generic *parent)\n"
            << "{\n"
            << "    " << name << " *me = (" << name << " *) s;\n"
            << "    initialise_state(s, parent);\n"
            << "    s->children = me->children;\n"
            << "    s->spec = &state_spec_table[" << node->index << "];\n";
        if (node->type != NULL_TREE)
            out << "    s->data = &me->data;\n";
        else
            out << "    s->data = NULL;\n";
        int counter = 0;
        for (size_t i = 0; i < node->children.size(); i++)
        {
            if (node->children[i]->name != "[]")
                out << "    me->children[" << counter++ << "] = &me->c_"
                    << node->children[i]->name << ".generic;\n"
                    << "    " << node->children[i]->instance_class
                    << "_constructor((state_generic *) &me->c_" << node->children[i]->name << ", s);\n";
        }
        out << "};\n\n";
    }
}

static void make_state_spec_data(gen_state_tree *node,
                                 ostream &out)
{
    for (size_t i = 0; i < node->children.size(); i++)
        make_state_spec_data(node->children[i], out);

    out << "const state_spec *" << node->instance_class << "_spec_children[] = { ";
    for (size_t i = 0; i < node->children.size(); i++)
        if (node->children[i]->name != "[]")
            out << "&state_spec_table[" << node->children[i]->index << "], ";
    out << "NULL };\n";
}

static void make_state_spec_table_entry(gen_state_tree *node,
                                        ostream &out)
{
    gen_state_tree *indexed = NULL;
    for (size_t i = 0; i < node->children.size(); i++)
    {
        make_state_spec_table_entry(node->children[i], out);
        if (node->children[i]->name == "[]")
            indexed = node->children[i];
    }

    if (node->index) out << ",\n";
    out << "    { \"" << node->name << "\", "
        << node->instance_class << "_spec_children, ";
    if (indexed)
        out << "&state_spec_table[" << indexed->index << "], ";
    else
        out << "NULL, ";
    out << "NULL, "; // FIXME: key_compare
    if (node->type != NULL_TREE)
        out << type_to_enum(node->type) << ", "
            << node->count << ", ";
    else
        out << "NULL_TYPE, 0, ";
    out << "sizeof(" << node->instance_class << "), ";
    if (node->loader != "") out << node->loader;
    else out << "NULL";
    out << ", "
        << node->instance_class << "_constructor }";
}

void make_state_structs(bool prototype, ostream &out)
{
    int count = 0;

    if (!prototype)
        out << "static const state_spec state_spec_table[];\n";
    make_state_instance_struct(&root_state, "state", count, prototype, out);
    if (!prototype)
    {
        make_state_spec_data(&root_state, out);
        out << "static const state_spec state_spec_table[] =\n"
            << "{\n";
        make_state_spec_table_entry(&root_state, out);
        out << "\n};\n"
            << "const state_spec *root_state_spec = &state_spec_table["
            << root_state.index << "];\n\n";
    }
}

// Overrides

void set_dump_override(const param_or_type &p, const string &subst)
{
    dump_overrides[p] = subst;
}

void set_load_override(const param_or_type &p, const string &subst)
{
    load_overrides[p] = subst;
}

void set_save_override(const param_or_type &p, const string &subst)
{
    save_overrides[p] = subst;
}

/*! Replaces $\it n in the string with the value of the nth parameter,
 * assuming that the string will be used in a function that has \c call as
 * a parameter. Also, $f is replaced with the \c budgie_function name of
 * the function, and $t\it n with the \c budgie_type type of the n'th
 * parameter. FIXME: implement this
 * \param func The function that is involved
 * \param s The user-supplied string
 * \returns The modified string.
 */
static string override_substitutions(tree_node_p func, const string &s)
{
    string::size_type pos;
    string l = s;
    while ((pos = l.find("$")) != string::npos)
    {
        // FIXME: parameters beyond 10
        int param = l[pos + 1] - '0';

        tree_node_p cur = TREE_ARG_TYPES(TREE_TYPE(func));
        // FIXME: falling off the end
        for (int i = 0; i < param; i++)
            cur = TREE_CHAIN(cur);
        tree_node_p ptr = make_pointer(make_const(TREE_VALUE(cur)), false);
        ostringstream repl;
        repl << "(*("
            << type_to_string(ptr, "", false) << ") call->args[" << param << "])";
        l.replace(pos, 2, repl.str());
        destroy_temporary(ptr);
    }
    return l;
}

void set_type_override(const param_or_type &p, const string &new_type)
{
    if (p.func)
        type_overrides[p] = override_substitutions(p.func, new_type);
    else
        type_overrides[p] = new_type;
}

void set_length_override(const param_or_type &p, const string &len)
{
    if (p.func)
        length_overrides[p] = override_substitutions(p.func, len);
    else
        length_overrides[p] = len;
}

void set_state_spec(const string &name, tree_node_p type,
                    int count, const string loader)
{
    gen_state_tree *tree = &root_state;
    string name2 = name;
    string child;
    string::size_type pos;
    size_t i;

    while (!name2.empty())
    {
        if (name2[0] == '.')
        {
            name2.erase(0, 1);
            continue;
        }
        else if (name2[0] == '[')
        {
            // FIXME: give a decent error
            assert(name2[1] == ']');
            pos = 2;
        }
        else
            pos = name2.find_first_of("[].");
        child = name2.substr(0, pos);
        name2.erase(0, pos);
        for (i = 0; i < tree->children.size(); i++)
            if (tree->children[i]->name == child) break;
        if (i == tree->children.size())
        {
            tree->children.push_back(new gen_state_tree());
            tree = tree->children.back();
            tree->name = child;
        }
        else
            tree = tree->children[i];
    }
    tree->count = count;
    tree->type = type;
    tree->loader = loader;
}

tree_node_p new_bitfield_type(tree_node_p base,
                              const vector<string> &tokens)
{
    tree_node_p type = new tree_node;
    tree_node_p ans = new tree_node;

    bitfield_types[ans] = tokens;

    // copy the TYPE_DECL node as well, since it will have its name
    // field redirected
    *type = *TYPE_NAME(base);
    *ans = *base;
    TREE_TYPE(type) = ans;
    DECL_NAME(type) = NULL_TREE;
    TYPE_NAME(ans) = type;

    ans->number = type->number = -1;
    ans->user_flags |= FLAG_ARTIFICIAL;
    type->user_flags |= FLAG_ARTIFICIAL;
    return ans;
}
