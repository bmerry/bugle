/* Tests the texture completeness validation code */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <GL/glew.h>
#include <GL/glext.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "test.h"

/* It's too much effort to make this test portable to older GL's */
#if GL_VERSION_2_0

typedef enum
{
    MODE_COMPLETE_FULL,
    MODE_COMPLETE_MAX,
    MODE_COMPLETE_NOMIP,
    MODE_INCOMPLETE_EMPTY,
    MODE_INCOMPLETE_SHORT,
    MODE_INCOMPLETE_SIZE,
    MODE_INCOMPLETE_FORMAT,
    MODE_INCOMPLETE_BORDER,
    MODE_INCOMPLETE_NO_LEVELS
} texture_mode;
#define MODE_COUNT ((int) MODE_INCOMPLETE_NO_LEVELS + 1)

typedef struct
{
    const char *name;
    const char *target_regex;
    GLenum target;
    GLenum faces[7];
    const char *source;
} texture_target;

static const GLchar vertex_source[] =
"#version 110\n"
"void main()\n"
"{\n"
"    gl_Position = gl_Vertex;\n"
"}\n";

static const texture_target targets[] =
{
    { 
        "1D", "GL_TEXTURE_1D",  GL_TEXTURE_1D, { GL_TEXTURE_1D }, 
        "#version 110\n"
            "uniform sampler1D s;\n"
            "void main() { gl_FragColor = texture1D(s, 0.5); }\n"
    },
    {
        "2D", "GL_TEXTURE_2D", GL_TEXTURE_2D, { GL_TEXTURE_2D },
        "#version 110\n"
            "uniform sampler2D s;\n"
            "void main() { gl_FragColor = texture2D(s, vec2(0.5, 0.5)); }\n"
    },
    {
        "3D", "GL_TEXTURE_3D(_EXT)?", GL_TEXTURE_3D, { GL_TEXTURE_3D },
        "#version 110\n"
            "uniform sampler3D s;\n"
            "void main() { gl_FragColor = texture3D(s, vec3(0.5, 0.5, 0.5)); }\n"
    },
    {
        "cube map", "GL_TEXTURE_CUBE_MAP(_(POSITIVE|NEGATIVE)_([XYZ]))?(_EXT|_ARB)?", GL_TEXTURE_CUBE_MAP,
        {
            GL_TEXTURE_CUBE_MAP_POSITIVE_X,
            GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
            GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
            GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
            GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
            GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
        },
        "#version 110\n"
            "uniform samplerCube s;\n"
            "void main() { gl_FragColor = textureCube(s, vec3(1.0, 0.5, 0.5)); }\n"
    },
};

static void state_init(void)
{
    static const int verts[8] =
    {
        -1, -1,
         1, -1,
         1,  1,
        -1,  1
    };
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_INT, 0, verts);
}

static void tex_image(GLenum face, int level, GLenum internalformat,
                      int width, int height, int depth, int border,
                      GLenum format, GLenum type, const void *pixels)
{
    switch (face)
    {
    case GL_TEXTURE_1D:
        glTexImage1D(face, level, internalformat, width, border,
                     format, type, pixels);
        break;
    case GL_TEXTURE_3D:
        glTexImage3D(face, level, internalformat, width, height, depth, border,
                     format, type, pixels);
        break;
    default:
        glTexImage2D(face, level, internalformat, width, height, border,
                     format, type, pixels);
        break;
    }
}

static void tex_images(const GLenum *faces, int level, GLenum internalformat,
                       int width, int height, int depth, int border,
                       GLenum format, GLenum type, const void *pixels)
{
    const GLenum *f;
    for (f = faces; *f; f++)
        tex_image(*f, level, internalformat, width, height, depth, border,
                  format, type, pixels);
}

static GLuint make_texture_object(GLenum target, const GLenum *faces,
                                  texture_mode mode, const void *image)
{
    GLuint id;

    glGenTextures(1, &id);
    glBindTexture(target, id);

    if (target == GL_TEXTURE_CUBE_MAP_ARB)
    {
        switch (mode)
        {
        case MODE_COMPLETE_FULL:
            tex_images(faces, 0, GL_RGB8, 4, 4, 4, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 1, GL_RGB8, 2, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 2, GL_RGB8, 1, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            break;
        case MODE_COMPLETE_MAX:
            glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, 1);
            tex_images(faces, 0, GL_RGB8, 4, 4, 4, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 1, GL_RGB8, 2, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            break;
        case MODE_COMPLETE_NOMIP:
            glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            tex_images(faces, 0, GL_RGB8, 4, 4, 4, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            break;
        case MODE_INCOMPLETE_EMPTY:
            break;
        case MODE_INCOMPLETE_SHORT:
            tex_images(faces, 0, GL_RGB8, 4, 4, 4, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 1, GL_RGB8, 2, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            break;
        case MODE_INCOMPLETE_SIZE:
            tex_images(faces, 0, GL_RGB8, 4, 4, 4, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 1, GL_RGB8, 1, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 2, GL_RGB8, 1, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            break;
        case MODE_INCOMPLETE_FORMAT:
            tex_images(faces, 0, GL_RGB8, 4, 4, 4, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 1, GL_RGBA8, 2, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 2, GL_RGB8, 1, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            break;
        case MODE_INCOMPLETE_BORDER:
            tex_images(faces, 0, GL_RGB8, 6, 6, 6, 1, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 1, GL_RGB8, 4, 4, 4, 1, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 2, GL_RGB8, 1, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            break;
        case MODE_INCOMPLETE_NO_LEVELS:
            tex_images(faces, 0, GL_RGB8, 4, 4, 4, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 1, GL_RGB8, 2, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 2, GL_RGB8, 1, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            glTexParameteri(target, GL_TEXTURE_BASE_LEVEL, 1);
            glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, 0);
            break;
        }
    }
    else
    {
        switch (mode)
        {
        case MODE_COMPLETE_FULL:
            tex_images(faces, 0, GL_RGB8, 4, 1, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 1, GL_RGB8, 2, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 2, GL_RGB8, 1, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            break;
        case MODE_COMPLETE_MAX:
            glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, 1);
            tex_images(faces, 0, GL_RGB8, 4, 1, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 1, GL_RGB8, 2, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            break;
        case MODE_COMPLETE_NOMIP:
            glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            tex_images(faces, 0, GL_RGB8, 4, 1, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            break;
        case MODE_INCOMPLETE_EMPTY:
            break;
        case MODE_INCOMPLETE_SHORT:
            tex_images(faces, 0, GL_RGB8, 4, 1, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 1, GL_RGB8, 2, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            break;
        case MODE_INCOMPLETE_SIZE:
            tex_images(faces, 0, GL_RGB8, 4, 1, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 1, GL_RGB8, 1, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 2, GL_RGB8, 1, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            break;
        case MODE_INCOMPLETE_FORMAT:
            tex_images(faces, 0, GL_RGB8, 4, 1, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 1, GL_RGBA8, 2, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 2, GL_RGB8, 1, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            break;
        case MODE_INCOMPLETE_BORDER:
            tex_images(faces, 0, GL_RGB8, 6, 3, 4, 1, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 1, GL_RGB8, 4, 3, 3, 1, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 2, GL_RGB8, 1, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            break;
        case MODE_INCOMPLETE_NO_LEVELS:
            tex_images(faces, 0, GL_RGB8, 4, 1, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 1, GL_RGB8, 2, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 2, GL_RGB8, 1, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            glTexParameteri(target, GL_TEXTURE_BASE_LEVEL, 1);
            glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, 0);
            break;
        }
    }
    return id;
}

static void dump_log(const char *phase,
                     void (*getiv)(GLuint, GLenum, GLint *),
                     void (*get)(GLuint, GLsizei, GLsizei *, GLchar *), GLuint object)
{
    GLint length;
    GLchar *lg;

    fprintf(stderr, "%s failed:\n", phase);
    getiv(object, GL_INFO_LOG_LENGTH, &length);
    lg = malloc(length + 1);
    get(object, length + 1, &length, lg);
    fprintf(stderr, "%s\n", lg);
    free(lg);
    exit(1);
}

static GLuint make_shader(GLenum type, const char *source)
{
    GLuint shader;
    GLint status;

    shader = glCreateShader(type);
    glShaderSourceARB(shader, 1, &source, NULL);
    glCompileShaderARB(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE)
        dump_log("compilation", glGetShaderiv, glGetShaderInfoLog, shader);
    return shader;
}

static GLuint make_program(GLuint vs, GLuint fs)
{
    GLuint program;
    GLint status;

    program = glCreateProgram();
    if (vs)
        glAttachShader(program, vs);
    if (fs)
        glAttachShader(program, fs);
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status != GL_TRUE)
        dump_log("linking", glGetProgramiv, glGetProgramInfoLog, program);

    glUseProgram(program);
    if (vs) glDeleteShader(vs);
    if (fs) glDeleteShader(fs);
    return program;
}

test_status texcomplete_suite(void)
{
    int i;
    texture_mode j;

    if (!GLEW_VERSION_2_0)
        return TEST_SKIPPED;
    state_init();

    for (i = 0; i < sizeof(targets) / sizeof(texture_target); i++)
    {
        fprintf(stderr, "Testing %s\n", targets[i].name);
        for (j = 0; j < MODE_COUNT; j++)
        {
            GLuint tex;
            GLuint program, vs, fs;
            GLint loc;
            GLubyte image[256];

            memset(image, 255, sizeof(image));
            tex = make_texture_object(targets[i].target, targets[i].faces, j, image);
            vs = make_shader(GL_VERTEX_SHADER, vertex_source);
            fs = make_shader(GL_FRAGMENT_SHADER, targets[i].source);
            program = make_program(vs, fs);
            loc = glGetUniformLocation(program, "s");
            TEST_ASSERT(loc >= 0);
            glUniform1i(loc, 0);

            glClear(GL_COLOR_BUFFER_BIT);
            glDrawArrays(GL_QUADS, 0, 4);
            /* Debug code
            {
                GLubyte pixel[4] = {0, 1, 2, 3};
                glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
                fprintf(stderr, "(%d,%d,%d,%d)\n", pixel[0], pixel[1], pixel[2], pixel[3]);
            }
            */

            if (j >= MODE_INCOMPLETE_EMPTY)
                test_log_printf("checks\\.texture: GL_TEXTURE0 / %s: incomplete texture.*\n",
                                targets[i].target_regex);

            glUseProgram(0);
            glDeleteProgram(program);
            glDeleteTextures(1, &tex);
        }
    }
    return TEST_RAN;
}

#else /* GL_VERSION_2_0 */

test_status texcomplete_suite(void)
{
    return TEST_SKIPPED;
}

#endif
