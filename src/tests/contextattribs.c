/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2011, 2013  Bruce Merry
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

/* Tests the contextattribs filterset */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <GL/gl.h>
#include <GL/glext.h>
#include "test.h"
#include <bugle/bool.h>
#include <bugle/porting.h>

#if BUGLE_GLWIN_GLX && BUGLE_PLATFORM_POSIX

#include <GL/glx.h>
#include <GL/glxext.h>
#include <X11/Xlib.h>
#include <string.h>
#include <budgie/addresses.h>
#include <budgie/call.h>
#include <dlfcn.h>

typedef struct
{
    Display *dpy;
    GLXContext ctx;
    Colormap colormap;
    Window window;
} info_glXCreateContext;

typedef struct
{
    Display *dpy;
    GLXDrawable draw;
    GLXFBConfig config;
} info_fbconfig;

typedef struct
{
    info_fbconfig fbconfig;
    GLXContext ctx;
} info_glXCreateNewContext;

typedef info_glXCreateNewContext info_glXCreateContextAttribsARB;

static bugle_bool open_glXCreateContext(info_glXCreateContext *info)
{
    Display *dpy;
    GLXContext ctx;
    Window window;
    XSetWindowAttributes x_attribs;
    Bool result;

    dpy = XOpenDisplay(NULL);

    const char * extensions = glXQueryExtensionsString(dpy, DefaultScreen(dpy));
    if (strstr(extensions, "GLX_ARB_create_context_profile"))
    {
        int visual_attribs[] =
        {
            GLX_RGBA,
            GLX_RED_SIZE, 8,
            GLX_GREEN_SIZE, 8,
            GLX_BLUE_SIZE, 8,
            None
        };
        XVisualInfo *vis = glXChooseVisual(dpy, DefaultScreen(dpy), visual_attribs);
        if (vis == NULL)
        {
            test_skipped("No suitable visual was found");
            XCloseDisplay(dpy);
            return BUGLE_FALSE;
        }
        ctx = glXCreateContext(dpy, vis, NULL, True);
        if (ctx == NULL)
        {
            test_failed("Context creation failed");
            XFree(vis);
            XCloseDisplay(dpy);
            return BUGLE_FALSE;
        }

        x_attribs.colormap = XCreateColormap(dpy, RootWindow(dpy, vis->screen), vis->visual, AllocNone);

        window = XCreateWindow(dpy,
                               RootWindow(dpy, vis->screen),
                               0, 0, 300, 300, 0,  /* x, y, w, h, border */
                               vis->depth,
                               InputOutput,
                               vis->visual,
                               CWColormap,         /* valuemask */
                               &x_attribs);        /* attributes */
        XFree(vis);
        XMapWindow(dpy, window);

        result = glXMakeCurrent(dpy, window, ctx);
        if (!result)
        {
            test_failed("glXMakeCurrent failed");
            glXDestroyContext(dpy, ctx);
            XDestroyWindow(dpy, window);
            XFreeColormap(dpy, x_attribs.colormap);
            XCloseDisplay(dpy);
            return BUGLE_FALSE;
        }
    }
    else
    {
        test_skipped("ARB_create_context_profile not found");
        return BUGLE_FALSE;
    }

    info->dpy = dpy;
    info->window = window;
    info->colormap = x_attribs.colormap;
    info->ctx = ctx;
    return BUGLE_TRUE;
}

static void close_glXCreateContext(const info_glXCreateContext *info)
{
    glXMakeCurrent(info->dpy, None, NULL);
    glXDestroyContext(info->dpy, info->ctx);
    XDestroyWindow(info->dpy, info->window);
    XFreeColormap(info->dpy, info->colormap);
    XCloseDisplay(info->dpy);
}

/* Configures the display, config and pbuffer, but not the context itself.
 */
static bugle_bool open_fbconfig(info_fbconfig *info)
{
    Display *dpy;
    GLXDrawable draw;
    int nelements;
    GLXFBConfig config;
    GLXFBConfig *configs;
    const char *extensions;

    dpy = XOpenDisplay(NULL);

    extensions = glXQueryExtensionsString(dpy, DefaultScreen(dpy));
    if (strstr(extensions, "GLX_ARB_create_context_profile"))
    {
        const int fbconfig_attribs[] =
        {
            GLX_RENDER_TYPE, GLX_RGBA_BIT,
            GLX_DRAWABLE_TYPE, GLX_PBUFFER_BIT,
            GLX_RED_SIZE, 8,
            GLX_GREEN_SIZE, 8,
            GLX_BLUE_SIZE, 8,
            None
        };
        const int pbuffer_attribs[] =
        {
            GLX_PBUFFER_WIDTH, 1,
            GLX_PBUFFER_HEIGHT, 1
        };

        configs = glXChooseFBConfig(dpy, DefaultScreen(dpy), fbconfig_attribs, &nelements);
        if (configs == NULL || nelements == 0)
        {
            test_skipped("No suitable FBConfig was found");
            if (configs != NULL)
                XFree(configs);
            XCloseDisplay(dpy);
            return BUGLE_FALSE;
        }
        config = configs[0];
        XFree(configs);

        draw = glXCreatePbuffer(dpy, config, pbuffer_attribs);
        if (draw == None)
        {
            test_failed("Pbuffer creation failed");
            XCloseDisplay(dpy);
            return BUGLE_FALSE;
        }
    }
    else
    {
        test_skipped("GLX_ARB_context_create_profile not present");
        XCloseDisplay(dpy);
        return BUGLE_FALSE;
    }

    info->dpy = dpy;
    info->draw = draw;
    info->config = config;
    return BUGLE_TRUE;
}

static void close_fbconfig(const info_fbconfig *info)
{
    glXDestroyPbuffer(info->dpy, info->draw);
    XCloseDisplay(info->dpy);
}

static bugle_bool open_glXCreateNewContext(info_glXCreateNewContext *info)
{
    Display *dpy;
    GLXContext ctx;
    Bool result;

    if (!open_fbconfig(&info->fbconfig))
        return BUGLE_FALSE;
    dpy = info->fbconfig.dpy;

    ctx = glXCreateNewContext(dpy, info->fbconfig.config, GLX_RGBA_TYPE, NULL, True);
    if (ctx == NULL)
    {
        test_failed("Context creation failed");
        close_fbconfig(&info->fbconfig);
        return BUGLE_FALSE;
    }

    result = glXMakeContextCurrent(dpy, info->fbconfig.draw, info->fbconfig.draw, ctx);
    if (!result)
    {
        test_failed("glXMakeCurrent failed");
        glXDestroyContext(dpy, ctx);
        close_fbconfig(&info->fbconfig);
        return BUGLE_FALSE;
    }

    info->ctx = ctx;
    return BUGLE_TRUE;
}

static void close_glXCreateNewContext(const info_glXCreateNewContext *info)
{
    glXMakeContextCurrent(info->fbconfig.dpy, None, None, NULL);
    glXDestroyContext(info->fbconfig.dpy, info->ctx);
    close_fbconfig(&info->fbconfig);
}

static bugle_bool open_glXCreateContextAttribsARB(info_glXCreateContextAttribsARB *info)
{
    Display *dpy;
    GLXContext ctx;
    Bool result;
    const int attribs[] =
    {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 2,
        GLX_CONTEXT_MINOR_VERSION_ARB, 1,
        GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
        None
    };
    PFNGLXCREATECONTEXTATTRIBSARBPROC glXCreateContextAttribsARB_ptr
        = (PFNGLXCREATECONTEXTATTRIBSARBPROC) glXGetProcAddress((const GLubyte *) "glXCreateContextAttribsARB");

    if (!open_fbconfig(&info->fbconfig))
        return BUGLE_FALSE;
    dpy = info->fbconfig.dpy;

    ctx = glXCreateContextAttribsARB_ptr(dpy, info->fbconfig.config, NULL, True, attribs);
    if (ctx == NULL)
    {
        test_failed("Context creation failed");
        close_fbconfig(&info->fbconfig);
        return BUGLE_FALSE;
    }

    result = glXMakeContextCurrent(dpy, info->fbconfig.draw, info->fbconfig.draw, ctx);
    if (!result)
    {
        test_failed("glXMakeCurrent failed");
        glXDestroyContext(dpy, ctx);
        close_fbconfig(&info->fbconfig);
        return BUGLE_FALSE;
    }

    info->ctx = ctx;
    return BUGLE_TRUE;
}

static void close_glXCreateContextAttribsARB(const info_glXCreateContextAttribsARB *info)
{
    close_glXCreateNewContext(info);
}

static int gl32_core_supported_handler(Display *dpy, XErrorEvent *e)
{
    /* Do nothing. We detect the error from the fact that the context
     * creation call returns NULL.
     */
    (void) dpy;
    (void) e;
    return 0;
}

/* Checks whether GL 3.2 core profile is supported. If not, marks the
 * test as skipped.
 */
static bugle_bool gl32_core_supported(void)
{
    PFNGLXCREATECONTEXTATTRIBSARBPROC glXCreateContextAttribsARB_real;
    BUDGIEAPIPROC (*budgie_function_address_real_ptr)(budgie_function);
    int (*old_handler)(Display *, XErrorEvent *);
    info_fbconfig info;
    GLXContext ctx;

    const int attribs[] =
    {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
        GLX_CONTEXT_MINOR_VERSION_ARB, 2,
        GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
        GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
        None
    };

    if (!open_fbconfig(&info))
        return BUGLE_FALSE;

    budgie_function_address_real_ptr = dlsym(NULL, "budgie_function_address_real");
    if (!budgie_function_address_real_ptr)
    {
        test_skipped("could not get address for budgie_function_address_real");
        close_fbconfig(&info);
        return BUGLE_FALSE;
    }
    glXCreateContextAttribsARB_real = (PFNGLXCREATECONTEXTATTRIBSARBPROC)
        budgie_function_address_real_ptr(BUDGIE_FUNCTION_ID(glXCreateContextAttribsARB));

    /* We use XSync to ensure that our custom handler handles only the
     * context creation call.
     */
    XSync(info.dpy, False);
    old_handler = XSetErrorHandler(gl32_core_supported_handler);

    /* Bypass the bugle wrapper */
    ctx = glXCreateContextAttribsARB_real(info.dpy, info.config, NULL, True, attribs);

    XSync(info.dpy, False);
    XSetErrorHandler(old_handler);
    if (ctx != NULL)
        glXDestroyContext(info.dpy, ctx);
    else
        test_skipped("GL 3.2 core profile is not supported");

    close_fbconfig(&info);
    return ctx != NULL;
}

static void test_gl(void)
{
    const GLubyte *version;
    GLint flags;
    GLint profile;

    version = glGetString(GL_VERSION);
    TEST_ASSERT(strcmp((const char *) version, "3.0") >= 0);

    glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
    TEST_ASSERT(flags & GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT);

    glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);
    TEST_ASSERT(profile == GL_CONTEXT_CORE_PROFILE_BIT);
}

static void contextattribs_glXCreateContext_test(void)
{
    info_glXCreateContext info;

    if (!gl32_core_supported())
        return; /* callee set the test status */
    if (!open_glXCreateContext(&info))
        return; /* callee set the test status */
    test_gl();
    close_glXCreateContext(&info);
}

static void contextattribs_glXCreateNewContext_test(void)
{
    info_glXCreateNewContext info;

    if (!gl32_core_supported())
        return; /* callee set the test status */
    if (!open_glXCreateNewContext(&info))
        return; /* callee set the test status */
    test_gl();
    close_glXCreateNewContext(&info);
}

static void contextattribs_glXCreateContextAttribsARB_test(void)
{
    info_glXCreateContextAttribsARB info;

    if (!gl32_core_supported())
        return; /* callee set the test status */
    if (!open_glXCreateContextAttribsARB(&info))
        return; /* callee set the test status */
    test_gl();
    close_glXCreateContextAttribsARB(&info);
}

#endif /* BUGLE_GLWIN_GLX && BUGLE_PLATFORM_POSIX */

void contextattribs_suite_register(void)
{
    test_suite *ts = test_suite_new("contextattribs", TEST_FLAG_MANUAL, NULL, NULL);
    (void) ts; /* suppress unused variable warning when not under GLX */
#if BUGLE_GLWIN_GLX && BUGLE_PLATFORM_POSIX
    test_suite_add_test(ts, "glXCreateContext", contextattribs_glXCreateContext_test);
    test_suite_add_test(ts, "glXCreateNewContext", contextattribs_glXCreateNewContext_test);
    test_suite_add_test(ts, "glXCreateContextAttribsARB", contextattribs_glXCreateContextAttribsARB_test);
#endif
}
