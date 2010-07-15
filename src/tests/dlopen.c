/* Check that dlopen()ing libGL.so still passes through bugle */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#ifndef _GNU_SOURCE
# define _GNU_SOURCE /* To get RTLD_NEXT defined, if it is available */
#endif
#include <stdio.h>
#include <dlfcn.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include "test.h"

/* Some versions of Mesa 6.5 don't define PFNGLXGETPROCADDRESSARBPROC */
#ifdef GLX_VERSION_1_4
# undef PFNGLXGETPROCADDRESSARBPROC
# define PFNGLXGETPROCADDRESSARBPROC PFNGLXGETPROCADDRESSPROC
#endif
static PFNGLXGETPROCADDRESSARBPROC dl_glXGetProcAddressARB;
static void (*glx_glEnd)(void);

test_status dlopen_suite(void)
{
#if defined(RTLD_NEXT) && !defined(DEBUG_CONSTRUCTOR)
    void *handle;

    handle = dlopen("libGL.so", RTLD_LAZY | RTLD_GLOBAL);
    if (!handle)
    {
        fprintf(stderr, "failed to open libGL.so: %s\n", dlerror());
        return TEST_FAILED;
    }
    dl_glXGetProcAddressARB = (PFNGLXGETPROCADDRESSARBPROC) dlsym(handle, "glXGetProcAddressARB");
    glx_glEnd = dl_glXGetProcAddressARB((const GLubyte *) "glEnd");
    test_log_printf("trace\\.call: glXGetProcAddressARB\\(\"glEnd\"\\) = %p\n",
                    glx_glEnd);
    dlclose(handle);
    return TEST_RAN;
#else
    return TEST_SKIPPED;
#endif
}
