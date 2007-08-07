/* This tests that the linker setup was done correctly, in the sense that
 * glXGetProcAddressARB should return the wrapper, rather than anything
 * interposed over the top by the application.
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <GL/glut.h>
#include <GL/glx.h>
#include <stdlib.h>
#include <stdio.h>

void glGetIntegerv(GLenum pname, GLint *out)
{
    fprintf(stderr,
            "bugle will not work if the application defines symbols with the\n"
            "same names as OpenGL functions. If you are using GCC and an ELF\n"
            "linker, this indicates that GCC was not linked correctly.\n");
    exit(1);
}

void (*pglGetIntegerv)(GLenum, GLint *);

int main(int argc, char **argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowSize(300, 300);
    glutCreateWindow("linker test");

    GLint dummy;
    pglGetIntegerv = (void (*)(GLenum, GLint *)) glXGetProcAddressARB((GLubyte *) "glGetIntegerv");
    pglGetIntegerv(GL_MATRIX_MODE, &dummy);
    return 0;
}
