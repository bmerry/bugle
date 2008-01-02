/* Tries to use a number of extensions, to test the show-extensions filter-set */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#define _POSIX_SOURCE
#include <glee/GLee.h>
#include <GL/glut.h>
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    FILE *ref;
    GLint max;

    ref = fdopen(3, "w");
    if (!ref) ref = fopen("/dev/null", "w");
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowSize(300, 300);
    glutCreateWindow("extension tester");
    GLeeInit();

    if (!GLEE_VERSION_1_2 || !GLEE_ARB_texture_cube_map)
    {
        fclose(ref);
        return 0;  /* Need a minimum level of driver support to do a decent test */
    }
    glBlendColor(1.0f, 1.0f, 1.0f, 1.0f); /* 1.2 */
    glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE_ARB, &max); /* cube_map/1.3 */

    fprintf(ref, 
            "\\[INFO\\] showextensions\\.gl: Min GL version: 1\\.2\n"
            "\\[INFO\\] showextensions\\.glx: Min GLX version: 1\\.\\d\n"
            "\\[INFO\\] showextensions\\.ext: Required extensions: GL_EXT_texture_cube_map GLX_ARB_get_proc_address\n");

    return 0;
}
