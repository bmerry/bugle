/* Generates a variety of GL errors */
#ifdef NDEBUG /* make sure that assert is defined */
# undef NDEBUG
#endif

#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static void invalid_enum(void)
{
    glBegin(GL_MATRIX_MODE);
    if (glGetError() != GL_INVALID_ENUM)
    {
        fprintf(stderr,
                "glBegin(GL_MATRIX_MODE) failed to generate GL_INVALID_ENUM.\n"
                "Mesa 6.1 (distributed with xorg) doesn't, so this isn't considered a failure.\n"
                "If you're not using Mesa, then it may indicate a bug.\n");
    }
}

static void invalid_operation(void)
{
    glBegin(GL_TRIANGLES);
    glGetError();
    glEnd();
    assert(glGetError() == GL_INVALID_OPERATION);
}

static void invalid_value(void)
{
    /* border of 2 is illegal */
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 1, 1, 2, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    assert(glGetError() == GL_INVALID_VALUE);
}

static void stack_underflow(void)
{
    glPopAttrib();
    assert(glGetError() == GL_STACK_UNDERFLOW);
}

static void stack_overflow(void)
{
    GLint depth, i;

    glGetIntegerv(GL_MAX_ATTRIB_STACK_DEPTH, &depth);
    for (i = 0; i <= depth; i++)
        glPushAttrib(GL_ALL_ATTRIB_BITS);
    for (i = 0; i < depth; i++)
        glPopAttrib();
    assert(glGetError() == GL_STACK_OVERFLOW);
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
    return 0;
}
