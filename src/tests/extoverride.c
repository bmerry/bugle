/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2010  Bruce Merry
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* Tests the extoverride filterset by checking for certain extensions */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <string.h>
#include "test.h"

static void version(void)
{
    const char *version = (const char *) glGetString(GL_VERSION);
    TEST_ASSERT(0 == strcmp(version, "1.1"));
}

static void extensions(void)
{
    const char *exts = (const char *) glGetString(GL_EXTENSIONS);
    TEST_ASSERT(0 == strcmp(exts, "GL_EXT_texture") || 0 == strcmp(exts, ""));
}

static void disabled_core(void)
{
    /* Bypass GLEW, since it will do its own checks for whether the extension
     * is enabled.
     */
    PFNGLTEXIMAGE3DPROC tex_image_3d = (PFNGLTEXIMAGE3DPROC) test_get_proc_address("glTexImage3D");

    if (tex_image_3d)
    {
        /* Try calling a disabled function, to provoke a warning */
        tex_image_3d(GL_TEXTURE_3D, 0, GL_RGBA, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        test_log_printf("\\[NOTICE\\] extoverride\\.warn: glTexImage3D was called, although the corresponding extension was suppressed\n");
    }
    else
        test_skipped("glTexImage3D not found");
}

static void disabled_extension(void)
{
    /* Bypass GLEW, since it will do its own checks for whether the extension
     * is enabled.
     */
    PFNGLTEXIMAGE3DPROC tex_image_3d_ext = (PFNGLTEXIMAGE3DPROC) test_get_proc_address("glTexImage3DEXT");

    if (tex_image_3d_ext)
    {
        /* Try calling a disabled function, to provoke a warning */
        tex_image_3d_ext(GL_TEXTURE_3D, 0, GL_RGBA, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        test_log_printf("\\[NOTICE\\] extoverride\\.warn: glTexImage3DEXT was called, although the corresponding extension was suppressed\n");
    }
    else
        test_skipped("glTexImage3DEXT not found");
}

void extoverride_suite_register(void)
{
    test_suite *ts = test_suite_new("extoverride", TEST_FLAG_CONTEXT | TEST_FLAG_LOG, NULL, NULL);
    test_suite_add_test(ts, "version", version);
    test_suite_add_test(ts, "extensions", extensions);
    test_suite_add_test(ts, "disabled_core", disabled_core);
    test_suite_add_test(ts, "disabled_extension", disabled_extension);
}
