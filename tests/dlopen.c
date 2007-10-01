#include <stdio.h>
#include <dlfcn.h>
#include <GL/gl.h>
#include <GL/glx.h>

static PFNGLXGETPROCADDRESSARBPROC dl_glXGetProcAddressARB;
static PFNGLXCREATEWINDOWPROC glx_glXCreateWindow;

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
    glx_glXCreateWindow = dl_glXGetProcAddressARB("glXCreateWindow");
    fprintf(ref, "trace\\.call: glXGetProcAddressARB\\(\"glXCreateWindow\"\\) = %p\n",
            glx_glXCreateWindow);
    dlclose(handle);
#endif
    return 0;
}
