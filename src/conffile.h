#ifndef BUGLE_SRC_CONFFILE_H
#define BUGLE_SRC_CONFFILE_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "linkedlist.h"

typedef struct
{
    char *name;
    char *value;
} config_variable;

typedef struct
{
    char *name;
    linked_list variables;
} config_filterset;

typedef struct
{
    char *name;
    linked_list filtersets;
} config_chain;

void config_destroy(void);
const config_chain *config_get_chain(const char *name);
const linked_list *config_get_root(void);

#endif /* !BUGLE_SRC_CONFFILE_H */
