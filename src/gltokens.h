#ifndef BUGLE_SRC_GLTOKENS_H
#define BUGLE_SRC_GLTOKENS_H

#include <GL/gl.h>

typedef struct
{
    const char *name;
    GLenum value;
} gl_token;

extern const gl_token gl_tokens_name[];
extern const gl_token gl_tokens_value[];
extern int gl_token_count;

#endif /* !BUGLE_SRC_GLTOKENS_H */
