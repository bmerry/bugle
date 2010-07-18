/* Tries to use a number of extensions, to test the show-extensions filter-set */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <GL/glew.h>
#include <stdlib.h>
#include "test.h"

test_status showextensions_suite(void)
{
    GLint max;

    /* Need a minimum level of driver support to do a decent test */
    if (!GLEW_VERSION_1_2 || !GLEW_ARB_texture_cube_map)
    {
        return TEST_SKIPPED;
    }
    glDrawRangeElements(GL_TRIANGLES, 0, 0, 0, GL_UNSIGNED_SHORT, NULL); /* 1.2 */
    glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE_ARB, &max); /* cube_map/1.3 */

    test_log_printf(
            "\\[INFO\\] showextensions\\.gl: Min GL version: 1\\.2\n"
            "\\[INFO\\] showextensions\\.glx: Min GLX version: 1\\.\\d\n"
            "\\[INFO\\] showextensions\\.ext: Required extensions: GL_EXT_texture_cube_map GLX_ARB_get_proc_address\n");

    return TEST_RAN;
}
