/* Generates triangles using a variety of OpenGL functions, to test the
 * triangle counting code.
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#define GL_GLEXT_PROTOTYPES
#define _POSIX_SOURCE
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glx.h>
#include <GL/glut.h>
#include <stdlib.h>
#include <stdio.h>

static FILE *ref;

static GLfloat float_data[18] =
{
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f
};

static void triangles_immediate(GLenum mode, int count)
{
    GLfloat f[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    GLdouble d[4] = {0.0, 0.0, 0.0, 0.0};
    GLshort s[4] = {0, 0, 0, 0};

    glBegin(mode);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex2d(0.0, 0.0);
    glVertex4s(0, 0, 0, 0);

    glVertex2fv(f);
    glVertex3sv(s);
    glVertex4dv(d);
    glEnd();
    fprintf(ref, "stats\\.fps: [0-9.]+\nstats\\.triangles: %d\n", count);
}

static void triangles_draw_arrays(GLenum mode, int count)
{
    glVertexPointer(GL_FLOAT, 3, 0, float_data);
    glDrawArrays(mode, 0, 6);
    fprintf(ref, "stats\\.fps: [0-9.]+\nstats\\.triangles: %d\n", count);
}

static void triangles_draw_elements(GLenum mode, int count)
{
    GLushort indices[6] = {0, 1, 2, 3, 4, 5};

    glVertexPointer(GL_FLOAT, 3, 0, float_data);
    glDrawElements(mode, 6, GL_UNSIGNED_SHORT, indices);
    fprintf(ref, "stats\\.fps: [0-9.]+\nstats\\.triangles: %d\n", count);
}

static void triangles_draw_range_elements(GLenum mode, int count)
{
    GLushort indices[6] = {0, 1, 2, 3, 4, 5};

    glVertexPointer(GL_FLOAT, 3, 0, float_data);
#ifdef GL_EXT_draw_range_elements
    if (glutExtensionSupported("GL_EXT_draw_range_elements"))
    {
        glDrawRangeElementsEXT(mode, 0, 5, 6, GL_UNSIGNED_SHORT, indices);
        fprintf(ref, "stats\\.fps: [0-9.]+\nstats\\.triangles: %d\n", count);
    }
#endif
}

static void run(void (*func)(GLenum, int))
{
    func(GL_POINTS, 0);
    glutSwapBuffers();
    func(GL_TRIANGLES, 2);
    glutSwapBuffers();
    func(GL_QUADS, 2);
    glutSwapBuffers();
    func(GL_TRIANGLE_STRIP, 4);
    glutSwapBuffers();
    func(GL_POLYGON, 4);
    glutSwapBuffers();
}

int main(int argc, char **argv)
{
    ref = fdopen(3, "w");
    if (!ref) ref = fopen("/dev/null", "w");
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowSize(300, 300);
    glutCreateWindow("triangle count test");
    run(triangles_immediate);
    run(triangles_draw_arrays);
    run(triangles_draw_elements);
    run(triangles_draw_range_elements);
    return 0;
}
