/* Generates a variety of OpenGL queries, mainly for testing the logger */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#define _POSIX_SOURCE
#include "glee/GLee.h"
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glut.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>

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
 * - GetActiveUniforms
 * - GetUniformLocation
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
#ifdef GL_ARB_texture_compression
    if (GLEE_ARB_texture_compression)
    {
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED_ARB, &i);
        fprintf(ref, "trace\\.call: glGetTexLevelParameteriv\\(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED(_ARB)?, %p -> (GL_TRUE|GL_FALSE)\\)\n", (void *) &i);
    }
#endif
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

#ifdef GL_EXT_texture_lod_bias
    if (GLEE_EXT_texture_lod_bias)
    {
        glGetTexEnvfv(GL_TEXTURE_FILTER_CONTROL_EXT, GL_TEXTURE_LOD_BIAS_EXT, color);
        fprintf(ref, "trace\\.call: glGetTexEnvfv\\(GL_TEXTURE_FILTER_CONTROL(_EXT)?, GL_TEXTURE_LOD_BIAS(_EXT)?, %p -> 0\\)\n",
                (void *) color);
    }
#endif
#ifdef GL_ARB_point_sprite
    if (GLEE_ARB_point_sprite)
    {
        glGetTexEnviv(GL_POINT_SPRITE_ARB, GL_COORD_REPLACE_ARB, &mode);
        fprintf(ref, "trace\\.call: glGetTexEnviv\\(GL_POINT_SPRITE(_ARB)?, GL_COORD_REPLACE(_ARB)?, %p -> GL_FALSE\\)\n",
                (void *) &mode);
    }
#endif
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
#ifdef GL_ARB_vertex_program
    GLvoid *p;
    GLint i;
    GLdouble d[4];

    if (GLEE_ARB_vertex_program)
    {
        /* We use attribute 6, since ATI seems to use the same buffer for
         * attribute 0 and VertexPointer.
         */
        glGetVertexAttribPointervARB(6, GL_VERTEX_ATTRIB_ARRAY_POINTER_ARB, &p);
        fprintf(ref, "trace\\.call: glGetVertexAttribPointervARB\\(6, GL_VERTEX_ATTRIB_ARRAY_POINTER(_ARB)?, %p -> \\(nil\\)\\)\n", (void *) &p);
        glGetVertexAttribdvARB(6, GL_CURRENT_VERTEX_ATTRIB_ARB, d);
        fprintf(ref, "trace\\.call: glGetVertexAttribdvARB\\(6, (GL_CURRENT_VERTEX_ATTRIB(_ARB)?|GL_CURRENT_ATTRIB_NV), %p -> { 0, 0, 0, 1 }\\)\n", (void *) d);
        glGetVertexAttribivARB(6, GL_VERTEX_ATTRIB_ARRAY_TYPE_ARB, &i);
        fprintf(ref, "trace\\.call: glGetVertexAttribivARB\\(6, (GL_VERTEX_ATTRIB_ARRAY_TYPE(_ARB)?|GL_ATTRIB_ARRAY_TYPE_NV), %p -> GL_FLOAT\\)\n", (void *) &i);
    }
#endif
}

static void query_query(void)
{
#ifdef GL_ARB_occlusion_query
    GLint res;
    GLuint count;

    if (GLEE_ARB_occlusion_query)
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
#endif
}

static void query_buffer_parameter(void)
{
#ifdef GL_ARB_vertex_buffer_object
    GLubyte data[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    GLint res;
    GLvoid *ptr;

    if (GLEE_ARB_vertex_buffer_object)
    {
        glBindBufferARB(GL_ARRAY_BUFFER_ARB, 1);
        fprintf(ref, "trace\\.call: glBindBufferARB\\(GL_ARRAY_BUFFER(_ARB)?, 1\\)\n");
        glBufferDataARB(GL_ARRAY_BUFFER_ARB, 8, data, GL_STATIC_DRAW_ARB);
        fprintf(ref, "trace\\.call: glBufferDataARB\\(GL_ARRAY_BUFFER(_ARB)?, 8, %p, GL_STATIC_DRAW(_ARB)?\\)\n",
                (void *) data);
        glGetBufferParameterivARB(GL_ARRAY_BUFFER_ARB, GL_BUFFER_MAPPED_ARB, &res);
        fprintf(ref, "trace\\.call: glGetBufferParameterivARB\\(GL_ARRAY_BUFFER(_ARB)?, GL_BUFFER_MAPPED(_ARB)?, %p -> GL_FALSE\\)\n",
                (void *) &res);
        glGetBufferPointervARB(GL_ARRAY_BUFFER_ARB, GL_BUFFER_MAP_POINTER_ARB, &ptr);
        fprintf(ref, "trace\\.call: glGetBufferPointervARB\\(GL_ARRAY_BUFFER(_ARB)?, GL_BUFFER_MAP_POINTER(_ARB)?, %p -> \\(nil\\)\\)\n",
                (void *) &ptr);
    }
#endif
}

static void query_color_table(void)
{
#ifdef GL_ARB_imaging
    GLubyte data[12] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    GLfloat scale[4];
    GLint format;

    if (GLEE_ARB_imaging)
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
#endif
}

static void query_convolution(void)
{
#ifdef GL_ARB_imaging
    GLubyte data[3] = {0, 1, 2};
    GLfloat bias[4];
    GLint border_mode;

    if (GLEE_ARB_imaging)
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
#endif
}

static void query_histogram(void)
{
#ifdef GL_ARB_imaging
    GLint format, sink;

    if (GLEE_ARB_imaging)
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
#endif
}

static void query_minmax(void)
{
#ifdef GL_ARB_imaging
    GLint format, sink;

    if (GLEE_ARB_imaging)
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
#endif
}

static void query_shaders(void)
{
#if defined(GL_ARB_shader_objects) && defined(GL_ARB_vertex_shader)
    GLhandleARB v1, v2, p, attached[2];
    GLsizei count;

    if (GLEE_ARB_shader_objects
        && GLEE_ARB_vertex_shader)
    {
        v1 = glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
        fprintf(ref, "trace\\.call: glCreateShaderObjectARB\\(GL_VERTEX_SHADER(_ARB)?\\) = %u\n",
                (unsigned int) v1);
        v2 = glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
        fprintf(ref, "trace\\.call: glCreateShaderObjectARB\\(GL_VERTEX_SHADER(_ARB)?\\) = %u\n",
                (unsigned int) v2);
        p = glCreateProgramObjectARB();
        fprintf(ref, "trace\\.call: glCreateProgramObjectARB\\(\\) = %u\n",
                (unsigned int) p);

        glAttachObjectARB(p, v1);
        fprintf(ref, "trace\\.call: glAttachObjectARB\\(%u, %u\\)\n",
                (unsigned int) p, (unsigned int) v1);
        glAttachObjectARB(p, v2);
        fprintf(ref, "trace\\.call: glAttachObjectARB\\(%u, %u\\)\n",
                (unsigned int) p, (unsigned int) v2);
        glGetAttachedObjectsARB(p, 2, &count, attached);
        fprintf(ref, "trace\\.call: glGetAttachedObjectsARB\\(%u, 2, %p -> 2, %p -> { %u, %u }\\)\n",
                (unsigned int) p, (void *) &count, (void *) attached, (unsigned int) v1, (unsigned int) v2);
    }
#endif
    /* FIXME: test lots more things e.g. source */
}

static void query_ll_programs(void)
{
#if defined(GL_ARB_vertex_program)
    static const char *vp = "!!ARBvp1.0\nMOV result.position, vertex.position;\nEND\n";
    char *source;

    GLuint program;
    if (GLEE_ARB_vertex_program)
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
        glGetProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_STRING_ARB, source);
        fprintf(ref, "trace\\.call: glGetProgramStringARB\\(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_STRING_ARB, ");
        dump_string(ref, vp);
        fprintf(ref, "\\)\n");
        free(source);

        glDeleteProgramsARB(1, &program);
        fprintf(ref, "trace\\.call: glDeleteProgramsARB\\(1, %p -> { %u }\\)\n",
                (void *) &program, (unsigned int) program);
    }
#endif
}

#ifdef GL_EXT_framebuffer_object
/* Generates empty contents for a texture */
static void texture2D(GLenum target, const char *target_regex)
{
    glTexImage2D(target, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    fprintf(ref, "trace\\.call: glTexImage2D\\(%s, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, \\(nil\\)\\)\n", target_regex);
}

#ifdef GL_EXT_texture3D
/* Same but for 3D */
static void texture3D(GLenum target, const char *target_regex)
{
    glTexImage3DEXT(target, 0, GL_RGBA, 4, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    fprintf(ref, "trace\\.call: glTexImage3DEXT\\(%s, 0, GL_RGBA, 4, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, \\(nil\\)\\)\n", target_regex);
}
#endif /* GL_EXT_texture3D */
#endif /* GL_EXT_framebuffer_object */

static void query_framebuffers(void)
{
#ifdef GL_EXT_framebuffer_object
    GLuint fb, tex[3];
    GLint i;

    if (GLEE_EXT_framebuffer_object)
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
        fprintf(ref, "trace\\.call: glGetFramebufferAttachmentParameterivEXT\\(GL_FRAMEBUFFER(_EXT)?, GL_COLOR_ATTACHMENT0(_EXT)?, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE(_EXT), %p -> GL_TEXTURE\\)\n",
                (void *) &i);
        glGetFramebufferAttachmentParameterivEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                                                      GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME_EXT, &i);
        fprintf(ref, "trace\\.call: glGetFramebufferAttachmentParameterivEXT\\(GL_FRAMEBUFFER(_EXT)?, GL_COLOR_ATTACHMENT0(_EXT)?, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME(_EXT), %p -> %i\\)\n",
                (void *) &i, (int) tex[0]);
        glGetFramebufferAttachmentParameterivEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                                                      GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL_EXT, &i);
        fprintf(ref, "trace\\.call: glGetFramebufferAttachmentParameterivEXT\\(GL_FRAMEBUFFER(_EXT)?, GL_COLOR_ATTACHMENT0(_EXT)?, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL(_EXT), %p -> 1\\)\n",
                (void *) &i);

        /* 3D texture */
#ifdef GL_EXT_texture3D
        if (GLEE_EXT_texture3D)
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
            fprintf(ref, "trace\\.call: glGetFramebufferAttachmentParameterivEXT\\(GL_FRAMEBUFFER(_EXT)?, GL_COLOR_ATTACHMENT0(_EXT)?, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_3D_ZOFFSET(_EXT), %p -> 1\\)\n",
                    (void *) &i);
        }
#endif

        /* Cube map texture */
#ifdef GL_ARB_texture_cube_map
        if (GLEE_ARB_texture_cube_map)
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
            fprintf(ref, "trace\\.call: glGetFramebufferAttachmentParameterivEXT\\(GL_FRAMEBUFFER(_EXT)?, GL_COLOR_ATTACHMENT0(_EXT)?, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE(_EXT), %p -> GL_TEXTURE_CUBE_MAP_NEGATIVE_X(_ARB)?\\)\n",
                    (void *) &i);
        }
#endif

        glDeleteFramebuffersEXT(1, &fb);
        fprintf(ref, "trace\\.call: glDeleteFramebuffersEXT\\(1, %p -> { %u }\\)\n",
                (void *) &fb, (unsigned int) fb);
        glDeleteTextures(3, tex);
        fprintf(ref, "trace\\.call: glDeleteTextures\\(3, %p -> { %u, %u, %u }\\)\n",
                (void *) tex, (unsigned int) tex[0], (unsigned int) tex[1], (unsigned int) tex[2]);
    }
#endif /* GL_EXT_framebuffer_object */
}

static void query_renderbuffers(void)
{
#ifdef GL_EXT_framebuffer_object
    GLuint rb;
    GLint i;

    if (GLEE_EXT_framebuffer_object)
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
        /* FIXME: Disabled for now because NVIDIA 76.76 doesn't support it */
#if 0
        glGetRenderbufferParameterivEXT(GL_RENDERBUFFER_EXT, GL_RENDERBUFFER_RED_SIZE_EXT, &i);
        fprintf(ref, "trace\\.call: glGetRenderbufferParameterivEXT\\(GL_RENDERBUFFER(_EXT)?, GL_RENDERBUFFER_RED_SIZE(_EXT)?, %p -> %d\\)\n",
                (void *) &i, (int) i);
#endif
        glDeleteRenderbuffersEXT(1, &rb);
        fprintf(ref, "trace\\.call: glDeleteRenderbuffersEXT\\(1, %p -> { %u }\\)\n",
                (void *) &rb, (unsigned int) rb);
    }
#endif /* GL_EXT_framebuffer_object */
}

int main(int argc, char **argv)
{
    ref = fdopen(3, "w");
    if (!ref) ref = fopen("/dev/null", "w");
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowSize(300, 300);
    glutCreateWindow("query generator");

    GLeeInit();

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
    return 0;
}
