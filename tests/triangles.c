/* Generates triangles using a variety of OpenGL functions, to test the
 * triangle counting code.
 */

#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glut.h>
#include <stdlib.h>

static GLfloat float_data[18] =
{
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f
};

static void triangles_immediate(GLenum mode)
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
}

static void triangles_draw_arrays(GLenum mode)
{
    glVertexPointer(GL_FLOAT, 3, 0, float_data);
    glDrawArrays(mode, 0, 6);
}

static void triangles_draw_elements(GLenum mode)
{
    GLushort indices[6] = {0, 1, 2, 3, 4, 5};

    glVertexPointer(GL_FLOAT, 3, 0, float_data);
    glDrawElements(mode, 6, GL_UNSIGNED_SHORT, indices);
}

static void triangles_draw_range_elements(GLenum mode)
{
    GLushort indices[6] = {0, 1, 2, 3, 4, 5};

    glVertexPointer(GL_FLOAT, 3, 0, float_data);
#ifdef GL_VERSION_1_2
    glDrawRangeElements(mode, 0, 5, 6, GL_UNSIGNED_SHORT, indices);
#endif
}

static void run(void (*func)(GLenum))
{
    func(GL_POINTS);
    glutSwapBuffers();
    func(GL_TRIANGLES);
    glutSwapBuffers();
    func(GL_QUADS);
    glutSwapBuffers();
    func(GL_TRIANGLE_STRIP);
    glutSwapBuffers();
    func(GL_POLYGON);
    glutSwapBuffers();
}

int main(int argc, char **argv)
{
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
