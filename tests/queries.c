/* Generates a variety of OpenGL queries, mainly for testing the logger */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#define _POSIX_SOURCE
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glut.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

/* Still TODO (how depressing)
 * - GetColorTable
 * - GetConvolutionParameter
 * - GetConvolutionFilter
 * - GetSeparableFilter
 * - GetHistogram
 * - GetHistogramParameter
 * - GetMinmax
 * - GetMinmaxParameter
 * - GetPointer
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

static int extension_supported(const char *str)
{
    const char *ext;
    size_t len;

    ext = glGetString(GL_EXTENSIONS);
    fprintf(ref, "trace\\.call: glGetString\\(GL_EXTENSIONS\\) = \"[A-Za-z0-9_ ]+\"\n");
    len = strlen(ext);
    while ((ext = strstr(ext, str)) != NULL)
        if (ext[len] == ' ' || ext[len] == '\0') return 1;
        else ext++;
    return 0;
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
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &i);
    fprintf(ref, "trace\\.call: glGetTexLevelParameteriv\\(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, %p -> (1|2|3|4|GL_[A-Z0-9_]+)\\)\n", (void *) &i);
#ifdef GL_ARB_texture_compression
    if (extension_supported("GL_ARB_texture_compression"))
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
    if (extension_supported("GL_EXT_texture_lod_bias"))
    {
        glGetTexEnvfv(GL_TEXTURE_FILTER_CONTROL_EXT, GL_TEXTURE_LOD_BIAS_EXT, color);
        fprintf(ref, "trace\\.call: glGetTexEnvfv\\(GL_TEXTURE_FILTER_CONTROL(_EXT)?, GL_TEXTURE_LOD_BIAS(_EXT)?, %p -> 0\\)\n",
                (void *) color);
    }
#endif
#ifdef GL_ARB_point_sprite
    if (extension_supported("GL_ARB_point_sprite"))
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

    if (extension_supported("GL_ARB_vertex_program"))
    {
        glGetVertexAttribPointervARB(0, GL_VERTEX_ATTRIB_ARRAY_POINTER_ARB, &p);
        fprintf(ref, "trace\\.call: glGetVertexAttribPointervARB\\(0, GL_VERTEX_ATTRIB_ARRAY_POINTER(_ARB)?, %p -> \\(nil\\)\\)\n", (void *) &p);
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

    if (extension_supported("GL_ARB_occlusion_query"))
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

    if (extension_supported("GL_ARB_vertex_buffer_object"))
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

    if (extension_supported("GL_ARB_imaging"))
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
    }
#endif
}

int main(int argc, char **argv)
{
    ref = fdopen(3, "w");
    if (!ref) ref = fopen("/dev/null", "w");
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowSize(300, 300);
    glutCreateWindow("query generator");

    query_enums();
    query_bools();
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
    query_strings();
    return 0;
}
