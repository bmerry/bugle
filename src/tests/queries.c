/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2010, 2013  Bruce Merry
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

/* Generates a variety of OpenGL queries, mainly for testing the logger */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <GL/glew.h>
#include <GL/gl.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <bugle/bool.h>
#include <bugle/memory.h>
#include "test.h"

/* Still TODO (how depressing)
 * - GetConvolutionFilter
 * - GetSeparableFilter
 * - GetHistogram
 * - GetMinmax
 * - GetPixelMap
 * - GetMap
 * - GetTexImage
 * - GetCompressedTexImage
 * - GetPolygonStipple
 * - GetShader
 * - GetProgram
 * - GetAttachedShaders
 * - GetShaderInfoLog
 * - GetProgramInfoLog
 * - GetShaderSource
 * - GetUniform
 * - GetUniformLocation
 * - Just about anything from GL 3.0 onwards
 */

static void dump_string(const char *value)
{
    if (value == NULL)
    {
        test_log_printf("NULL");
        return;
    }

    test_log_printf("\"");
    while (value[0])
    {
        switch (value[0])
        {
        case '"':  test_log_printf("\\\\\""); break;
        case '\\': test_log_printf("\\\\\\"); break;
        case '\n': test_log_printf("\\\\n"); break;
        case '\r': test_log_printf("\\\\r"); break;
        default:
            if (iscntrl(value[0]))
                test_log_printf("\\\\%03o", (int) value[0]);
            else if (isalnum(value[0]))
                test_log_printf("%c", value[0]);
            else
                test_log_printf("\\%c", value[0]);
        }
        value++;
    }
    test_log_printf("\"");
}

/* Query a bunch of things that are actually enumerants */
static void query_enums(void)
{
    GLint i;
    glGetIntegerv(GL_COLOR_MATERIAL_FACE, &i);
    test_log_printf("trace\\.call: glGetIntegerv\\(GL_COLOR_MATERIAL_FACE, %p -> GL_FRONT_AND_BACK\\)\n", (void *) &i);
    glGetIntegerv(GL_DEPTH_FUNC, &i);
    test_log_printf("trace\\.call: glGetIntegerv\\(GL_DEPTH_FUNC, %p -> GL_LESS\\)\n", (void *) &i);
    glGetIntegerv(GL_DRAW_BUFFER, &i);
    test_log_printf("trace\\.call: glGetIntegerv\\(GL_DRAW_BUFFER, %p -> GL_BACK\\)\n", (void *) &i);
}

/* Query a bunch of things that are actually booleans */
static void query_bools(void)
{
    GLint i;

    /* Enables */
    glGetIntegerv(GL_ALPHA_TEST, &i);
    test_log_printf("trace\\.call: glGetIntegerv\\(GL_ALPHA_TEST, %p -> GL_FALSE\\)\n", (void *) &i);
    glGetIntegerv(GL_LIGHTING, &i);
    test_log_printf("trace\\.call: glGetIntegerv\\(GL_LIGHTING, %p -> GL_FALSE\\)\n", (void *) &i);

    /* True booleans */
    glGetIntegerv(GL_DOUBLEBUFFER, &i);
    test_log_printf("trace\\.call: glGetIntegerv\\(GL_DOUBLEBUFFER, %p -> GL_TRUE\\)\n", (void *) &i);
    glGetIntegerv(GL_CURRENT_RASTER_POSITION_VALID, &i);
    test_log_printf("trace\\.call: glGetIntegerv\\(GL_CURRENT_RASTER_POSITION_VALID, %p -> GL_TRUE\\)\n", (void *) &i);
    glGetIntegerv(GL_LIGHT_MODEL_TWO_SIDE, &i);
    test_log_printf("trace\\.call: glGetIntegerv\\(GL_LIGHT_MODEL_TWO_SIDE, %p -> GL_FALSE\\)\n", (void *) &i);
}

static void query_pointers(void)
{
    GLfloat data[4] = {0.0f, 1.0f, 2.0f, 3.0f};
    GLvoid *ptr;

    glVertexPointer(4, GL_FLOAT, 0, data);
    test_log_printf("trace\\.call: glVertexPointer\\(4, GL_FLOAT, 0, %p\\)\n",
                    (void *) data);
    glGetPointerv(GL_VERTEX_ARRAY_POINTER, &ptr);
    test_log_printf("trace\\.call: glGetPointerv\\(GL_VERTEX_ARRAY_POINTER, %p -> %p\\)\n",
                    (void *) &ptr, (void *) data);
}

/* Query a bunch of things that are actually arrays */
static void query_multi(void)
{
    GLint i[16];
    GLdouble d[16];

    glGetIntegerv(GL_COLOR_WRITEMASK, i);
    test_log_printf("trace\\.call: glGetIntegerv\\(GL_COLOR_WRITEMASK, %p -> { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE }\\)\n", (void *) i);
    glGetDoublev(GL_CURRENT_COLOR, d);
    test_log_printf("trace\\.call: glGetDoublev\\(GL_CURRENT_COLOR, %p -> { 1, 1, 1, 1 }\\)\n", (void *) d);
    glGetDoublev(GL_MODELVIEW_MATRIX, d);
    test_log_printf("trace\\.call: glGetDoublev\\(GL_MODELVIEW_MATRIX, %p -> { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 }\\)\n", (void *) d);
}

/* Creates a copy of a string with regex special characters escaped. The caller
 * must free the memory afterwards.
 */
static char *re_escape(const char *s)
{
    size_t old_len = strlen(s);
    size_t new_len = 2 * old_len;
    char *out = (char *) malloc((new_len + 1) * sizeof(char));
    char *p = out;
    size_t i;

    for (i = 0; i < old_len; i++)
    {
        /* To keep the tests readable, we enumerate some common bad characters.
         * This may cause test failures if any slip through, but those can
         * be fixed as and when they occur.
         */
        switch (s[i])
        {
        case '(':
        case ')':
        case '[':
        case ']':
        case '+':
        case '*':
        case '?':
        case '{':
        case '}':
        case '\\':
        case '^':
        case '$':
        case '.':
        case '|':
            *p++ = '\\';
            /* Fall through */
        default:
            *p++ = s[i];
        }
    }
    *p++ = '\0';
    return out;
}

static void query_string(GLenum token, const char *name)
{
    const char *value;
    char *escaped;

    value = (const char *) glGetString(token);
    escaped = re_escape(value);
    test_log_printf("trace\\.call: glGetString\\(%s\\) = \"%s\"\n", name, escaped);
    free(escaped);
}

static void query_strings(void)
{
    query_string(GL_VENDOR, "GL_VENDOR");
    query_string(GL_RENDERER, "GL_RENDERER");
    query_string(GL_EXTENSIONS, "GL_EXTENSIONS");
}

static void query_tex_parameter(void)
{
    GLint i;
    GLfloat f[4];

    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &i);
    test_log_printf("trace\\.call: glGetTexParameteriv\\(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, %p -> GL_LINEAR\\)\n", (void *) &i);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_RESIDENT, &i);
    test_log_printf("trace\\.call: glGetTexParameteriv\\(GL_TEXTURE_2D, GL_TEXTURE_RESIDENT, %p -> (GL_TRUE|GL_FALSE)\\)\n", (void *) &i);
    glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, f);
    test_log_printf("trace\\.call: glGetTexParameterfv\\(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, %p -> { 0, 0, 0, 0 }\\)\n", (void *) f);
}

static void query_tex_level_parameter(void)
{
    GLint i;

    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &i);
    test_log_printf("trace\\.call: glGetTexLevelParameteriv\\(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, %p -> 0\\)\n", (void *) &i);
    /* ATI rather suspiciously return 0 here, which is why 0 is included in the regex */
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &i);
    test_log_printf("trace\\.call: glGetTexLevelParameteriv\\(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, %p -> ([0-4]|GL_[A-Z0-9_]+)\\)\n", (void *) &i);
}

static void query_tex_level_parameter_compressed(void)
{
    GLint i;

    if (GLEW_ARB_texture_compression)
    {
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED_ARB, &i);
        test_log_printf("trace\\.call: glGetTexLevelParameteriv\\(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED(_ARB)?, %p -> (GL_TRUE|GL_FALSE)\\)\n", (void *) &i);
    }
    else
        test_skipped("ARB_texture_compression required");
}

static void query_tex_gen(void)
{
    GLint mode;
    GLdouble plane[4];

    glGetTexGeniv(GL_S, GL_TEXTURE_GEN_MODE, &mode);
    test_log_printf("trace\\.call: glGetTexGeniv\\(GL_S, GL_TEXTURE_GEN_MODE, %p -> GL_EYE_LINEAR\\)\n",
                    (void *) &mode);
    glGetTexGendv(GL_T, GL_OBJECT_PLANE, plane);
    test_log_printf("trace\\.call: glGetTexGendv\\(GL_T, GL_OBJECT_PLANE, %p -> { 0, 1, 0, 0 }\\)\n",
                    (void *) plane);
}

static void query_tex_env(void)
{
    GLint mode;
    GLfloat color[4];

    glGetTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &mode);
    test_log_printf("trace\\.call: glGetTexEnviv\\(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, %p -> GL_MODULATE\\)\n",
                    (void *) &mode);
    glGetTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color);
    test_log_printf("trace\\.call: glGetTexEnvfv\\(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, %p -> { 0, 0, 0, 0 }\\)\n",
                    (void *) color);
}

static void query_tex_env_lod_bias(void)
{
    GLfloat bias;

    if (GLEW_EXT_texture_lod_bias)
    {
        glGetTexEnvfv(GL_TEXTURE_FILTER_CONTROL_EXT, GL_TEXTURE_LOD_BIAS_EXT, &bias);
        test_log_printf("trace\\.call: glGetTexEnvfv\\(GL_TEXTURE_FILTER_CONTROL(_EXT)?, GL_TEXTURE_LOD_BIAS(_EXT)?, %p -> 0\\)\n",
                        (void *) &bias);
    }
    else
        test_skipped("EXT_texture_lod_bias required");
}

static void query_tex_env_point_sprite(void)
{
    GLint mode;

    if (GLEW_ARB_point_sprite)
    {
        glGetTexEnviv(GL_POINT_SPRITE_ARB, GL_COORD_REPLACE_ARB, &mode);
        test_log_printf("trace\\.call: glGetTexEnviv\\(GL_POINT_SPRITE(_ARB)?, GL_COORD_REPLACE(_ARB)?, %p -> GL_FALSE\\)\n",
                        (void *) &mode);
    }
    else
        test_skipped("ARB_point_sprite required");
}

static void query_light(void)
{
    GLfloat f[4];

    glGetLightfv(GL_LIGHT1, GL_SPECULAR, f);
    test_log_printf("trace\\.call: glGetLightfv\\(GL_LIGHT1, GL_SPECULAR, %p -> { 0, 0, 0, 1 }\\)\n", (void *) f);
}

static void query_material(void)
{
    GLfloat f[4];

    glGetMaterialfv(GL_FRONT, GL_SPECULAR, f);
    test_log_printf("trace\\.call: glGetMaterialfv\\(GL_FRONT, GL_SPECULAR, %p -> { 0, 0, 0, 1 }\\)\n", (void *) f);
}

static void query_clip_plane(void)
{
    GLdouble eq[4];

    /* GL 3.1 renames this to GL_CLIP_DISTANCE0 */
    glGetClipPlane(GL_CLIP_PLANE0, eq);
    test_log_printf("trace\\.call: glGetClipPlane\\((GL_CLIP_PLANE0|GL_CLIP_DISTANCE0), %p -> { 0, 0, 0, 0 }\\)\n",
                    (void *) eq);
}

static void query_vertex_attrib(void)
{
    GLvoid *p;
    GLint i;
    GLdouble d[4];

    if (GLEW_ARB_vertex_program)
    {
        /* We use attribute 7, since ATI seems to use the same buffer for
         * attribute 0 and VertexPointer, and NVIDIA have incorrect initial
         * values for several initial attributes.
         */
        glGetVertexAttribPointervARB(7, GL_VERTEX_ATTRIB_ARRAY_POINTER_ARB, &p);
        test_log_printf("trace\\.call: glGetVertexAttribPointervARB\\(7, GL_VERTEX_ATTRIB_ARRAY_POINTER(_ARB)?, %p -> NULL\\)\n", (void *) &p);
        glGetVertexAttribdvARB(7, GL_CURRENT_VERTEX_ATTRIB_ARB, d);
        test_log_printf("trace\\.call: glGetVertexAttribdvARB\\(7, (GL_CURRENT_VERTEX_ATTRIB(_ARB)?|GL_CURRENT_ATTRIB_NV), %p -> { 0, 0, 0, 1 }\\)\n", (void *) d);
        glGetVertexAttribivARB(7, GL_VERTEX_ATTRIB_ARRAY_TYPE_ARB, &i);
        test_log_printf("trace\\.call: glGetVertexAttribivARB\\(7, (GL_VERTEX_ATTRIB_ARRAY_TYPE(_ARB)?|GL_ATTRIB_ARRAY_TYPE_NV), %p -> GL_FLOAT\\)\n", (void *) &i);
    }
    else
        test_skipped("ARB_vertex_program required");
}

static void query_query(void)
{
    GLint res;
    GLuint count;

    if (GLEW_ARB_occlusion_query)
    {
        glGetQueryivARB(GL_SAMPLES_PASSED_ARB, GL_QUERY_COUNTER_BITS_ARB, &res);
        test_log_printf("trace\\.call: glGetQueryivARB\\(GL_SAMPLES_PASSED(_ARB)?, GL_QUERY_COUNTER_BITS(_ARB)?, %p -> %d\\)\n",
                        (void *) &res, (int) res);

        glBeginQueryARB(GL_SAMPLES_PASSED_ARB, 1);
        test_log_printf("trace\\.call: glBeginQueryARB\\(GL_SAMPLES_PASSED(_ARB)?, 1\\)\n");
        glEndQueryARB(GL_SAMPLES_PASSED_ARB);
        test_log_printf("trace\\.call: glEndQueryARB\\(GL_SAMPLES_PASSED(_ARB)?\\)\n");
        glGetQueryObjectivARB(1, GL_QUERY_RESULT_AVAILABLE_ARB, &res);
        test_log_printf("trace\\.call: glGetQueryObjectivARB\\(1, GL_QUERY_RESULT_AVAILABLE(_ARB)?, %p -> (GL_TRUE|GL_FALSE)\\)\n",
                        (void *) &res);
        glGetQueryObjectuivARB(1, GL_QUERY_RESULT_ARB, &count);
        test_log_printf("trace\\.call: glGetQueryObjectuivARB\\(1, GL_QUERY_RESULT(_ARB)?, %p -> 0\\)\n",
                        (void *) &count);
    }
    else
        test_skipped("ARB_occlusion_query required");
}

static void query_buffer_parameter(void)
{
    GLubyte data[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    GLint res;
    GLvoid *ptr;

    if (GLEW_ARB_vertex_buffer_object)
    {
        glBindBufferARB(GL_ARRAY_BUFFER_ARB, 1);
        test_log_printf("trace\\.call: glBindBufferARB\\(GL_ARRAY_BUFFER(_ARB)?, 1\\)\n");
        glBufferDataARB(GL_ARRAY_BUFFER_ARB, 8, data, GL_STATIC_DRAW_ARB);
        test_log_printf("trace\\.call: glBufferDataARB\\(GL_ARRAY_BUFFER(_ARB)?, 8, %p -> { 0, 1, 2, 3, 4, 5, 6, 7 }, GL_STATIC_DRAW(_ARB)?\\)\n",
                        (void *) data);
        glGetBufferParameterivARB(GL_ARRAY_BUFFER_ARB, GL_BUFFER_MAPPED_ARB, &res);
        test_log_printf("trace\\.call: glGetBufferParameterivARB\\(GL_ARRAY_BUFFER(_ARB)?, GL_BUFFER_MAPPED(_ARB)?, %p -> GL_FALSE\\)\n",
                        (void *) &res);
        glGetBufferPointervARB(GL_ARRAY_BUFFER_ARB, GL_BUFFER_MAP_POINTER_ARB, &ptr);
        test_log_printf("trace\\.call: glGetBufferPointervARB\\(GL_ARRAY_BUFFER(_ARB)?, GL_BUFFER_MAP_POINTER(_ARB)?, %p -> NULL\\)\n",
                        (void *) &ptr);
    }
    else
        test_skipped("ARB_vertex_buffer_object required");
}

static void query_color_table(void)
{
    GLubyte data[12] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    GLfloat scale[4];
    GLint format = 0;

    if (GLEW_ARB_imaging)
    {
        glColorTable(GL_COLOR_TABLE, GL_RGB8, 4, GL_RGB, GL_UNSIGNED_BYTE, data);
        test_log_printf("trace\\.call: glColorTable\\(GL_COLOR_TABLE, GL_RGB8, 4, GL_RGB, GL_UNSIGNED_BYTE, %p\\)\n",
                        (void *) data);
        glGetColorTableParameteriv(GL_COLOR_TABLE, GL_COLOR_TABLE_FORMAT, &format);
        test_log_printf("trace\\.call: glGetColorTableParameteriv\\(GL_COLOR_TABLE, GL_COLOR_TABLE_FORMAT, %p -> GL_RGB8\\)\n",
                        (void *) &format);
        glGetColorTableParameterfv(GL_COLOR_TABLE, GL_COLOR_TABLE_SCALE, scale);
        test_log_printf("trace\\.call: glGetColorTableParameterfv\\(GL_COLOR_TABLE, GL_COLOR_TABLE_SCALE, %p -> { 1, 1, 1, 1 }\\)\n",
                        (void *) scale);
        glGetColorTable(GL_COLOR_TABLE, GL_RGB, GL_UNSIGNED_BYTE, data);
        test_log_printf("trace\\.call: glGetColorTable\\(GL_COLOR_TABLE, GL_RGB, GL_UNSIGNED_BYTE, %p\\)\n",
                        (void *) data);
    }
    else
        test_skipped("ARB_imaging required");
}

static void query_convolution(void)
{
    GLubyte data[3] = {0, 1, 2};
    GLfloat bias[4];
    GLint border_mode;

    if (GLEW_ARB_imaging)
    {
        glConvolutionFilter1D(GL_CONVOLUTION_1D, GL_LUMINANCE, 3, GL_LUMINANCE, GL_UNSIGNED_BYTE, data);
        test_log_printf("trace\\.call: glConvolutionFilter1D\\(GL_CONVOLUTION_1D, GL_LUMINANCE, 3, GL_LUMINANCE, GL_UNSIGNED_BYTE, %p\\)\n",
                        (void *) data);
        glGetConvolutionParameterfv(GL_CONVOLUTION_1D, GL_CONVOLUTION_FILTER_BIAS, bias);
        test_log_printf("trace\\.call: glGetConvolutionParameterfv\\(GL_CONVOLUTION_1D, GL_CONVOLUTION_FILTER_BIAS, %p -> { 0, 0, 0, 0 }\\)\n",
                        (void *) bias);
        glGetConvolutionParameteriv(GL_CONVOLUTION_1D, GL_CONVOLUTION_BORDER_MODE, &border_mode);
        test_log_printf("trace\\.call: glGetConvolutionParameteriv\\(GL_CONVOLUTION_1D, GL_CONVOLUTION_BORDER_MODE, %p -> GL_REDUCE\\)\n",
                        (void *) &border_mode);
    }
    else
        test_skipped("ARB_imaging required");
}

static void query_histogram(void)
{
    GLint format, sink;

    if (GLEW_ARB_imaging)
    {
        glHistogram(GL_HISTOGRAM, 16, GL_RGB, GL_FALSE);
        test_log_printf("trace\\.call: glHistogram\\(GL_HISTOGRAM, 16, GL_RGB, GL_FALSE\\)\n");
        glGetHistogramParameteriv(GL_HISTOGRAM, GL_HISTOGRAM_FORMAT, &format);
        test_log_printf("trace\\.call: glGetHistogramParameteriv\\(GL_HISTOGRAM, GL_HISTOGRAM_FORMAT, %p -> GL_RGB\\)\n",
                        (void *) &format);
        glGetHistogramParameteriv(GL_HISTOGRAM, GL_HISTOGRAM_SINK, &sink);
        test_log_printf("trace\\.call: glGetHistogramParameteriv\\(GL_HISTOGRAM, GL_HISTOGRAM_SINK, %p -> GL_FALSE\\)\n",
                        (void *) &sink);
    }
    else
        test_skipped("ARB_imaging required");
}

static void query_minmax(void)
{
    GLint format, sink;

    if (GLEW_ARB_imaging)
    {
        glMinmax(GL_MINMAX, GL_RGBA, GL_FALSE);
        test_log_printf("trace\\.call: glMinmax\\(GL_MINMAX, GL_RGBA, GL_FALSE\\)\n");
        glGetMinmaxParameteriv(GL_MINMAX, GL_MINMAX_FORMAT, &format);
        test_log_printf("trace\\.call: glGetMinmaxParameteriv\\(GL_MINMAX, GL_MINMAX_FORMAT, %p -> GL_RGBA\\)\n",
                        (void *) &format);
        glGetMinmaxParameteriv(GL_MINMAX, GL_MINMAX_SINK, &sink);
        test_log_printf("trace\\.call: glGetMinmaxParameteriv\\(GL_MINMAX, GL_MINMAX_SINK, %p -> GL_FALSE\\)\n",
                        (void *) &sink);
    }
    else
        test_skipped("ARB_imaging required");
}

static void shader_source(GLhandleARB shader, const char *source)
{
    GLint status;

    glShaderSourceARB(shader, 1, &source, NULL);
    test_log_printf("trace\\.call: glShaderSourceARB\\(%u, 1, %p -> { ",
                    (unsigned int) shader, &source);
    dump_string(source);
    test_log_printf(" }, NULL\\)\n");

    glCompileShaderARB(shader);
    test_log_printf("trace\\.call: glCompileShaderARB\\(%u\\)\n",
                    (unsigned int) shader);

    glGetObjectParameterivARB(shader, GL_OBJECT_COMPILE_STATUS_ARB, &status);
    test_log_printf("trace\\.call: glGetObjectParameterivARB\\(%u, (GL_OBJECT_COMPILE_STATUS_ARB|GL_COMPILE_STATUS), %p -> GL_TRUE\\)\n",
                    (unsigned int) shader, &status);
}

static void query_shaders(void)
{
    GLhandleARB vs, fs, p, attached[4];
    GLsizei count;
    GLint location;
    GLsizei length, size;
    GLenum type;
    const char *language_version;
    const char *vs_source100 =
        "uniform mat4 m;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = m * gl_Vertex;\n"
        "}\n";
    const char *vs_source120 =
        "#version 120\n"
        "uniform mat4x3 m;\n"
        "void main()\n"
        "{\n"
        "    gl_Position.xyz = m * gl_Vertex;\n"
        "    gl_Position.w = 1.0;\n"
        "}\n";
    const char *fs_source =
        "void main()\n"
        "{\n"
        "    gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);\n"
        "}\n";
    const char *vs_source;
    char name[128];
    bugle_bool lang120;

    if (GLEW_ARB_shader_objects
        && GLEW_ARB_vertex_shader
        && GLEW_ARB_fragment_shader)
    {
        vs = glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
        test_log_printf("trace\\.call: glCreateShaderObjectARB\\(GL_VERTEX_SHADER(_ARB)?\\) = %u\n",
                        (unsigned int) vs);
        fs = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
        test_log_printf("trace\\.call: glCreateShaderObjectARB\\(GL_FRAGMENT_SHADER(_ARB)?\\) = %u\n",
                        (unsigned int) fs);
        p = glCreateProgramObjectARB();
        test_log_printf("trace\\.call: glCreateProgramObjectARB\\(\\) = %u\n",
                        (unsigned int) p);

        glAttachObjectARB(p, vs);
        test_log_printf("trace\\.call: glAttachObjectARB\\(%u, %u\\)\n",
                        (unsigned int) p, (unsigned int) vs);
        glAttachObjectARB(p, fs);
        test_log_printf("trace\\.call: glAttachObjectARB\\(%u, %u\\)\n",
                        (unsigned int) p, (unsigned int) fs);
        glGetAttachedObjectsARB(p, 2, &count, attached);
        test_log_printf("trace\\.call: glGetAttachedObjectsARB\\(%u, 2, %p -> 2, %p -> { %u, %u }\\)\n",
                        (unsigned int) p, (void *) &count, (void *) attached, (unsigned int) vs, (unsigned int) fs);
        glGetAttachedObjectsARB(p, 4, NULL, attached);
        test_log_printf("trace\\.call: glGetAttachedObjectsARB\\(%u, 4, NULL, %p -> { %u, %u }\\)\n",
                        (unsigned int) p, (void *) attached, (unsigned int) vs, (unsigned int) fs);

        language_version = (const char *) glGetString(GL_SHADING_LANGUAGE_VERSION_ARB);
        test_log_printf("trace\\.call: glGetString\\(GL_SHADING_LANGUAGE_VERSION(_ARB)?\\) = ");
        dump_string(language_version);
        test_log_printf("\n");

        lang120 = strcmp(language_version, "1.20") >= 0;
        vs_source = lang120 ? vs_source120 : vs_source100;
        shader_source(vs, vs_source);
        shader_source(fs, fs_source);
        glLinkProgramARB(p);
        test_log_printf("trace\\.call: glLinkProgramARB\\(%u\\)\n",
                        (unsigned int) p);

        location = glGetUniformLocationARB(p, "m");
        test_log_printf("trace\\.call: glGetUniformLocationARB\\(%u, \"m\"\\) = %d\n",
                        (unsigned int) p, location);
        glGetActiveUniformARB(p, 0, sizeof(name), &length, &size, &type, name);
        /* Prerelease OpenGL 2.1 drivers from NVIDIA return GL_FLOAT_MAT3 in
         * this situation. Mesa 7.0.2 returns the size as 16 instead of 1.
         */
        test_log_printf("trace\\.call: glGetActiveUniformARB\\(%u, %d, %u, %p -> 1, %p -> %d, %p -> %s, \"%s\"\\)\n",
                        (unsigned int) p, (int) location, (unsigned int) sizeof(name),
                        &length, &size, (int) size, &type,
                        lang120 ? (type == GL_FLOAT_MAT3 ? "GL_FLOAT_MAT3" : "GL_FLOAT_MAT4x3") : "GL_FLOAT_MAT4",
                        name);
    }
    else
        test_skipped("ARB shaders required");
    /* FIXME: test lots more things e.g. source */
}

static void query_ll_programs(void)
{
    static const char *vp = "!!ARBvp1.0\nMOV result.position, vertex.position;\nEND\n";
    char *source;

    GLuint program;
    if (GLEW_ARB_vertex_program)
    {
        GLint param;
        glGenProgramsARB(1, &program);
        test_log_printf("trace\\.call: glGenProgramsARB\\(1, %p -> { %u }\\)\n", (void *) &program, (unsigned int) program);
        glBindProgramARB(GL_VERTEX_PROGRAM_ARB, program);
        test_log_printf("trace\\.call: glBindProgramARB\\(GL_VERTEX_PROGRAM_ARB, %u\\)\n", (unsigned int) program);
        glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB,
                           strlen(vp), vp);
        test_log_printf("trace\\.call: glProgramStringARB\\(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, %u, ",
                        (unsigned int) strlen(vp));
        dump_string(vp);
        test_log_printf("\\)\n");

        glGetProgramivARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ARB, &param);
        test_log_printf("trace\\.call: glGetProgramivARB\\(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ARB, %p -> GL_PROGRAM_FORMAT_ASCII_ARB\\)\n",
                        (void *) &param);
        glGetProgramivARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_LENGTH_ARB, &param);
        test_log_printf("trace\\.call: glGetProgramivARB\\(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_LENGTH_ARB, %p -> %u\\)\n",
                        (void *) &param, (unsigned int) strlen(vp));
        source = malloc(strlen(vp) + 1);
        /* Make sure that the dumper counts things right, since glGetProgramStringARB
         * does NOT write a terminating NULL. */
        source[strlen(vp)] = '\1';
        glGetProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_STRING_ARB, source);
        test_log_printf("trace\\.call: glGetProgramStringARB\\(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_STRING_ARB, ");
        dump_string(vp);
        test_log_printf("\\)\n");
        free(source);

        glDeleteProgramsARB(1, &program);
        test_log_printf("trace\\.call: glDeleteProgramsARB\\(1, %p -> { %u }\\)\n",
                        (void *) &program, (unsigned int) program);
    }
    else
        test_skipped("ARB_vertex_program required");
}

/* Generates empty contents for a texture */
static void texture2D(GLenum target, const char *target_regex)
{
    glTexImage2D(target, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    test_log_printf("trace\\.call: glTexImage2D\\(%s, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL\\)\n", target_regex);
    glTexImage2D(target, 1, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    test_log_printf("trace\\.call: glTexImage2D\\(%s, 1, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL\\)\n", target_regex);
    glTexImage2D(target, 2, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    test_log_printf("trace\\.call: glTexImage2D\\(%s, 2, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL\\)\n", target_regex);
}

/* Same but for 3D */
static void texture3D(GLenum target, const char *target_regex)
{
    glTexImage3DEXT(target, 0, GL_RGBA, 4, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    test_log_printf("trace\\.call: glTexImage3DEXT\\(%s, 0, GL_RGBA, 4, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL\\)\n", target_regex);
    glTexImage3DEXT(target, 1, GL_RGBA, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    test_log_printf("trace\\.call: glTexImage3DEXT\\(%s, 1, GL_RGBA, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL\\)\n", target_regex);
    glTexImage3DEXT(target, 2, GL_RGBA, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    test_log_printf("trace\\.call: glTexImage3DEXT\\(%s, 2, GL_RGBA, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL\\)\n", target_regex);
}

static void query_framebuffers(void)
{
    GLuint fb, tex[3];
    GLint i;

    if (GLEW_EXT_framebuffer_object)
    {
        glGenTextures(3, tex);
        test_log_printf("trace\\.call: glGenTextures\\(3, %p -> { %u, %u, %u }\\)\n",
                        (void *) tex, (unsigned int) tex[0], (unsigned int) tex[1], (unsigned int) tex[2]);
        glGenFramebuffersEXT(1, &fb);
        test_log_printf("trace\\.call: glGenFramebuffersEXT\\(1, %p -> { %u }\\)\n",
                        (void *) &fb, (unsigned int) fb);
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fb);
        test_log_printf("trace\\.call: glBindFramebufferEXT\\(GL_FRAMEBUFFER(_EXT)?, %u\\)\n",
                        (unsigned int) fb);

        /* 2D texture */
        glBindTexture(GL_TEXTURE_2D, tex[0]);
        test_log_printf("trace\\.call: glBindTexture\\(GL_TEXTURE_2D, %u\\)\n",
                        (unsigned int) tex[0]);
        texture2D(GL_TEXTURE_2D, "GL_TEXTURE_2D");
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                                  GL_TEXTURE_2D, tex[0], 1);
        test_log_printf("trace\\.call: glFramebufferTexture2DEXT\\(GL_FRAMEBUFFER(_EXT)?, GL_COLOR_ATTACHMENT0(_EXT)?, GL_TEXTURE_2D, %u, 1\\)\n",
                        (unsigned int) tex[0]);
        glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &i);
        test_log_printf("trace\\.call: glGetIntegerv\\(GL_FRAMEBUFFER_BINDING(_EXT)?, %p -> %i\\)\n",
                        (void *) &i, (int) fb);
        glGetFramebufferAttachmentParameterivEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                                                 GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE_EXT, &i);
        test_log_printf("trace\\.call: glGetFramebufferAttachmentParameterivEXT\\(GL_FRAMEBUFFER(_EXT)?, GL_COLOR_ATTACHMENT0(_EXT)?, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE(_EXT)?, %p -> GL_TEXTURE\\)\n",
                        (void *) &i);
        glGetFramebufferAttachmentParameterivEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                                                 GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME_EXT, &i);
        test_log_printf("trace\\.call: glGetFramebufferAttachmentParameterivEXT\\(GL_FRAMEBUFFER(_EXT)?, GL_COLOR_ATTACHMENT0(_EXT)?, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME(_EXT)?, %p -> %i\\)\n",
                        (void *) &i, (int) tex[0]);
        glGetFramebufferAttachmentParameterivEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                                                 GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL_EXT, &i);
        test_log_printf("trace\\.call: glGetFramebufferAttachmentParameterivEXT\\(GL_FRAMEBUFFER(_EXT)?, GL_COLOR_ATTACHMENT0(_EXT)?, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL(_EXT)?, %p -> 1\\)\n",
                        (void *) &i);

        /* 3D texture */
        if (GLEW_EXT_texture3D)
        {
            glBindTexture(GL_TEXTURE_3D_EXT, tex[1]);
            test_log_printf("trace\\.call: glBindTexture\\(GL_TEXTURE_3D(_EXT)?, %u\\)\n",
                            (unsigned int) tex[1]);
            texture3D(GL_TEXTURE_3D_EXT, "GL_TEXTURE_3D(_EXT)?");
            glFramebufferTexture3DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                                      GL_TEXTURE_3D, tex[1], 1, 1);
            test_log_printf("trace\\.call: glFramebufferTexture3DEXT\\(GL_FRAMEBUFFER(_EXT)?, GL_COLOR_ATTACHMENT0(_EXT)?, GL_TEXTURE_3D(_EXT)?, %u, 1, 1\\)\n",
                            (unsigned int) tex[1]);
            glGetFramebufferAttachmentParameterivEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                                                     GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_3D_ZOFFSET_EXT, &i);
            test_log_printf("trace\\.call: glGetFramebufferAttachmentParameterivEXT\\(GL_FRAMEBUFFER(_EXT)?, GL_COLOR_ATTACHMENT0(_EXT)?, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_(3D_ZOFFSET|LAYER)(_EXT)?, %p -> 1\\)\n",
                            (void *) &i);
        }

        /* Cube map texture */
        if (GLEW_ARB_texture_cube_map)
        {
            glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, tex[2]);
            test_log_printf("trace\\.call: glBindTexture\\(GL_TEXTURE_CUBE_MAP(_ARB)?, %u\\)\n",
                            (unsigned int) tex[2]);
            texture2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X_ARB, "GL_TEXTURE_CUBE_MAP_NEGATIVE_X(_ARB)?");
            texture2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_ARB, "GL_TEXTURE_CUBE_MAP_NEGATIVE_Y(_ARB)?");
            texture2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_ARB, "GL_TEXTURE_CUBE_MAP_NEGATIVE_Z(_ARB)?");
            texture2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB, "GL_TEXTURE_CUBE_MAP_POSITIVE_X(_ARB)?");
            texture2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y_ARB, "GL_TEXTURE_CUBE_MAP_POSITIVE_Y(_ARB)?");
            texture2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z_ARB, "GL_TEXTURE_CUBE_MAP_POSITIVE_Z(_ARB)?");
            glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                                      GL_TEXTURE_CUBE_MAP_NEGATIVE_X_ARB, tex[2], 1);
            test_log_printf("trace\\.call: glFramebufferTexture2DEXT\\(GL_FRAMEBUFFER(_EXT)?, GL_COLOR_ATTACHMENT0(_EXT)?, GL_TEXTURE_CUBE_MAP_NEGATIVE_X(_ARB)?, %u, 1\\)\n",
                            (unsigned int) tex[2]);
            glGetFramebufferAttachmentParameterivEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                                                     GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE_EXT, &i);
            test_log_printf("trace\\.call: glGetFramebufferAttachmentParameterivEXT\\(GL_FRAMEBUFFER(_EXT)?, GL_COLOR_ATTACHMENT0(_EXT)?, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE(_EXT)?, %p -> GL_TEXTURE_CUBE_MAP_NEGATIVE_X(_ARB)?\\)\n",
                            (void *) &i);
        }

        glDeleteFramebuffersEXT(1, &fb);
        test_log_printf("trace\\.call: glDeleteFramebuffersEXT\\(1, %p -> { %u }\\)\n",
                        (void *) &fb, (unsigned int) fb);
        glDeleteTextures(3, tex);
        test_log_printf("trace\\.call: glDeleteTextures\\(3, %p -> { %u, %u, %u }\\)\n",
                        (void *) tex, (unsigned int) tex[0], (unsigned int) tex[1], (unsigned int) tex[2]);
    }
    else
        test_skipped("EXT_framebuffer_object required");
}

static void query_renderbuffers(void)
{
    GLuint rb;
    GLint i;

    if (GLEW_EXT_framebuffer_object)
    {
        glGenRenderbuffersEXT(1, &rb);
        test_log_printf("trace\\.call: glGenRenderbuffersEXT\\(1, %p -> { %u }\\)\n",
                        (void *) &rb, (unsigned int) rb);
        glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, rb);
        test_log_printf("trace\\.call: glBindRenderbufferEXT\\(GL_RENDERBUFFER(_EXT)?, %u\\)\n",
                        (unsigned int) rb);
        glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_RGBA8, 64, 64);
        test_log_printf("trace\\.call: glRenderbufferStorageEXT\\(GL_RENDERBUFFER(_EXT)?, GL_RGBA8, 64, 64\\)\n");
        glGetRenderbufferParameterivEXT(GL_RENDERBUFFER_EXT, GL_RENDERBUFFER_WIDTH_EXT, &i);
        test_log_printf("trace\\.call: glGetRenderbufferParameterivEXT\\(GL_RENDERBUFFER(_EXT)?, GL_RENDERBUFFER_WIDTH(_EXT)?, %p -> 64\\)\n",
                        (void *) &i);
        glGetRenderbufferParameterivEXT(GL_RENDERBUFFER_EXT, GL_RENDERBUFFER_INTERNAL_FORMAT_EXT, &i);
        test_log_printf("trace\\.call: glGetRenderbufferParameterivEXT\\(GL_RENDERBUFFER(_EXT)?, GL_RENDERBUFFER_INTERNAL_FORMAT(_EXT)?, %p -> GL_RGBA8\\)\n",
                        (void *) &i);
        glGetRenderbufferParameterivEXT(GL_RENDERBUFFER_EXT, GL_RENDERBUFFER_RED_SIZE_EXT, &i);
        test_log_printf("trace\\.call: glGetRenderbufferParameterivEXT\\(GL_RENDERBUFFER(_EXT)?, GL_RENDERBUFFER_RED_SIZE(_EXT)?, %p -> %d\\)\n",
                        (void *) &i, (int) i);
        glDeleteRenderbuffersEXT(1, &rb);
        test_log_printf("trace\\.call: glDeleteRenderbuffersEXT\\(1, %p -> { %u }\\)\n",
                        (void *) &rb, (unsigned int) rb);
    }
    else
        test_skipped("EXT_framebuffer_object required");
}

static void query_readpixels(void)
{
    GLubyte ans[14];
    int i;

    glReadPixels(1, 1, 2, 2, GL_RGB, GL_UNSIGNED_BYTE, ans);
    test_log_printf("trace\\.call: glReadPixels\\(1, 1, 2, 2, GL_RGB, GL_UNSIGNED_BYTE, %p -> { ",
                    ans);
    /* 14 elements because of the padding due to alignment */
    for (i = 0; i < 13; i++)
        test_log_printf("%d, ", (int) ans[i]);
    test_log_printf("%d }\\)\n", ans[13]);
}

static void query_sync(void)
{
    if (GLEW_ARB_sync || GLEW_VERSION_3_2)
    {
        GLint v;
        GLint len;

        GLsync sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        test_log_printf("trace\\.call: glFenceSync\\(GL_SYNC_GPU_COMMANDS_COMPLETE, 0\\) = %p\n", sync);
        glClientWaitSync(sync, GL_SYNC_FLUSH_COMMANDS_BIT, (GLuint64) -1);
        test_log_printf("trace\\.call: glClientWaitSync\\(%p, GL_SYNC_FLUSH_COMMANDS_BIT, 18446744073709551615\\) = GL_.*\n",
                        sync);
        glClientWaitSync(sync, 0, (GLuint64) -1);
        test_log_printf("trace\\.call: glClientWaitSync\\(%p, 0, 18446744073709551615\\) = GL_ALREADY_SIGNALED\n",
                        sync);

        glGetSynciv(sync, GL_SYNC_CONDITION, 1, &len, &v);
        test_log_printf("trace\\.call: glGetSynciv\\(%p, GL_SYNC_CONDITION, 1, %p -> 1, %p -> GL_SYNC_GPU_COMMANDS_COMPLETE\\)\n",
                        sync, &len, &v);
        glGetSynciv(sync, GL_SYNC_FLAGS, 1, NULL, &v);
        test_log_printf("trace\\.call: glGetSynciv\\(%p, GL_SYNC_FLAGS, 1, NULL, %p -> 0\\)\n",
                        sync, &v);
    }
    else
        test_skipped("ARB_sync required");
}

static void query_compressed_texture_formats(void)
{
    if (GLEW_ARB_texture_compression)
    {
        GLint num, i;
        GLint *formats;

        glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &num);
        test_log_printf("trace\\.call: glGetIntegerv\\(GL_NUM_COMPRESSED_TEXTURE_FORMATS, %p -> %d\\)\n",
                        &num, num);
        if (num > 0)
        {
            formats = BUGLE_NMALLOC(num, GLint);
            glGetIntegerv(GL_COMPRESSED_TEXTURE_FORMATS, formats);
            test_log_printf("trace\\.call: glGetIntegerv\\(GL_COMPRESSED_TEXTURE_FORMATS, %p -> { ",
                            formats);
            for (i = 0; i < num; i++)
            {
                if (i > 0)
                    test_log_printf(", ");
                test_log_printf("(GL_[A-Za-z0-9_]+|<unknown enum 0x[A-Fa-f0-9]+>)");
            }
            test_log_printf(" }\\)\n");
            bugle_free(formats);
        }
        else
            test_skipped("No compressed texture formats supported");
    }
    else
        test_skipped("ARB_compressed_texture_formats is required");
}

void queries_suite_register(void)
{
    test_suite *ts = test_suite_new("queries", TEST_FLAG_LOG | TEST_FLAG_CONTEXT, NULL, NULL);
    test_suite_add_test(ts, "enums", query_enums);
    test_suite_add_test(ts, "bools", query_bools);
    test_suite_add_test(ts, "pointers", query_pointers);
    test_suite_add_test(ts, "multi", query_multi);
    test_suite_add_test(ts, "tex_parameter", query_tex_parameter);
    test_suite_add_test(ts, "tex_level_parameter", query_tex_level_parameter);
    test_suite_add_test(ts, "tex_level_parameter_compressed", query_tex_level_parameter_compressed);
    test_suite_add_test(ts, "tex_gen", query_tex_gen);
    test_suite_add_test(ts, "tex_env", query_tex_env);
    test_suite_add_test(ts, "tex_env_point_sprite", query_tex_env_point_sprite);
    test_suite_add_test(ts, "tex_env_lod_bias", query_tex_env_lod_bias);
    test_suite_add_test(ts, "light", query_light);
    test_suite_add_test(ts, "material", query_material);
    test_suite_add_test(ts, "clip_plane", query_clip_plane);
    test_suite_add_test(ts, "vertex_attrib", query_vertex_attrib);
    test_suite_add_test(ts, "query", query_query);
    test_suite_add_test(ts, "buffer_parameter", query_buffer_parameter);
    test_suite_add_test(ts, "color_table", query_color_table);
    test_suite_add_test(ts, "convolution", query_convolution);
    test_suite_add_test(ts, "histogram", query_histogram);
    test_suite_add_test(ts, "minmax", query_minmax);
    test_suite_add_test(ts, "strings", query_strings);
    test_suite_add_test(ts, "shaders", query_shaders);
    test_suite_add_test(ts, "ll_programs", query_ll_programs);
    test_suite_add_test(ts, "framebuffers", query_framebuffers);
    test_suite_add_test(ts, "renderbuffers", query_renderbuffers);
    test_suite_add_test(ts, "readpixels", query_readpixels);
    test_suite_add_test(ts, "sync", query_sync);
    test_suite_add_test(ts, "compressed_texture_formats", query_compressed_texture_formats);
}
