#define _GNU_SOURCE
#include <stdio.h>
#include <dlfcn.h>
#include <GL/gl.h>
#include <GL/glx.h>

static PFNGLXGETPROCADDRESSARBPROC dl_glXGetProcAddressARB;
static void (*glx_glEnd)(void);

int main()
{
#ifdef RTLD_NEXT
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
    glx_glEnd = dl_glXGetProcAddressARB("glEnd");
    fprintf(ref, "trace\\.call: glXGetProcAddressARB\\(\"glEnd\"\\) = %p\n",
            glx_glEnd);
    dlclose(handle);
#endif
    return 0;
}
