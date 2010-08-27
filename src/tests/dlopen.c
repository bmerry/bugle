/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2007, 2009, 2010  Bruce Merry
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

/* Check that dlopen()ing libGL.so still passes through bugle */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#ifndef _GNU_SOURCE
# define _GNU_SOURCE /* To get RTLD_NEXT defined, if it is available */
#endif
#include <bugle/porting.h>

#if BUGLE_PLATFORM_POSIX
#include <dlfcn.h>
#include <GL/gl.h>
#include <GL/glx.h>

/* Some versions of Mesa 6.5 don't define PFNGLXGETPROCADDRESSARBPROC */
#ifdef GLX_VERSION_1_4
# undef PFNGLXGETPROCADDRESSARBPROC
# define PFNGLXGETPROCADDRESSARBPROC PFNGLXGETPROCADDRESSPROC
#endif
static PFNGLXGETPROCADDRESSARBPROC dl_glXGetProcAddressARB;
static void (*glx_glEnd)(void);
#endif /* BUGLE_PLATFORM_POSIX */

#include <stdio.h>
#include "test.h"


static void libGL_test(void)
{
#if defined(DEBUG_CONSTRUCTOR)
    test_skipped("DEBUG_CONSTRUCTOR defined");
#elif !BUGLE_PLATFORM_POSIX
    test_skipped("not POSIX");
#elif !defined(RTLD_NEXT)
    test_skipped("RTLD_NEXT not defined");
#elif !BUGLE_GLTYPE_GL
    test_skipped("not desktop GL");
#else
    void *handle;

    handle = dlopen("libGL.so", RTLD_LAZY | RTLD_GLOBAL);
    if (!handle)
    {
        test_failed("failed to open libGL.so: %s\n", dlerror());
        return;
    }
    dl_glXGetProcAddressARB = (PFNGLXGETPROCADDRESSARBPROC) dlsym(handle, "glXGetProcAddressARB");
    glx_glEnd = dl_glXGetProcAddressARB((const GLubyte *) "glEnd");
    test_log_printf("trace\\.call: glXGetProcAddressARB\\(\"glEnd\"\\) = %p\n",
                    glx_glEnd);
    dlclose(handle);
#endif
}

void dlopen_suite_register(void)
{
    test_suite *ts = test_suite_new("dlopen", TEST_FLAG_LOG, NULL, NULL);
    test_suite_add_test(ts, "libGL", libGL_test);
}
