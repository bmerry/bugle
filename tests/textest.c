/* Creates a number of textures, to test the texture viewer */

#include "glee/GLee.h"
#include <GL/glut.h>
#include <stdlib.h>

#ifndef LOG_TEXTURE_SIZE
# define LOG_TEXTURE_SIZE 6
#endif
#define TEXTURE_SIZE (1 << LOG_TEXTURE_SIZE)
#define LOG_TEXTURE_SIZE_3D (LOG_TEXTURE_SIZE * 2 / 3)
#define TEXTURE_SIZE_3D (1 << LOG_TEXTURE_SIZE_3D)

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
    GLubyte color[3];

    color[0] = rand() & 0xff;
    color[1] = rand() & 0xff;
    color[2] = rand() & 0xff;
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, color);

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
    GLubyte data[TEXTURE_SIZE][TEXTURE_SIZE][3];
    GLubyte cube[18] =
    {
        255, 0, 0,
        0, 255, 0,
        0, 0, 255,
        255, 255, 0,
        255, 0, 255,
        0, 255, 255
    };
    int i, j;

    for (i = 0; i < TEXTURE_SIZE; i++)
        for (j = 0; j < TEXTURE_SIZE; j++)
        {
            data[i][j][0] = i * 255 / (TEXTURE_SIZE - 1);
            data[i][j][1] = j * 255 / (TEXTURE_SIZE - 1);
            data[i][j][2] = 128;
        }

    glPixelStorei(GL_UNPACK_ROW_LENGTH, TEXTURE_SIZE);

    /* default texture; with non-generated mipmaps */
    for (i = 0; i <= LOG_TEXTURE_SIZE; i++)
        glTexImage2D(GL_TEXTURE_2D, i, GL_RGB, TEXTURE_SIZE >> i, TEXTURE_SIZE >> i, 0,
                     GL_RGB, GL_UNSIGNED_BYTE, data);

    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_1D, id);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB, TEXTURE_SIZE, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, TEXTURE_SIZE, TEXTURE_SIZE, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

#ifdef GL_NV_texture_rectangle
    if (GLEE_NV_texture_rectangle)
    {
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_RECTANGLE_NV, id);
        glTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, GL_RGB8, TEXTURE_SIZE, TEXTURE_SIZE, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    }
#endif

#ifdef GL_ARB_texture_cube_map
    if (GLEE_ARB_texture_cube_map)
    {
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, id);
        for (i = 0; i < 6; i++)
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, TEXTURE_SIZE, TEXTURE_SIZE, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        /* Second cube map tests that the view shows the right faces */
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, id);
        for (i = 0; i < 6; i++)
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, cube + 3 * i);
        glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    }
#endif

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#ifdef GL_EXT_texture3D
    if (GLEE_EXT_texture3D)
    {
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_3D_EXT, id);
        glTexImage3DEXT(GL_TEXTURE_3D_EXT, 0, GL_RGB,
                        TEXTURE_SIZE_3D, TEXTURE_SIZE_3D, TEXTURE_SIZE_3D,
                        0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glTexParameteri(GL_TEXTURE_3D_EXT, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_3D_EXT, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    }
#endif

#ifdef GL_EXT_texture_array
    if (GLEE_EXT_texture_array)
    {
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_1D_ARRAY_EXT, id);
        glTexImage2D(GL_TEXTURE_1D_ARRAY_EXT, 0, GL_RGB,
                     TEXTURE_SIZE, TEXTURE_SIZE,
                     0, GL_RGB, GL_UNSIGNED_BYTE, data);

        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D_ARRAY_EXT, id);
        glTexImage3DEXT(GL_TEXTURE_2D_ARRAY_EXT, 0, GL_RGB,
                        TEXTURE_SIZE_3D, TEXTURE_SIZE_3D, TEXTURE_SIZE_3D,
                        0, GL_RGB, GL_UNSIGNED_BYTE, data);
    }
#endif

    /* A 1-pixel texture that is updated every frame, to test that the
     * viewer updates properly.
     */
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, data);

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

    GLeeInit();
    init_gl();
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutIdleFunc(idle);
    glutMainLoop();
    return 0;
}
