/* Does miscellaneous sanity checking */

#include <GL/glut.h>
#include <GL/glx.h>
#include <stdlib.h>

void check_procaddress()
{
    /* We deliberately raise an error, then check for it via the
     * address glXGetProcAddressARB gives us for glGetError. If we've
     * messed up glXGetProcAddressARB, we won't get the error because
     * the error interception will have eaten it.
     */
    GLenum (*GetError)(void);

    GetError = (GLenum (*)(void)) glXGetProcAddressARB((const GLubyte *) "glGetError");
    glPopAttrib();
    if ((*GetError)() != GL_STACK_UNDERFLOW) abort();
}

int main(int argc, char **argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowSize(300, 300);
    glutCreateWindow("misc checks");
    check_procaddress();
    return 0;
}
