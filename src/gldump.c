#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <GL/gl.h>
#include <string.h>
#include <assert.h>
#include "gltokens.h"
#include "gldump.h"
#include "canon.h"
#include "safemem.h"
#include "budgielib/budgieutils.h"
#include "src/types.h"
#include "src/utils.h"

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
    if (strcmp(gl_tokens_name[l].name, name) != 0)
        return (GLenum) -1;
    // Pick the first one, to avoid using extension suffices
    while (l > 0 && strcmp(gl_tokens_name[l - 1].name, name) == 0)
        l--;
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

/* FIXME: redo definition of alternate enums, based on sort order */
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

static budgie_type get_real_type(const function_call *call)
{
    switch (canonical_call(call))
    {
    case FUNC_glTexParameteri:
    case FUNC_glTexParameteriv:
    case FUNC_glTexParameterf:
    case FUNC_glTexParameterfv:
    case FUNC_glGetTexParameteriv:
    case FUNC_glGetTexParameterfv:
        switch (*(const GLenum *) call->args[1])
        {
        case GL_TEXTURE_WRAP_S:
        case GL_TEXTURE_WRAP_T:
        case GL_TEXTURE_WRAP_R:
        case GL_TEXTURE_MIN_FILTER:
        case GL_TEXTURE_MAG_FILTER:
        case GL_DEPTH_TEXTURE_MODE:
        case GL_TEXTURE_COMPARE_MODE:
        case GL_TEXTURE_COMPARE_FUNC:
            return TYPE_6GLenum;
        case GL_GENERATE_MIPMAP:
            return TYPE_9GLboolean;
        default:
            return NULL_TYPE;
        }
        break;
    default:
        return NULL_TYPE;
    }
}

bool dump_convert(const generic_function_call *gcall,
                  int arg,
                  const void *value,
                  FILE *out)
{
    const function_call *call;
    budgie_type in_type, out_type;
    const void *in;
    int length = -1;
    void *out_data;

    call = (const function_call *) gcall;
    out_type = get_real_type(call);
    if (out_type == NULL_TYPE) return false;

    in_type = function_table[call->generic.id].parameters[2].type;
    if (type_table[in_type].code == CODE_POINTER)
    {
        in = *(const void * const *) value;
        in_type = type_table[in_type].type;
    }
    else
        in = value;
    if (function_table[call->generic.id].parameters[2].get_length)
        length = (*function_table[call->generic.id].parameters[2].get_length)(gcall, arg, value);
    if (length < 0) length = 1;

    out_data = xmalloc(type_table[out_type].size * length);
    type_convert(out_data, out_type, in, in_type, length);
    dump_any_type(out_type, out_data, length, out);
    free(out_data);
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
