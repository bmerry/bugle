/* Generates a variety of GL errors */

#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glut.h>
#include <stdlib.h>

static void invalid_enum(void)
{
    glBegin(GL_MATRIX_MODE);
}

static void invalid_operation(void)
{
    glBegin(GL_TRIANGLES);
    glGetError();
    glEnd();
}

static void invalid_value(void)
{
    /* border of 2 is illegal */
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 1, 1, 2, GL_RGB, GL_UNSIGNED_BYTE, NULL);
}

static void stack_underflow(void)
{
    glPopAttrib();
}

static void stack_overflow(void)
{
    GLint depth, i;

    glGetIntegerv(GL_MAX_ATTRIB_STACK_DEPTH, &depth);
    for (i = 0; i <= depth; i++)
        glPushAttrib(GL_ALL_ATTRIB_BITS);
    for (i = 0; i < depth; i++)
        glPopAttrib();
}

static void illegal_pointer(void)
{
    /* Pass a bogus pointer to cause a SIGSEGV */
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE,
                 (void *) (1 + (char *) NULL));
}

int main(int argc, char **argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowSize(300, 300);
    glutCreateWindow("error generator");
    invalid_enum();
    invalid_operation();
    invalid_value();
    stack_underflow();
    stack_overflow();
    illegal_pointer();
    return 0;
}
