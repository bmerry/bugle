/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2011  Bruce Merry
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

/* Tests support of the GL_ARB_create_context extension */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <bugle/porting.h>
#include "test.h"

#if BUGLE_GLWIN_GLX

#include <GL/glx.h>
#include <GL/glxext.h>
#include <X11/Xlib.h>
#include <string.h>

static void glxarbcreatecontext_test(void)
{
    Display *dpy;
    dpy = XOpenDisplay(NULL);
#if GLX_ARB_create_context
    const char * extensions = glXQueryExtensionsString(dpy, DefaultScreen(dpy));
    if (strstr(extensions, "GLX_ARB_create_context"))
    {
        const int cfg_attribs[] =
        {
            GLX_RENDER_TYPE, GLX_RGBA_BIT,
            None
        };
        const int ctx_attribs[] =
        {
            GLX_CONTEXT_MAJOR_VERSION_ARB, 1,
            GLX_CONTEXT_MINOR_VERSION_ARB, 0,
            None
        };
        GLXContext ctx;
        GLXFBConfig cfg, *cfgs;
        PFNGLXCREATECONTEXTATTRIBSARBPROC create =
            (PFNGLXCREATECONTEXTATTRIBSARBPROC) glXGetProcAddressARB((const GLubyte *) "glXCreateContextAttribsARB");
        int elements;

        cfgs = glXChooseFBConfig(dpy, DefaultScreen(dpy), cfg_attribs, &elements);
        if (elements == 0)
        {
            test_skipped("No suitable FBConfig was found");
            return;
        }
        cfg = *cfgs;
        XFree(cfgs);
        ctx = create(dpy, cfg, NULL, True, ctx_attribs);
        test_log_printf("trace\\.call: glXCreateContextAttribsARB\\(%p, %p, NULL, True, %p -> { GLX_CONTEXT_MAJOR_VERSION(_ARB)?, 1, GL_CONTEXT_MINOR_VERSION(_ARB)?, 0, None }\\) = %p\n",
                        (void *) dpy, (void *) cfg, ctx_attribs, (void *) ctx);
        TEST_ASSERT(ctx != NULL);
        if (ctx != NULL)
        {
            glXDestroyContext(dpy, ctx);
            test_log_printf("trace\\.call: glXDestroyContext\\(%p, %p\\)\n",
                            (void *) dpy, (void *) cfg);
        }

    }
    else
#endif /* ARB_create_context */
    {
        test_skipped("GLX_ARB_create_context not found");
    }
    XCloseDisplay(dpy);
}

#endif /* BUGLE_GLWIN_GLX */

void arbcreatecontext_suite_register(void)
{
    test_suite *ts = test_suite_new("ARB_create_context", TEST_FLAG_LOG, NULL, NULL);
#if BUGLE_GLWIN_GLX
    test_suite_add_test(ts, "GLX_ARB_create_context", glxarbcreatecontext_test);
#endif
    /* TODO: test aux context creation */
}
