#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <GL/gl.h>
#include <string.h>
#include <assert.h>
#include "gltokens.h"
#include "gldump.h"
#include "budgieutils.h"

const char *gl_enum_to_token(GLenum e)
{
    int l, r, m;

    l = 0;
    r = gl_token_count;
    while (l + 1 < r)
    {
        m = (l + r) / 2;
        if (e < gl_tokens_value[m].value) r = m;
        else l = m;
    }
    if (gl_tokens_value[l].value != e)
        return NULL;
    else
        return gl_tokens_value[l].name;
}

GLenum gl_token_to_enum(const char *name)
{
    int l, r, m;

    l = 0;
    r = gl_token_count;
    while (l + 1 < r)
    {
        m = (l + r) / 2;
        if (strcmp(name, gl_tokens_name[m].name) < 0) r = m;
        else l = m;
    }
    assert(strcmp(gl_tokens_name[l].name, name) == 0);
    return gl_tokens_name[l].value;
}

bool dump_GLenum(const void *value, int count, FILE *out)
{
    GLenum e = *(const GLenum *) value;
    const char *name = gl_enum_to_token(e);
    if (!name)
        fprintf(out, "<unknown token 0x%.4x>", (unsigned int) e);
    else
        fputs(name, out);
    return true;
}

bool dump_GLerror(const void *value, int count, FILE *out)
{
    GLenum err = *(const GLenum *) value;

    switch (err)
    {
    case GL_NO_ERROR: fputs("GL_NO_ERROR", out); break;
    default: dump_GLenum(value, count, out);
    }
    return true;
}

// FIXME: redo definition of alternate enums, based on sort order
bool dump_GLalternateenum(const void *value, int count, FILE *out)
{
    GLenum token = *(const GLenum *) value;

    switch (token)
    {
    case GL_ZERO: fputs("GL_ZERO", out); break;
    case GL_ONE: fputs("GL_ONE", out); break;
    default: dump_GLenum(value, count, out);
    }
    return true;
}

bool dump_GLboolean(const void *value, int count, FILE *out)
{
    fputs((*(const GLboolean *) value) ? "GL_TRUE" : "GL_FALSE", out);
    return true;
}

int count_gl(budgie_function func, GLenum token)
{
    switch (token)
    {
        /* lights and materials */
    case GL_AMBIENT:
    case GL_DIFFUSE:
    case GL_SPECULAR:
    case GL_EMISSION:
    case GL_AMBIENT_AND_DIFFUSE:
    case GL_POSITION:
        /* glLightModel */
    case GL_LIGHT_MODEL_AMBIENT:
        /* glTexEnv */
    case GL_TEXTURE_ENV_COLOR:
        /* glFog */
    case GL_FOG_COLOR:
        /* glTexParmeter */
    case GL_TEXTURE_BORDER_COLOR:
        /* glTexGen */
    case GL_OBJECT_PLANE:
    case GL_EYE_PLANE:
        return 4;
        /* Other */
    case GL_COLOR_INDEXES:
    case GL_SPOT_DIRECTION:
        return 3;
    default:
        return -1;
    }
}
