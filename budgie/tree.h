/* $Id: tree.h,v 1.6 2004/01/11 15:00:22 bruce Exp $ */
/*! \file tree.h
 * Low-level management of C++ parse trees
 */

#ifndef BUDGIE_TREE_H
#define BUDGIE_TREE_H

#include <unistd.h>
#include <string>
#include <vector>

/* tree.def comes from the gcc 3.3.2 source */

#define DEFTREECODE(SYM, STRING, TYPE, NARGS)   SYM,
enum tree_code {
#include "tree.def"
  LAST_AND_UNUSED_TREE_CODE     /* A convenient way to get a value for
                                   NUM_TREE_CODE.  */
};
#undef DEFTREECODE

struct tree_node;
typedef tree_node *tree_node_p;

/* If fields are added here, then changes need to be made in
 * - lexer.l
 * - tree_node() (tree.cpp)
 * - set_flags (treeutils.cpp)
 * - insert_tree
 */
struct tree_node
{
    tree_code code;
    int number;                          // @xxx code in the .tu file
    tree_node_p value, purpose, chain;   // TREE_LISTs
    tree_node_p type, unqual;

    tree_node_p type_name, decl_name;
    tree_node_p prms;                    // function parameters
    tree_node_p size, min, max;
    tree_node_p values, init;
    int prec;

    bool flag_const;
    bool flag_volatile;
    bool flag_restrict;
    bool flag_undefined;
    bool flag_extern;
    bool flag_unsigned;
    bool flag_static;
    bool flag_struct;
    bool flag_union;

    long int high, low;

    std::string str, filename;
    int line;
    ssize_t length;

    // flags not present in the .tu file, but used internally
    unsigned int user_flags;

    tree_node();
};

class tree
{
private:
    std::vector<tree_node_p> nodes;

    // The .tu output blurs DECL_NAME and TYPE_NAME; the following
    // function corrects any errors made during lexical scanning
    void fix_name_errors();
public:
    tree() {}
    ~tree();

    void load(const std::string &);
    void clear();

    tree_node_p &operator [](unsigned int);
    tree_node_p &get(unsigned int x) { return (*this)[x]; }
    bool exists(unsigned int);
    unsigned int size() { return nodes.size(); }
};

tree_code name_to_code(const std::string &);

/* Emulate some gcc internal functionality */

#define TYPE_UNQUALIFIED   0x0
#define TYPE_QUAL_CONST    0x1
#define TYPE_QUAL_VOLATILE 0x2
#define TYPE_QUAL_RESTRICT 0x4
#define TYPE_QUAL_BOUNDED  0x8
extern int cp_type_quals(tree_node_p);

/* Nonzero if this type is const-qualified.  */
#define CP_TYPE_CONST_P(NODE)                           \
  ((cp_type_quals (NODE) & TYPE_QUAL_CONST) != 0)

/* Nonzero if this type is volatile-qualified.  */
#define CP_TYPE_VOLATILE_P(NODE)                        \
  ((cp_type_quals (NODE) & TYPE_QUAL_VOLATILE) != 0)

/* Nonzero if this type is restrict-qualified.  */
#define CP_TYPE_RESTRICT_P(NODE)                        \
  ((cp_type_quals (NODE) & TYPE_QUAL_RESTRICT) != 0)

/* Nonzero if this type is const-qualified, but not
   volatile-qualified.  Other qualifiers are ignored.  This macro is
   used to test whether or not it is OK to bind an rvalue to a
   reference.  */
#define CP_TYPE_CONST_NON_VOLATILE_P(NODE)                              \
  ((cp_type_quals (NODE) & (TYPE_QUAL_CONST | TYPE_QUAL_VOLATILE))      \
   == TYPE_QUAL_CONST)

#define TREE_TYPE(NODE) ((NODE)->type)
#define TREE_CODE(NODE) ((NODE)->code)
#define TREE_CHAIN(NODE) ((NODE)->chain)
#define TREE_ARG_TYPES(NODE) ((NODE)->prms)
#define TREE_PURPOSE(NODE) ((NODE)->purpose)
#define TREE_VALUE(NODE) ((NODE)->value)

#define TYPE_MAIN_VARIANT(NODE) ((NODE)->unqual)
#define TYPE_NAME(NODE) ((NODE)->type_name)
#define TYPE_DOMAIN(NODE) ((NODE)->values)
#define TYPE_FIELDS(NODE) ((NODE)->values)
#define TYPE_VALUES(NODE) ((NODE)->values)
#define TYPE_MAX_VALUE(NODE) ((NODE)->max)
#define TYPE_MIN_VALUE(NODE) ((NODE)->min)

#define TREE_INT_CST_LOW(NODE) ((NODE)->low)
#define TREE_INT_CST_HIGH(NODE) ((NODE)->high)

#define DECL_SOURCE_FILE(NODE) ((NODE)->filename)
#define DECL_NAME(NODE) ((NODE)->decl_name)
#define IDENTIFIER_POINTER(NODE) ((NODE)->str)

#define NULL_TREE ((tree_node_p) NULL)

#endif /* BUDGIE_TREE_H */
