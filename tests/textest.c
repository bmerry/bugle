/* Creates a number of textures, to test the texture viewer */

#include "tests/loader.h"
#include <GL/glut.h>

static void display(void)
{
    static const GLint vertices[8] =
    {
        -1, -1,
        1, -1,
        1, 1,
        -1, 1
    };
    static const GLint texcoords[8] =
    {
        0, 0,
        1, 0,
        1, 1,
        0, 1
    };

    glClearColor(0.0, 0.0, 1.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glVertexPointer(2, GL_INT, 0, vertices);
    glTexCoordPointer(2, GL_INT, 0, texcoords);
    glDrawArrays(GL_QUADS, 0, 4);

    glutSwapBuffers();
}

static void reshape(int w, int h)
{
    glViewport(0, 0, w, h);
}

static void idle(void)
{
    glutPostRedisplay();
}

static void init_gl()
{
    GLuint id;
    GLubyte data[16][16][3];
    int i, j;

    for (i = 0; i < 16; i++)
        for (j = 0; j < 16; j++)
        {
            data[i][j][0] = i * 16;
            data[i][j][1] = j * 16;
            data[i][j][2] = 128;
        }

    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_1D, id);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB, 16, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 16, 16, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

#ifdef GL_NV_texture_rectangle
    if (BUGLE_GL_NV_texture_rectangle)
    {
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_RECTANGLE_NV, id);
        glTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, GL_RGB8, 16, 16, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    }
#endif

#ifdef GL_ARB_texture_cube_map
    if (BUGLE_GL_ARB_texture_cube_map)
    {
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, id);
        for (i = 0; i < 6; i++)
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, 16, 16, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    }
#endif

#ifdef GL_EXT_texture3D
    if (BUGLE_GL_EXT_texture3D)
    {
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_3D_EXT, id);
        glTexImage3DEXT(GL_TEXTURE_3D_EXT, 0, GL_RGB, 8, 8, 4, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    }
#endif

    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glEnable(GL_TEXTURE_2D);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
}

int main(int argc, char **argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowSize(512, 512);
    glutCreateWindow("Texture test");

    bugle_init();
    init_gl();
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutIdleFunc(idle);
    glutMainLoop();
    return 0;
}
