#define _GNU_SOURCE
#include <stdio.h>
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

int main()
{
#if defined(RTLD_NEXT) && !defined(DEBUG_CONSTRUCTOR)
    void *handle;
    FILE *ref;

    ref = fdopen(3, "w");
    if (!ref) ref = fopen("/dev/null", "w");

    handle = dlopen("libGL.so", RTLD_LAZY | RTLD_GLOBAL);
    if (!handle)
    {
        fprintf(stderr, "failed to open libGL.so: %s\n", dlerror());
        return 1;
    }
    dl_glXGetProcAddressARB = (PFNGLXGETPROCADDRESSARBPROC) dlsym(handle, "glXGetProcAddressARB");
    glx_glEnd = dl_glXGetProcAddressARB((const GLubyte *) "glEnd");
    fprintf(ref, "trace\\.call: glXGetProcAddressARB\\(\"glEnd\"\\) = %p\n",
            glx_glEnd);
    dlclose(handle);
#endif
    return 0;
}
