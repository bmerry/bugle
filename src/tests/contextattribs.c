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

/* Tests the contextattribs filterset */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <GL/gl.h>
#include <GL/glext.h>
#include "test.h"
#include <bugle/bool.h>

#if BUGLE_GLWIN_GLX

#include <GL/glx.h>
#include <GL/glxext.h>
#include <X11/Xlib.h>
#include <string.h>

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
    GLXContext ctx;
    GLXDrawable draw;
} info_glXCreateNewContext;

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

static bugle_bool open_glXCreateNewContext(info_glXCreateNewContext *info)
{
    Display *dpy;
    GLXContext ctx;
    GLXDrawable draw;
    int nelements;
    GLXFBConfig config;
    GLXFBConfig *configs;
    Bool result;

    dpy = XOpenDisplay(NULL);

    const char * extensions = glXQueryExtensionsString(dpy, DefaultScreen(dpy));
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
        ctx = glXCreateNewContext(dpy, config, GLX_RGBA_TYPE, NULL, True);
        if (ctx == NULL)
        {
            test_failed("Context creation failed");
            XCloseDisplay(dpy);
            return BUGLE_FALSE;
        }

        draw = glXCreatePbuffer(dpy, config, pbuffer_attribs);

        result = glXMakeContextCurrent(dpy, draw, draw, ctx);
        if (!result)
        {
            test_failed("glXMakeCurrent failed");
            glXDestroyContext(dpy, ctx);
            glXDestroyPbuffer(dpy, draw);
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
    info->draw = draw;
    info->ctx = ctx;
    return BUGLE_TRUE;
}

static void close_glXCreateNewContext(const info_glXCreateNewContext *info)
{
    glXMakeContextCurrent(info->dpy, None, None, NULL);
    glXDestroyContext(info->dpy, info->ctx);
    glXDestroyPbuffer(info->dpy, info->draw);
    XCloseDisplay(info->dpy);
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

    if (!open_glXCreateContext(&info))
        return; /* callee set the test status */
    test_gl();
    close_glXCreateContext(&info);
}

static void contextattribs_glXCreateNewContext_test(void)
{
    info_glXCreateNewContext info;

    if (!open_glXCreateNewContext(&info))
        return; /* callee set the test status */
    test_gl();
    close_glXCreateNewContext(&info);
}

#endif /* BUGLE_GLWIN_GLX */

void contextattribs_suite_register(void)
{
    test_suite *ts = test_suite_new("contextattribs", TEST_FLAG_MANUAL, NULL, NULL);
    (void) ts; /* suppress unused variable warning when not under GLX */
#if BUGLE_GLWIN_GLX
    test_suite_add_test(ts, "glXCreateContext", contextattribs_glXCreateContext_test);
    test_suite_add_test(ts, "glXCreateNewContext", contextattribs_glXCreateNewContext_test);
#endif
}
