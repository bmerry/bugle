/* Tries to use a number of extensions, to test the show-extensions filter-set */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <GL/glew.h>
#include <stdlib.h>
#include "test.h"

static void showextensions_test(void)
{
    GLint max;

    /* Need a minimum level of driver support to do a decent test */
    if (!GLEW_VERSION_1_2 || !GLEW_ARB_texture_cube_map)
    {
        test_skipped("GL 1.2 and ARB_texture_cube_map required");
        return;
    }
    glDrawRangeElements(GL_TRIANGLES, 0, 0, 0, GL_UNSIGNED_SHORT, NULL); /* 1.2 */
    glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE_ARB, &max); /* cube_map/1.3 */

#if BUGLE_GLWIN_GLX
    test_log_printf(
            "\\[INFO\\] showextensions\\.gl: Min GL version: 1\\.2\n"
            "\\[INFO\\] showextensions\\.glx: Min GLX version: 1\\.\\d\n"
            "\\[INFO\\] showextensions\\.ext: Required extensions: GL_EXT_texture_cube_map GLX_ARB_get_proc_address\n");
#elif BUGLE_GLWIN_WGL
    test_log_printf(
            "\\[INFO\\] showextensions\\.gl: Min GL version: 1\\.2\n"
            "\\[INFO\\] showextensions\\.glx: Min GLX version: 1\\.\\d\n"
            "\\[INFO\\] showextensions\\.ext: Required extensions: GL_EXT_texture_cube_map WGL_ARB_extensions_string\n");
#endif
}

void showextensions_suite_register(void)
{
    test_suite *ts = test_suite_new("showextensions", TEST_FLAG_LOG | TEST_FLAG_CONTEXT, NULL, NULL);
    test_suite_add_test(ts, "showextensions", showextensions_test);
}
