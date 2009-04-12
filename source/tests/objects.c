/* Creates a bunch of objects of various types, to test the
 * object tracker. This test is not automated.
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <GL/glew.h>
#include <GL/glut.h>
#include <GL/glext.h>
#include <stdlib.h>
#include <string.h>

static GLuint tex1d = 0, tex2d = 0, tex3d = 0, texcube = 0;
static GLuint query, buffer;
#ifdef GL_ARB_shader_objects
GLhandleARB shader, program;
#endif

static const GLubyte image[] =
{
    128, 129, 130,
    200, 201, 0,
    100, 200, 100,
    0, 200, 255,
    50, 100, 200,
    200, 100, 50,
    255, 255, 255,
    0, 0, 0
};

static void make_texture1d_object(void)
{
    glGenTextures(1, &tex1d);
    glBindTexture(GL_TEXTURE_1D, tex1d);
#ifdef GL_SGIS_generate_mipmap
    if (GLEW_SGIS_generate_mipmap)
        glTexParameteri(GL_TEXTURE_1D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
#endif
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB, 8, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
    glBindTexture(GL_TEXTURE_1D, 0);
}

static void make_texture2d_object(void)
{

    glGenTextures(1, &tex2d);
    glBindTexture(GL_TEXTURE_2D, tex2d);
#ifdef GL_SGIS_generate_mipmap
    if (GLEW_SGIS_generate_mipmap)
        glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
#endif
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2, 4, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
    glBindTexture(GL_TEXTURE_2D, 0);
}

static void make_texture3d_object(void)
{
#ifdef GL_EXT_texture3D
    if (GLEW_EXT_texture3D)
    {
        glGenTextures(1, &tex3d);
        glBindTexture(GL_TEXTURE_3D_EXT, tex3d);
#ifdef GL_SGIS_generate_mipmap
        if (GLEW_SGIS_generate_mipmap)
            glTexParameteri(GL_TEXTURE_3D_EXT, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
#endif
        glTexImage3DEXT(GL_TEXTURE_3D_EXT, 0, GL_RGB, 2, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
        glBindTexture(GL_TEXTURE_3D_EXT, 0);
    }
#endif
}

static void make_texture_cube_map_object(void)
{
#ifdef GL_ARB_texture_cube_map
    if (GLEW_ARB_texture_cube_map)
    {
        glGenTextures(1, &texcube);
        glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, texcube);
#ifdef GL_SGIS_generate_mipmap
        if (GLEW_SGIS_generate_mipmap)
            glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
#endif
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB, 0, GL_RGB, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X_ARB, 0, GL_RGB, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y_ARB, 0, GL_RGB, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_ARB, 0, GL_RGB, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z_ARB, 0, GL_RGB, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_ARB, 0, GL_RGB, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
        glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, 0);
    }
#endif
}

static void make_query_object(void)
{
#ifdef GL_ARB_occlusion_query
    if (GLEW_ARB_occlusion_query)
    {
        glGenQueriesARB(1, &query);
        glBeginQueryARB(GL_SAMPLES_PASSED_ARB, query);
        glEndQueryARB(GL_SAMPLES_PASSED_ARB);
    }
#endif
}

static void make_buffer_object(void)
{
#ifdef GL_ARB_vertex_buffer_object
    if (GLEW_ARB_vertex_buffer_object)
    {
        glGenBuffersARB(1, &buffer);
        glBindBufferARB(GL_ARRAY_BUFFER_ARB, buffer);
        glBufferDataARB(GL_ARRAY_BUFFER_ARB, sizeof(image), image, GL_STATIC_DRAW_ARB);
        glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
    }
#endif
}

static void make_shader_object(void)
{
#if defined(GL_ARB_shader_objects) && defined(GL_ARB_vertex_shader)
    const GLcharARB *source =
        "uniform vec4 c;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n"
        "    gl_FrontColor = c;\n"
        "}\n";
    GLint length;

    if (GLEW_ARB_shader_objects
        && GLEW_ARB_vertex_shader)
    {
        shader = glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
        length = strlen(source);
        glShaderSourceARB(shader, 1, &source, &length);
        glCompileShaderARB(shader);
    }
#endif
}

static void make_program_object(void)
{
#if defined(GL_ARB_shader_objects) && defined(GL_ARB_vertex_shader)
    if (GLEW_ARB_shader_objects
        && GLEW_ARB_vertex_shader)
    {
        GLfloat c[4] = {0.0, 0.5, 1.0, 0.5};

        program = glCreateProgramObjectARB();
        glAttachObjectARB(program, shader);
        glLinkProgramARB(program);
        glUseProgramObjectARB(program);
        glUniform4fvARB(glGetUniformLocationARB(program, "c"), 1, c);
    }
#endif
}

static void delete_texture1d_object(void)
{
    glDeleteTextures(1, &tex1d);
}

static void delete_texture2d_object(void)
{
    glDeleteTextures(1, &tex2d);
}

static void delete_texture3d_object(void)
{
    glDeleteTextures(1, &tex1d);
}

static void delete_texture_cube_map_object(void)
{
    glDeleteTextures(1, &texcube);
}

static void delete_query_object(void)
{
#ifdef GL_ARB_occlusion_query
    if (GLEW_ARB_occlusion_query)
        glDeleteQueriesARB(1, &query);
#endif
}

static void delete_buffer_object(void)
{
#ifdef GL_ARB_vertex_buffer_object
    if (GLEW_ARB_vertex_buffer_object)
        glDeleteBuffersARB(1, &buffer);
#endif
}

static void delete_shader_object(void)
{
#if defined(GL_ARB_shader_objects) && defined(GL_ARB_vertex_shader)
    if (GLEW_ARB_shader_objects
        && GLEW_ARB_vertex_shader)
        glDeleteObjectARB(shader);
#endif
}

static void delete_program_object(void)
{
#if defined(GL_ARB_shader_objects) && defined(GL_ARB_vertex_shader)
    if (GLEW_ARB_shader_objects
        && GLEW_ARB_vertex_shader)
    {
        glDeleteObjectARB(program);
        glUseProgramObjectARB(0);
    }
#endif
}

int main(int argc, char **argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowSize(300, 300);
    glutCreateWindow("object generator");

    glewInit();

    make_texture1d_object();
    make_texture2d_object();
    make_texture3d_object();
    make_texture_cube_map_object();
    make_query_object();
    make_buffer_object();
    make_shader_object();
    make_program_object();

    glutSwapBuffers();

    delete_texture1d_object();
    delete_texture2d_object();
    delete_texture3d_object();
    delete_texture_cube_map_object();
    delete_query_object();
    delete_buffer_object();
    delete_shader_object();
    delete_program_object();

    glutSwapBuffers();

    return 0;
}
