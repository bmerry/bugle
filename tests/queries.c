/* Generates a variety of OpenGL queries, mainly for testing the logger */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#define _POSIX_SOURCE
#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glut.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <bugle/bool.h>

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
 * - G80 extensions
 */

static FILE *ref;

static void dump_string(FILE *out, const char *value)
{
    if (value == NULL) fputs("NULL", out);
    else
    {
        fputc('"', out);
        while (value[0])
        {
            switch (value[0])
            {
            case '"': fputs("\\\\\"", out); break;
            case '\\': fputs("\\\\\\", out); break;
            case '\n': fputs("\\\\n", out); break;
            case '\r': fputs("\\\\r", out); break;
            default:
                if (iscntrl(value[0]))
                    fprintf(out, "\\\\%03o", (int) value[0]);
                else if (isalnum(value[0]))
                    fputc(value[0], out);
                else
                {
                    fputc('\\', out);
                    fputc(value[0], out);
                }
            }
            value++;
        }
        fputc('"', out);
    }
}

/* Query a bunch of things that are actually enumerants */
static void query_enums(void)
{
    GLint i;
    glGetIntegerv(GL_COLOR_MATERIAL_FACE, &i);
    fprintf(ref, "trace\\.call: glGetIntegerv\\(GL_COLOR_MATERIAL_FACE, %p -> GL_FRONT_AND_BACK\\)\n", (void *) &i);
    glGetIntegerv(GL_DEPTH_FUNC, &i);
    fprintf(ref, "trace\\.call: glGetIntegerv\\(GL_DEPTH_FUNC, %p -> GL_LESS\\)\n", (void *) &i);
    glGetIntegerv(GL_DRAW_BUFFER, &i);
    fprintf(ref, "trace\\.call: glGetIntegerv\\(GL_DRAW_BUFFER, %p -> GL_BACK\\)\n", (void *) &i);
}

/* Query a bunch of things that are actually booleans */
static void query_bools(void)
{
    GLint i;

    /* Enables */
    glGetIntegerv(GL_ALPHA_TEST, &i);
    fprintf(ref, "trace\\.call: glGetIntegerv\\(GL_ALPHA_TEST, %p -> GL_FALSE\\)\n", (void *) &i);
    glGetIntegerv(GL_LIGHTING, &i);
    fprintf(ref, "trace\\.call: glGetIntegerv\\(GL_LIGHTING, %p -> GL_FALSE\\)\n", (void *) &i);

    /* True booleans */
    glGetIntegerv(GL_DOUBLEBUFFER, &i);
    fprintf(ref, "trace\\.call: glGetIntegerv\\(GL_DOUBLEBUFFER, %p -> GL_TRUE\\)\n", (void *) &i);
    glGetIntegerv(GL_CURRENT_RASTER_POSITION_VALID, &i);
    fprintf(ref, "trace\\.call: glGetIntegerv\\(GL_CURRENT_RASTER_POSITION_VALID, %p -> GL_TRUE\\)\n", (void *) &i);
    glGetIntegerv(GL_LIGHT_MODEL_TWO_SIDE, &i);
    fprintf(ref, "trace\\.call: glGetIntegerv\\(GL_LIGHT_MODEL_TWO_SIDE, %p -> GL_FALSE\\)\n", (void *) &i);
}

static void query_pointers(void)
{
    GLfloat data[4] = {0.0f, 1.0f, 2.0f, 3.0f};
    GLvoid *ptr;

    glVertexPointer(4, GL_FLOAT, 0, data);
    fprintf(ref, "trace\\.call: glVertexPointer\\(4, GL_FLOAT, 0, %p\\)\n",
            (void *) data);
    glGetPointerv(GL_VERTEX_ARRAY_POINTER, &ptr);
    fprintf(ref, "trace\\.call: glGetPointerv\\(GL_VERTEX_ARRAY_POINTER, %p -> %p\\)\n",
            (void *) &ptr, (void *) data);
}

/* Query a bunch of things that are actually arrays */
static void query_multi(void)
{
    GLint i[16];
    GLdouble d[16];

    glGetIntegerv(GL_COLOR_WRITEMASK, i);
    fprintf(ref, "trace\\.call: glGetIntegerv\\(GL_COLOR_WRITEMASK, %p -> { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE }\\)\n", (void *) i);
    glGetDoublev(GL_CURRENT_COLOR, d);
    fprintf(ref, "trace\\.call: glGetDoublev\\(GL_CURRENT_COLOR, %p -> { 1, 1, 1, 1 }\\)\n", (void *) d);
    glGetDoublev(GL_MODELVIEW_MATRIX, d);
    fprintf(ref, "trace\\.call: glGetDoublev\\(GL_MODELVIEW_MATRIX, %p -> { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 }\\)\n", (void *) d);
}

static void query_strings(void)
{
    fprintf(ref, "trace\\.call: glGetString\\(GL_VENDOR\\) = \"%s\"\n", glGetString(GL_VENDOR));
    fprintf(ref, "trace\\.call: glGetString\\(GL_RENDERER\\) = \"%s\"\n", glGetString(GL_RENDERER));
    fprintf(ref, "trace\\.call: glGetString\\(GL_EXTENSIONS\\) = \"%s\"\n", glGetString(GL_EXTENSIONS));
}

static void query_tex_parameter(void)
{
    GLint i;
    GLfloat f[4];

    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &i);
    fprintf(ref, "trace\\.call: glGetTexParameteriv\\(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, %p -> GL_LINEAR\\)\n", (void *) &i);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_RESIDENT, &i);
    fprintf(ref, "trace\\.call: glGetTexParameteriv\\(GL_TEXTURE_2D, GL_TEXTURE_RESIDENT, %p -> (GL_TRUE|GL_FALSE)\\)\n", (void *) &i);
    glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, f);
    fprintf(ref, "trace\\.call: glGetTexParameterfv\\(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, %p -> { 0, 0, 0, 0 }\\)\n", (void *) f);
}

static void query_tex_level_parameter(void)
{
    GLint i;

    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &i);
    fprintf(ref, "trace\\.call: glGetTexLevelParameteriv\\(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, %p -> 0\\)\n", (void *) &i);
    /* ATI rather suspiciously return 0 here, which is why 0 is included in the regex */
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &i);
    fprintf(ref, "trace\\.call: glGetTexLevelParameteriv\\(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, %p -> ([0-4]|GL_[A-Z0-9_]+)\\)\n", (void *) &i);
    if (GLEW_ARB_texture_compression)
    {
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED_ARB, &i);
        fprintf(ref, "trace\\.call: glGetTexLevelParameteriv\\(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED(_ARB)?, %p -> (GL_TRUE|GL_FALSE)\\)\n", (void *) &i);
    }
}

static void query_tex_gen(void)
{
    GLint mode;
    GLdouble plane[4];

    glGetTexGeniv(GL_S, GL_TEXTURE_GEN_MODE, &mode);
    fprintf(ref, "trace\\.call: glGetTexGeniv\\(GL_S, GL_TEXTURE_GEN_MODE, %p -> GL_EYE_LINEAR\\)\n",
           (void *) &mode);
    glGetTexGendv(GL_T, GL_OBJECT_PLANE, plane);
    fprintf(ref, "trace\\.call: glGetTexGendv\\(GL_T, GL_OBJECT_PLANE, %p -> { 0, 1, 0, 0 }\\)\n",
            (void *) plane);
}

static void query_tex_env(void)
{
    GLint mode;
    GLfloat color[4];

    glGetTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &mode);
    fprintf(ref, "trace\\.call: glGetTexEnviv\\(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, %p -> GL_MODULATE\\)\n",
            (void *) &mode);
    glGetTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color);
    fprintf(ref, "trace\\.call: glGetTexEnvfv\\(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, %p -> { 0, 0, 0, 0 }\\)\n",
            (void *) color);

    if (GLEW_EXT_texture_lod_bias)
    {
        glGetTexEnvfv(GL_TEXTURE_FILTER_CONTROL_EXT, GL_TEXTURE_LOD_BIAS_EXT, color);
        fprintf(ref, "trace\\.call: glGetTexEnvfv\\(GL_TEXTURE_FILTER_CONTROL(_EXT)?, GL_TEXTURE_LOD_BIAS(_EXT)?, %p -> 0\\)\n",
                (void *) color);
    }
    if (GLEW_ARB_point_sprite)
    {
        glGetTexEnviv(GL_POINT_SPRITE_ARB, GL_COORD_REPLACE_ARB, &mode);
        fprintf(ref, "trace\\.call: glGetTexEnviv\\(GL_POINT_SPRITE(_ARB)?, GL_COORD_REPLACE(_ARB)?, %p -> GL_FALSE\\)\n",
                (void *) &mode);
    }
}

static void query_light(void)
{
    GLfloat f[4];

    glGetLightfv(GL_LIGHT1, GL_SPECULAR, f);
    fprintf(ref, "trace\\.call: glGetLightfv\\(GL_LIGHT1, GL_SPECULAR, %p -> { 0, 0, 0, 1 }\\)\n", (void *) f);
}

static void query_material(void)
{
    GLfloat f[4];

    glGetMaterialfv(GL_FRONT, GL_SPECULAR, f);
    fprintf(ref, "trace\\.call: glGetMaterialfv\\(GL_FRONT, GL_SPECULAR, %p -> { 0, 0, 0, 1 }\\)\n", (void *) f);
}

static void query_clip_plane(void)
{
    GLdouble eq[4];

    glGetClipPlane(GL_CLIP_PLANE0, eq);
    fprintf(ref, "trace\\.call: glGetClipPlane\\(GL_CLIP_PLANE0, %p -> { 0, 0, 0, 0 }\\)\n",
            (void *) eq);
}

static void query_vertex_attrib(void)
{
    GLvoid *p;
    GLint i;
    GLdouble d[4];

    if (GLEW_ARB_vertex_program)
    {
        /* We use attribute 6, since ATI seems to use the same buffer for
         * attribute 0 and VertexPointer.
         */
        glGetVertexAttribPointervARB(6, GL_VERTEX_ATTRIB_ARRAY_POINTER_ARB, &p);
        fprintf(ref, "trace\\.call: glGetVertexAttribPointervARB\\(6, GL_VERTEX_ATTRIB_ARRAY_POINTER(_ARB)?, %p -> NULL\\)\n", (void *) &p);
        glGetVertexAttribdvARB(6, GL_CURRENT_VERTEX_ATTRIB_ARB, d);
        fprintf(ref, "trace\\.call: glGetVertexAttribdvARB\\(6, (GL_CURRENT_VERTEX_ATTRIB(_ARB)?|GL_CURRENT_ATTRIB_NV), %p -> { 0, 0, 0, 1 }\\)\n", (void *) d);
        glGetVertexAttribivARB(6, GL_VERTEX_ATTRIB_ARRAY_TYPE_ARB, &i);
        fprintf(ref, "trace\\.call: glGetVertexAttribivARB\\(6, (GL_VERTEX_ATTRIB_ARRAY_TYPE(_ARB)?|GL_ATTRIB_ARRAY_TYPE_NV), %p -> GL_FLOAT\\)\n", (void *) &i);
    }
}

static void query_query(void)
{
    GLint res;
    GLuint count;

    if (GLEW_ARB_occlusion_query)
    {
        glGetQueryivARB(GL_SAMPLES_PASSED_ARB, GL_QUERY_COUNTER_BITS_ARB, &res);
        fprintf(ref, "trace\\.call: glGetQueryivARB\\(GL_SAMPLES_PASSED(_ARB)?, GL_QUERY_COUNTER_BITS(_ARB)?, %p -> %d\\)\n",
                (void *) &res, (int) res);

        glBeginQueryARB(GL_SAMPLES_PASSED_ARB, 1);
        fprintf(ref, "trace\\.call: glBeginQueryARB\\(GL_SAMPLES_PASSED(_ARB)?, 1\\)\n");
        glEndQueryARB(GL_SAMPLES_PASSED_ARB);
        fprintf(ref, "trace\\.call: glEndQueryARB\\(GL_SAMPLES_PASSED(_ARB)?\\)\n");
        glGetQueryObjectivARB(1, GL_QUERY_RESULT_AVAILABLE_ARB, &res);
        fprintf(ref, "trace\\.call: glGetQueryObjectivARB\\(1, GL_QUERY_RESULT_AVAILABLE(_ARB)?, %p -> (GL_TRUE|GL_FALSE)\\)\n",
                (void *) &res);
        glGetQueryObjectuivARB(1, GL_QUERY_RESULT_ARB, &count);
        fprintf(ref, "trace\\.call: glGetQueryObjectuivARB\\(1, GL_QUERY_RESULT(_ARB)?, %p -> 0\\)\n",
                (void *) &count);
    }
}

static void query_buffer_parameter(void)
{
    GLubyte data[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    GLint res;
    GLvoid *ptr;

    if (GLEW_ARB_vertex_buffer_object)
    {
        glBindBufferARB(GL_ARRAY_BUFFER_ARB, 1);
        fprintf(ref, "trace\\.call: glBindBufferARB\\(GL_ARRAY_BUFFER(_ARB)?, 1\\)\n");
        glBufferDataARB(GL_ARRAY_BUFFER_ARB, 8, data, GL_STATIC_DRAW_ARB);
        fprintf(ref, "trace\\.call: glBufferDataARB\\(GL_ARRAY_BUFFER(_ARB)?, 8, %p -> { 0, 1, 2, 3, 4, 5, 6, 7 }, GL_STATIC_DRAW(_ARB)?\\)\n",
                (void *) data);
        glGetBufferParameterivARB(GL_ARRAY_BUFFER_ARB, GL_BUFFER_MAPPED_ARB, &res);
        fprintf(ref, "trace\\.call: glGetBufferParameterivARB\\(GL_ARRAY_BUFFER(_ARB)?, GL_BUFFER_MAPPED(_ARB)?, %p -> GL_FALSE\\)\n",
                (void *) &res);
        glGetBufferPointervARB(GL_ARRAY_BUFFER_ARB, GL_BUFFER_MAP_POINTER_ARB, &ptr);
        fprintf(ref, "trace\\.call: glGetBufferPointervARB\\(GL_ARRAY_BUFFER(_ARB)?, GL_BUFFER_MAP_POINTER(_ARB)?, %p -> NULL\\)\n",
                (void *) &ptr);
    }
}

static void query_color_table(void)
{
    GLubyte data[12] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    GLfloat scale[4];
    GLint format;

    if (GLEW_ARB_imaging)
    {
        glColorTable(GL_COLOR_TABLE, GL_RGB8, 4, GL_RGB, GL_UNSIGNED_BYTE, data);
        fprintf(ref, "trace\\.call: glColorTable\\(GL_COLOR_TABLE, GL_RGB8, 4, GL_RGB, GL_UNSIGNED_BYTE, %p\\)\n",
                (void *) data);
        glGetColorTableParameteriv(GL_COLOR_TABLE, GL_COLOR_TABLE_FORMAT, &format);
        fprintf(ref, "trace\\.call: glGetColorTableParameteriv\\(GL_COLOR_TABLE, GL_COLOR_TABLE_FORMAT, %p -> GL_RGB8\\)\n",
                (void *) &format);
        glGetColorTableParameterfv(GL_COLOR_TABLE, GL_COLOR_TABLE_SCALE, scale);
        fprintf(ref, "trace\\.call: glGetColorTableParameterfv\\(GL_COLOR_TABLE, GL_COLOR_TABLE_SCALE, %p -> { 1, 1, 1, 1 }\\)\n",
                (void *) scale);
        glGetColorTable(GL_COLOR_TABLE, GL_RGB, GL_UNSIGNED_BYTE, data);
        fprintf(ref, "trace\\.call: glGetColorTable\\(GL_COLOR_TABLE, GL_RGB, GL_UNSIGNED_BYTE, %p\\)\n",
                (void *) data);
    }
}

static void query_convolution(void)
{
    GLubyte data[3] = {0, 1, 2};
    GLfloat bias[4];
    GLint border_mode;

    if (GLEW_ARB_imaging)
    {
        glConvolutionFilter1D(GL_CONVOLUTION_1D, GL_LUMINANCE, 3, GL_LUMINANCE, GL_UNSIGNED_BYTE, data);
        fprintf(ref, "trace\\.call: glConvolutionFilter1D\\(GL_CONVOLUTION_1D, GL_LUMINANCE, 3, GL_LUMINANCE, GL_UNSIGNED_BYTE, %p\\)\n",
                (void *) data);
        glGetConvolutionParameterfv(GL_CONVOLUTION_1D, GL_CONVOLUTION_FILTER_BIAS, bias);
        fprintf(ref, "trace\\.call: glGetConvolutionParameterfv\\(GL_CONVOLUTION_1D, GL_CONVOLUTION_FILTER_BIAS, %p -> { 0, 0, 0, 0 }\\)\n",
                (void *) bias);
        glGetConvolutionParameteriv(GL_CONVOLUTION_1D, GL_CONVOLUTION_BORDER_MODE, &border_mode);
        fprintf(ref, "trace\\.call: glGetConvolutionParameteriv\\(GL_CONVOLUTION_1D, GL_CONVOLUTION_BORDER_MODE, %p -> GL_REDUCE\\)\n",
                (void *) &border_mode);
    }
}

static void query_histogram(void)
{
    GLint format, sink;

    if (GLEW_ARB_imaging)
    {
        glHistogram(GL_HISTOGRAM, 16, GL_RGB, GL_FALSE);
        fprintf(ref, "trace\\.call: glHistogram\\(GL_HISTOGRAM, 16, GL_RGB, GL_FALSE\\)\n");
        glGetHistogramParameteriv(GL_HISTOGRAM, GL_HISTOGRAM_FORMAT, &format);
        fprintf(ref, "trace\\.call: glGetHistogramParameteriv\\(GL_HISTOGRAM, GL_HISTOGRAM_FORMAT, %p -> GL_RGB\\)\n",
                (void *) &format);
        glGetHistogramParameteriv(GL_HISTOGRAM, GL_HISTOGRAM_SINK, &sink);
        fprintf(ref, "trace\\.call: glGetHistogramParameteriv\\(GL_HISTOGRAM, GL_HISTOGRAM_SINK, %p -> GL_FALSE\\)\n",
                (void *) &sink);
    }
}

static void query_minmax(void)
{
    GLint format, sink;

    if (GLEW_ARB_imaging)
    {
        glMinmax(GL_MINMAX, GL_RGBA, GL_FALSE);
        fprintf(ref, "trace\\.call: glMinmax\\(GL_MINMAX, GL_RGBA, GL_FALSE\\)\n");
        glGetMinmaxParameteriv(GL_MINMAX, GL_MINMAX_FORMAT, &format);
        fprintf(ref, "trace\\.call: glGetMinmaxParameteriv\\(GL_MINMAX, GL_MINMAX_FORMAT, %p -> GL_RGBA\\)\n",
                (void *) &format);
        glGetMinmaxParameteriv(GL_MINMAX, GL_MINMAX_SINK, &sink);
        fprintf(ref, "trace\\.call: glGetMinmaxParameteriv\\(GL_MINMAX, GL_MINMAX_SINK, %p -> GL_FALSE\\)\n",
                (void *) &sink);
    }
}

static void shader_source(GLhandleARB shader, const char *source)
{
    GLint status;

    glShaderSourceARB(shader, 1, &source, NULL);
    fprintf(ref, "trace\\.call: glShaderSourceARB\\(%u, 1, %p -> { ",
            (unsigned int) shader, &source);
    dump_string(ref, source);
    fprintf(ref, " }, NULL\\)\n");

    glCompileShaderARB(shader);
    fprintf(ref, "trace\\.call: glCompileShaderARB\\(%u\\)\n",
            (unsigned int) shader);

    glGetObjectParameterivARB(shader, GL_OBJECT_COMPILE_STATUS_ARB, &status);
    fprintf(ref, "trace\\.call: glGetObjectParameterivARB\\(%u, (GL_OBJECT_COMPILE_STATUS_ARB|GL_COMPILE_STATUS), %p -> GL_TRUE\\)\n",
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
        fprintf(ref, "trace\\.call: glCreateShaderObjectARB\\(GL_VERTEX_SHADER(_ARB)?\\) = %u\n",
                (unsigned int) vs);
        fs = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
        fprintf(ref, "trace\\.call: glCreateShaderObjectARB\\(GL_FRAGMENT_SHADER(_ARB)?\\) = %u\n",
                (unsigned int) fs);
        p = glCreateProgramObjectARB();
        fprintf(ref, "trace\\.call: glCreateProgramObjectARB\\(\\) = %u\n",
                (unsigned int) p);

        glAttachObjectARB(p, vs);
        fprintf(ref, "trace\\.call: glAttachObjectARB\\(%u, %u\\)\n",
                (unsigned int) p, (unsigned int) vs);
        glAttachObjectARB(p, fs);
        fprintf(ref, "trace\\.call: glAttachObjectARB\\(%u, %u\\)\n",
                (unsigned int) p, (unsigned int) fs);
        glGetAttachedObjectsARB(p, 2, &count, attached);
        fprintf(ref, "trace\\.call: glGetAttachedObjectsARB\\(%u, 2, %p -> 2, %p -> { %u, %u }\\)\n",
                (unsigned int) p, (void *) &count, (void *) attached, (unsigned int) vs, (unsigned int) fs);
        glGetAttachedObjectsARB(p, 4, NULL, attached);
        fprintf(ref, "trace\\.call: glGetAttachedObjectsARB\\(%u, 4, NULL, %p -> { %u, %u }\\)\n",
                (unsigned int) p, (void *) attached, (unsigned int) vs, (unsigned int) fs);

        language_version = (const char *) glGetString(GL_SHADING_LANGUAGE_VERSION_ARB);
        fprintf(ref, "trace\\.call: glGetString\\(GL_SHADING_LANGUAGE_VERSION(_ARB)?\\) = ");
        dump_string(ref, language_version);
        fprintf(ref, "\n");

        lang120 = strcmp(language_version, "1.20") >= 0;
        vs_source = lang120 ? vs_source120 : vs_source100;
        shader_source(vs, vs_source);
        shader_source(fs, fs_source);
        glLinkProgramARB(p);
        fprintf(ref, "trace\\.call: glLinkProgramARB\\(%u\\)\n",
                (unsigned int) p);

        location = glGetUniformLocationARB(p, "m");
        fprintf(ref, "trace\\.call: glGetUniformLocationARB\\(%u, \"m\"\\) = %d\n",
                (unsigned int) p, location);
        glGetActiveUniformARB(p, 0, sizeof(name), &length, &size, &type, name);
        /* Prerelease OpenGL 2.1 drivers from NVIDIA return GL_FLOAT_MAT3 in
         * this situation. Mesa 7.0.2 returns the size as 16 instead of 1.
         */
        fprintf(ref, "trace\\.call: glGetActiveUniformARB\\(%u, %d, %u, %p -> 1, %p -> %d, %p -> %s, \"%s\"\\)\n",
                (unsigned int) p, (int) location, (unsigned int) sizeof(name),
                &length, &size, (int) size, &type,
                lang120 ? (type == GL_FLOAT_MAT3 ? "GL_FLOAT_MAT3" : "GL_FLOAT_MAT4x3") : "GL_FLOAT_MAT4",
                name);
    }
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
        fprintf(ref, "trace\\.call: glGenProgramsARB\\(1, %p -> { %u }\\)\n", (void *) &program, (unsigned int) program);
        glBindProgramARB(GL_VERTEX_PROGRAM_ARB, program);
        fprintf(ref, "trace\\.call: glBindProgramARB\\(GL_VERTEX_PROGRAM_ARB, %u\\)\n", (unsigned int) program);
        glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB,
                           strlen(vp), vp);
        fprintf(ref, "trace\\.call: glProgramStringARB\\(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, %u, ",
                (unsigned int) strlen(vp));
        dump_string(ref, vp);
        fprintf(ref, "\\)\n");

        glGetProgramivARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ARB, &param);
        fprintf(ref, "trace\\.call: glGetProgramivARB\\(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ARB, %p -> GL_PROGRAM_FORMAT_ASCII_ARB\\)\n",
                (void *) &param);
        glGetProgramivARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_LENGTH_ARB, &param);
        fprintf(ref, "trace\\.call: glGetProgramivARB\\(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_LENGTH_ARB, %p -> %u\\)\n",
                (void *) &param, (unsigned int) strlen(vp));
        source = malloc(strlen(vp) + 1);
        /* Make sure that the dumper counts things right, since glGetProgramStringARB
         * does NOT write a terminating NULL. */
        source[strlen(vp)] = '\1';
        glGetProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_STRING_ARB, source);
        fprintf(ref, "trace\\.call: glGetProgramStringARB\\(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_STRING_ARB, ");
        dump_string(ref, vp);
        fprintf(ref, "\\)\n");
        free(source);

        glDeleteProgramsARB(1, &program);
        fprintf(ref, "trace\\.call: glDeleteProgramsARB\\(1, %p -> { %u }\\)\n",
                (void *) &program, (unsigned int) program);
    }
}

/* Generates empty contents for a texture */
static void texture2D(GLenum target, const char *target_regex)
{
    glTexImage2D(target, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    fprintf(ref, "trace\\.call: glTexImage2D\\(%s, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL\\)\n", target_regex);
    glTexImage2D(target, 1, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    fprintf(ref, "trace\\.call: glTexImage2D\\(%s, 1, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL\\)\n", target_regex);
    glTexImage2D(target, 2, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    fprintf(ref, "trace\\.call: glTexImage2D\\(%s, 2, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL\\)\n", target_regex);
}

/* Same but for 3D */
static void texture3D(GLenum target, const char *target_regex)
{
    glTexImage3DEXT(target, 0, GL_RGBA, 4, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    fprintf(ref, "trace\\.call: glTexImage3DEXT\\(%s, 0, GL_RGBA, 4, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL\\)\n", target_regex);
    glTexImage3DEXT(target, 1, GL_RGBA, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    fprintf(ref, "trace\\.call: glTexImage3DEXT\\(%s, 1, GL_RGBA, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL\\)\n", target_regex);
    glTexImage3DEXT(target, 2, GL_RGBA, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    fprintf(ref, "trace\\.call: glTexImage3DEXT\\(%s, 2, GL_RGBA, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL\\)\n", target_regex);
}

static void query_framebuffers(void)
{
    GLuint fb, tex[3];
    GLint i;

    if (GLEW_EXT_framebuffer_object)
    {
        glGenTextures(3, tex);
        fprintf(ref, "trace\\.call: glGenTextures\\(3, %p -> { %u, %u, %u }\\)\n",
                (void *) tex, (unsigned int) tex[0], (unsigned int) tex[1], (unsigned int) tex[2]);
        glGenFramebuffersEXT(1, &fb);
        fprintf(ref, "trace\\.call: glGenFramebuffersEXT\\(1, %p -> { %u }\\)\n",
                (void *) &fb, (unsigned int) fb);
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fb);
        fprintf(ref, "trace\\.call: glBindFramebufferEXT\\(GL_FRAMEBUFFER(_EXT)?, %u\\)\n",
                (unsigned int) fb);

        /* 2D texture */
        glBindTexture(GL_TEXTURE_2D, tex[0]);
        fprintf(ref, "trace\\.call: glBindTexture\\(GL_TEXTURE_2D, %u\\)\n",
                (unsigned int) tex[0]);
        texture2D(GL_TEXTURE_2D, "GL_TEXTURE_2D");
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                                  GL_TEXTURE_2D, tex[0], 1);
        fprintf(ref, "trace\\.call: glFramebufferTexture2DEXT\\(GL_FRAMEBUFFER(_EXT)?, GL_COLOR_ATTACHMENT0(_EXT)?, GL_TEXTURE_2D, %u, 1\\)\n",
               (unsigned int) tex[0]);
        glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &i);
        fprintf(ref, "trace\\.call: glGetIntegerv\\(GL_FRAMEBUFFER_BINDING(_EXT)?, %p -> %i\\)\n",
                (void *) &i, (int) fb);
        glGetFramebufferAttachmentParameterivEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                                                 GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE_EXT, &i);
        fprintf(ref, "trace\\.call: glGetFramebufferAttachmentParameterivEXT\\(GL_FRAMEBUFFER(_EXT)?, GL_COLOR_ATTACHMENT0(_EXT)?, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE(_EXT)?, %p -> GL_TEXTURE\\)\n",
                (void *) &i);
        glGetFramebufferAttachmentParameterivEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                                                 GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME_EXT, &i);
        fprintf(ref, "trace\\.call: glGetFramebufferAttachmentParameterivEXT\\(GL_FRAMEBUFFER(_EXT)?, GL_COLOR_ATTACHMENT0(_EXT)?, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME(_EXT)?, %p -> %i\\)\n",
                (void *) &i, (int) tex[0]);
        glGetFramebufferAttachmentParameterivEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                                                 GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL_EXT, &i);
        fprintf(ref, "trace\\.call: glGetFramebufferAttachmentParameterivEXT\\(GL_FRAMEBUFFER(_EXT)?, GL_COLOR_ATTACHMENT0(_EXT)?, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL(_EXT)?, %p -> 1\\)\n",
                (void *) &i);

        /* 3D texture */
        if (GLEW_EXT_texture3D)
        {
            glBindTexture(GL_TEXTURE_3D_EXT, tex[1]);
            fprintf(ref, "trace\\.call: glBindTexture\\(GL_TEXTURE_3D(_EXT)?, %u\\)\n",
                    (unsigned int) tex[1]);
            texture3D(GL_TEXTURE_3D_EXT, "GL_TEXTURE_3D(_EXT)?");
            glFramebufferTexture3DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                                      GL_TEXTURE_3D, tex[1], 1, 1);
            fprintf(ref, "trace\\.call: glFramebufferTexture3DEXT\\(GL_FRAMEBUFFER(_EXT)?, GL_COLOR_ATTACHMENT0(_EXT)?, GL_TEXTURE_3D(_EXT)?, %u, 1, 1\\)\n",
                    (unsigned int) tex[1]);
            glGetFramebufferAttachmentParameterivEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                                                     GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_3D_ZOFFSET_EXT, &i);
            fprintf(ref, "trace\\.call: glGetFramebufferAttachmentParameterivEXT\\(GL_FRAMEBUFFER(_EXT)?, GL_COLOR_ATTACHMENT0(_EXT)?, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_3D_ZOFFSET(_EXT)?, %p -> 1\\)\n",
                    (void *) &i);
        }

        /* Cube map texture */
        if (GLEW_ARB_texture_cube_map)
        {
            glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, tex[2]);
            fprintf(ref, "trace\\.call: glBindTexture\\(GL_TEXTURE_CUBE_MAP(_ARB)?, %u\\)\n",
                    (unsigned int) tex[2]);
            texture2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X_ARB, "GL_TEXTURE_CUBE_MAP_NEGATIVE_X(_ARB)?");
            texture2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_ARB, "GL_TEXTURE_CUBE_MAP_NEGATIVE_Y(_ARB)?");
            texture2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_ARB, "GL_TEXTURE_CUBE_MAP_NEGATIVE_Z(_ARB)?");
            texture2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB, "GL_TEXTURE_CUBE_MAP_POSITIVE_X(_ARB)?");
            texture2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y_ARB, "GL_TEXTURE_CUBE_MAP_POSITIVE_Y(_ARB)?");
            texture2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z_ARB, "GL_TEXTURE_CUBE_MAP_POSITIVE_Z(_ARB)?");
            glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                                      GL_TEXTURE_CUBE_MAP_NEGATIVE_X_ARB, tex[2], 1);
            fprintf(ref, "trace\\.call: glFramebufferTexture2DEXT\\(GL_FRAMEBUFFER(_EXT)?, GL_COLOR_ATTACHMENT0(_EXT)?, GL_TEXTURE_CUBE_MAP_NEGATIVE_X(_ARB)?, %u, 1\\)\n",
                   (unsigned int) tex[2]);
            glGetFramebufferAttachmentParameterivEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                                                     GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE_EXT, &i);
            fprintf(ref, "trace\\.call: glGetFramebufferAttachmentParameterivEXT\\(GL_FRAMEBUFFER(_EXT)?, GL_COLOR_ATTACHMENT0(_EXT)?, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE(_EXT)?, %p -> GL_TEXTURE_CUBE_MAP_NEGATIVE_X(_ARB)?\\)\n",
                    (void *) &i);
        }

        glDeleteFramebuffersEXT(1, &fb);
        fprintf(ref, "trace\\.call: glDeleteFramebuffersEXT\\(1, %p -> { %u }\\)\n",
                (void *) &fb, (unsigned int) fb);
        glDeleteTextures(3, tex);
        fprintf(ref, "trace\\.call: glDeleteTextures\\(3, %p -> { %u, %u, %u }\\)\n",
                (void *) tex, (unsigned int) tex[0], (unsigned int) tex[1], (unsigned int) tex[2]);
    }
}

static void query_renderbuffers(void)
{
    GLuint rb;
    GLint i;

    if (GLEW_EXT_framebuffer_object)
    {
        glGenRenderbuffersEXT(1, &rb);
        fprintf(ref, "trace\\.call: glGenRenderbuffersEXT\\(1, %p -> { %u }\\)\n",
                (void *) &rb, (unsigned int) rb);
        glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, rb);
        fprintf(ref, "trace\\.call: glBindRenderbufferEXT\\(GL_RENDERBUFFER(_EXT)?, %u\\)\n",
                (unsigned int) rb);
        glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_RGBA8, 64, 64);
        fprintf(ref, "trace\\.call: glRenderbufferStorageEXT\\(GL_RENDERBUFFER(_EXT)?, GL_RGBA8, 64, 64\\)\n");
        glGetRenderbufferParameterivEXT(GL_RENDERBUFFER_EXT, GL_RENDERBUFFER_WIDTH_EXT, &i);
        fprintf(ref, "trace\\.call: glGetRenderbufferParameterivEXT\\(GL_RENDERBUFFER(_EXT)?, GL_RENDERBUFFER_WIDTH(_EXT)?, %p -> 64\\)\n",
                (void *) &i);
        glGetRenderbufferParameterivEXT(GL_RENDERBUFFER_EXT, GL_RENDERBUFFER_INTERNAL_FORMAT_EXT, &i);
        fprintf(ref, "trace\\.call: glGetRenderbufferParameterivEXT\\(GL_RENDERBUFFER(_EXT)?, GL_RENDERBUFFER_INTERNAL_FORMAT(_EXT)?, %p -> GL_RGBA8\\)\n",
                (void *) &i);
        glGetRenderbufferParameterivEXT(GL_RENDERBUFFER_EXT, GL_RENDERBUFFER_RED_SIZE_EXT, &i);
        fprintf(ref, "trace\\.call: glGetRenderbufferParameterivEXT\\(GL_RENDERBUFFER(_EXT)?, GL_RENDERBUFFER_RED_SIZE(_EXT)?, %p -> %d\\)\n",
                (void *) &i, (int) i);
        glDeleteRenderbuffersEXT(1, &rb);
        fprintf(ref, "trace\\.call: glDeleteRenderbuffersEXT\\(1, %p -> { %u }\\)\n",
                (void *) &rb, (unsigned int) rb);
    }
}

static void query_readpixels(void)
{
    GLubyte ans[14];
    int i;

    glReadPixels(1, 1, 2, 2, GL_RGB, GL_UNSIGNED_BYTE, ans);
    fprintf(ref, "trace\\.call: glReadPixels\\(1, 1, 2, 2, GL_RGB, GL_UNSIGNED_BYTE, %p -> { ",
            ans);
    /* 14 elements because of the padding due to alignment */
    for (i = 0; i < 13; i++)
        fprintf(ref, "%d, ", (int) ans[i]);
    fprintf(ref, "%d }\\)\n", ans[13]);
}

static void query_transform_feedback(void)
{
    if (GLEW_NV_transform_feedback)
    {
        GLuint buffers[4];
        GLint bound;
        GLint record[3];
        int i;

        const GLint attribs[12] =
        {
            GL_POSITION, 4, 0,
            GL_TEXTURE_COORD_NV, 2, 0,
            GL_TEXTURE_COORD_NV, 2, 1,
            GL_POINT_SIZE, 1, 0
        };

        glGenBuffers(4, buffers);
        fprintf(ref, "trace\\.call: glGenBuffers\\(4, %p -> { %u, %u, %u, %u }\\)\n",
                buffers, buffers[0], buffers[1], buffers[2], buffers[3]);
        for (i = 0; i < 4; i++)
        {
            glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER_NV, buffers[i]);
            fprintf(ref, "trace\\.call: glBindBuffer\\(GL_TRANSFORM_FEEDBACK_BUFFER_NV, %u\\)\n",
                    buffers[i]);
            glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER_NV, 1024, NULL, GL_STREAM_COPY);
            fprintf(ref, "trace\\.call: glBufferData\\(GL_TRANSFORM_FEEDBACK_BUFFER_NV, 1024, NULL, GL_STREAM_COPY(_ARB)?\\)\n");
            glBindBufferBaseNV(GL_TRANSFORM_FEEDBACK_BUFFER_NV, i, i);
            fprintf(ref, "trace\\.call: glBindBufferBaseNV\\(GL_TRANSFORM_FEEDBACK_BUFFER_NV, %d, %d\\)\n",
                    i, i);
        }
        glGetIntegerv(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING_NV, &bound);
        fprintf(ref, "trace\\.call: glGetIntegerv\\(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING_NV, %p -> %d\\)\n",
                &bound, bound);
        glTransformFeedbackAttribsNV(4, attribs, GL_SEPARATE_ATTRIBS_NV);
        fprintf(ref, "trace\\.call: glTransformFeedbackAttribsNV\\(4, %p -> { "
                "{ GL_POSITION, 4, 0 }, "
                "{ GL_TEXTURE_COORD_NV, 2, 0 }, "
                "{ GL_TEXTURE_COORD_NV, 2, 1 }, "
                "{ GL_POINT_SIZE, 1, 0 } }, GL_SEPARATE_ATTRIBS_NV\\)\n",
                attribs);
        glGetIntegerIndexedvEXT(GL_TRANSFORM_FEEDBACK_RECORD_NV, 0, record);
        fprintf(ref, "trace\\.call: glGetIntegerIndexedvEXT\\(GL_TRANSFORM_FEEDBACK_RECORD_NV, 0, %p -> { GL_POSITION, 4, 0 }\\)\n",
                record);
    }
}

int main(int argc, char **argv)
{
    ref = fdopen(3, "w");
    if (!ref) ref = fopen("/dev/null", "w");
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowSize(300, 300);
    glutCreateWindow("query generator");

    glewInit();

    query_enums();
    query_bools();
    query_pointers();
    query_multi();
    query_tex_parameter();
    query_tex_level_parameter();
    query_tex_gen();
    query_tex_env();
    query_light();
    query_material();
    query_clip_plane();
    query_vertex_attrib();
    query_query();
    query_buffer_parameter();
    query_color_table();
    query_convolution();
    query_histogram();
    query_minmax();
    query_strings();
    query_shaders();
    query_ll_programs();
    query_framebuffers();
    query_renderbuffers();
    query_readpixels();
    query_transform_feedback();
    return 0;
}
