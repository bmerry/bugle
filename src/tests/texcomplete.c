/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2008-2010, 2012  Bruce Merry
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
    {
        "1D array", "GL_TEXTURE_1D_ARRAY(_EXT)?", GL_TEXTURE_1D_ARRAY, { GL_TEXTURE_1D_ARRAY },
        "#version 130\n"
        "uniform sampler1DArray s;\n"
        "void main() { gl_FragColor = texture(s, vec2(0.5, 0.5)); }\n"
    },
    {
        "2D array", "GL_TEXTURE_2D_ARRAY(_EXT)?", GL_TEXTURE_2D_ARRAY, { GL_TEXTURE_2D_ARRAY },
        "#version 130\n"
        "uniform sampler2DArray s;\n"
        "void main() { gl_FragColor = texture(s, vec3(0.5, 0.5, 0.5)); }\n"
    }
};

static void setup_texcomplete(void)
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

static void teardown_texcomplete(void)
{
    glDisableClientState(GL_VERTEX_ARRAY);
}

static void tex_image(GLenum face, int level, GLenum internalformat,
                      int width, int height, int depth, int layers, int border,
                      GLenum format, GLenum type, const void *pixels)
{
    switch (face)
    {
    case GL_TEXTURE_1D:
        glTexImage1D(face, level, internalformat, width, border,
                     format, type, pixels);
        break;
    case GL_TEXTURE_2D:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        glTexImage2D(face, level, internalformat, width, height, border,
                     format, type, pixels);
        break;
    case GL_TEXTURE_3D:
        glTexImage3D(face, level, internalformat, width, height, depth, border,
                     format, type, pixels);
        break;
    case GL_TEXTURE_1D_ARRAY:
        glTexImage2D(face, level, internalformat, width, layers, border,
                     format, type, pixels);
        break;
    case GL_TEXTURE_2D_ARRAY:
        glTexImage3D(face, level, internalformat, width, height, layers, border,
                     format, type, pixels);
        break;
    }
}

static void tex_images(const GLenum *faces, int level, GLenum internalformat,
                       int width, int height, int depth, int layers, int border,
                       GLenum format, GLenum type, const void *pixels)
{
    const GLenum *f;
    for (f = faces; *f; f++)
        tex_image(*f, level, internalformat, width, height, depth, layers, border,
                  format, type, pixels);
}

static GLuint make_texture_object(GLenum target, const GLenum *faces,
                                  texture_mode mode, const void *image)
{
    GLuint id;

    glGenTextures(1, &id);
    glBindTexture(target, id);

    if (target == GL_TEXTURE_CUBE_MAP)
    {
        switch (mode)
        {
        case MODE_COMPLETE_FULL:
            tex_images(faces, 0, GL_RGB8, 4, 4, 4, 4, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 1, GL_RGB8, 2, 2, 2, 4, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 2, GL_RGB8, 1, 1, 1, 4, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            break;
        case MODE_COMPLETE_MAX:
            glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, 1);
            tex_images(faces, 0, GL_RGB8, 4, 4, 4, 4, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 1, GL_RGB8, 2, 2, 2, 4, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            break;
        case MODE_COMPLETE_NOMIP:
            glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            tex_images(faces, 0, GL_RGB8, 4, 4, 4, 4, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            break;
        case MODE_INCOMPLETE_EMPTY:
            break;
        case MODE_INCOMPLETE_SHORT:
            tex_images(faces, 0, GL_RGB8, 4, 4, 4, 4, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 1, GL_RGB8, 2, 2, 2, 4, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            break;
        case MODE_INCOMPLETE_SIZE:
            tex_images(faces, 0, GL_RGB8, 4, 4, 4, 4, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 1, GL_RGB8, 1, 1, 1, 4, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 2, GL_RGB8, 1, 1, 1, 4, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            break;
        case MODE_INCOMPLETE_FORMAT:
            tex_images(faces, 0, GL_RGB8, 4, 4, 4, 4, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 1, GL_RGBA8, 2, 2, 2, 4, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 2, GL_RGB8, 1, 1, 1, 4, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            break;
        case MODE_INCOMPLETE_BORDER:
            tex_images(faces, 0, GL_RGB8, 6, 6, 6, 4, 1, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 1, GL_RGB8, 4, 4, 4, 4, 1, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 2, GL_RGB8, 1, 1, 1, 4, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            break;
        case MODE_INCOMPLETE_NO_LEVELS:
            tex_images(faces, 0, GL_RGB8, 4, 4, 4, 4, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 1, GL_RGB8, 2, 2, 2, 4, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 2, GL_RGB8, 1, 1, 1, 4, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
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
            tex_images(faces, 0, GL_RGB8, 4, 1, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 1, GL_RGB8, 2, 1, 1, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 2, GL_RGB8, 1, 1, 1, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            break;
        case MODE_COMPLETE_MAX:
            glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, 1);
            tex_images(faces, 0, GL_RGB8, 4, 1, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 1, GL_RGB8, 2, 1, 1, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            break;
        case MODE_COMPLETE_NOMIP:
            glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            tex_images(faces, 0, GL_RGB8, 4, 1, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            break;
        case MODE_INCOMPLETE_EMPTY:
            break;
        case MODE_INCOMPLETE_SHORT:
            tex_images(faces, 0, GL_RGB8, 4, 1, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 1, GL_RGB8, 2, 1, 1, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            break;
        case MODE_INCOMPLETE_SIZE:
            tex_images(faces, 0, GL_RGB8, 4, 1, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 1, GL_RGB8, 1, 1, 1, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 2, GL_RGB8, 1, 1, 1, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            break;
        case MODE_INCOMPLETE_FORMAT:
            tex_images(faces, 0, GL_RGB8, 4, 1, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 1, GL_RGBA8, 2, 1, 1, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 2, GL_RGB8, 1, 1, 1, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            break;
        case MODE_INCOMPLETE_BORDER:
            tex_images(faces, 0, GL_RGB8, 6, 3, 4, 4, 1, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 1, GL_RGB8, 4, 3, 3, 4, 1, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 2, GL_RGB8, 1, 1, 1, 4, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            break;
        case MODE_INCOMPLETE_NO_LEVELS:
            tex_images(faces, 0, GL_RGB8, 4, 1, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 1, GL_RGB8, 2, 1, 1, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            tex_images(faces, 2, GL_RGB8, 1, 1, 1, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
            glTexParameteri(target, GL_TEXTURE_BASE_LEVEL, 1);
            glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, 0);
            break;
        }
    }
    return id;
}

static void dump_program_log(GLuint program)
{
    GLint length;
    GLchar *lg;

    fprintf(stderr, "Linking failed:\n");
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    lg = malloc(length + 1);
    glGetProgramInfoLog(program, length + 1, &length, lg);
    fprintf(stderr, "%s\n", lg);
    free(lg);
}

static void dump_shader_log(GLuint shader)
{
    GLint length;
    GLchar *lg;

    fprintf(stderr, "Compilation failed:\n");
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    lg = malloc(length + 1);
    glGetShaderInfoLog(shader, length + 1, &length, lg);
    fprintf(stderr, "%s\n", lg);
    free(lg);
}

static GLuint make_shader(GLenum type, const char *source)
{
    GLuint shader;
    GLint status;

    shader = glCreateShader(type);
    glShaderSourceARB(shader, 1, &source, NULL);
    glCompileShaderARB(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    TEST_ASSERT(status);
    if (!status)
    {
        dump_shader_log(shader);
        glDeleteShader(shader);
        return 0;
    }
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
    TEST_ASSERT(status);
    if (vs) glDeleteShader(vs);
    if (fs) glDeleteShader(fs);
    if (!status)
    {
        dump_program_log(program);
        glDeleteProgram(program);
        return 0;
    }

    glUseProgram(program);
    return program;
}

static void texcomplete_test(GLenum target_enum)
{
    int i;
    texture_mode j;

    if (!GLEW_VERSION_3_0)
    {
        /* It's not required for this test, but it's easier to test this
         * way. */
        test_skipped("GL 3.0 required");
        return;
    }

    for (i = 0; i < sizeof(targets) / sizeof(texture_target); i++)
    {
        if (targets[i].target != target_enum)
            continue;
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
            if (!vs || !fs)
            {
                glDeleteShader(vs);
                glDeleteShader(fs);
                glDeleteTextures(1, &tex);
                test_failed("Failed to create shaders");
                return;
            }
            program = make_program(vs, fs);
            if (!program)
            {
                glDeleteTextures(1, &tex);
                test_failed("Failed to create program");
                return;
            }
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
}

static void texcomplete_1D(void)
{
    texcomplete_test(GL_TEXTURE_1D);
}

static void texcomplete_2D(void)
{
    texcomplete_test(GL_TEXTURE_2D);
}

static void texcomplete_3D(void)
{
    texcomplete_test(GL_TEXTURE_3D);
}

static void texcomplete_cube(void)
{
    texcomplete_test(GL_TEXTURE_CUBE_MAP);
}

static void texcomplete_1d_array(void)
{
    texcomplete_test(GL_TEXTURE_1D_ARRAY);
}

static void texcomplete_2d_array(void)
{
    texcomplete_test(GL_TEXTURE_2D_ARRAY);
}

void texcomplete_suite_register(void)
{
    test_suite *ts = test_suite_new("texcomplete", TEST_FLAG_LOG | TEST_FLAG_CONTEXT, setup_texcomplete, teardown_texcomplete);
    test_suite_add_test(ts, "1D", texcomplete_1D);
    test_suite_add_test(ts, "2D", texcomplete_2D);
    test_suite_add_test(ts, "3D", texcomplete_3D);
    test_suite_add_test(ts, "cube", texcomplete_cube);
    test_suite_add_test(ts, "1D_array", texcomplete_1d_array);
    test_suite_add_test(ts, "2D_array", texcomplete_2d_array);
}
