#ifndef BUGLE_SRC_GLDUMP_H
#define BUGLE_SRC_GLDUMP_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <GL/gl.h>
#include "budgieutils.h"

int count_gl(budgie_function func, GLenum token);
GLenum gl_token_to_enum(const char *name);
const char *gl_enum_to_token(GLenum e);

bool dump_GLenum(const void *value, int count, FILE *out);
bool dump_GLalternateenum(const void *value, int count, FILE *out);
bool dump_GLerror(const void *value, int count, FILE *out);
bool dump_GLboolean(const void *value, int count, FILE *out);
bool dump_convert(const generic_function_call *gcall,
                  int arg,
                  const void *value,
                  FILE *out);

#endif /* !BUGLE_SRC_GLDUMP_H */
