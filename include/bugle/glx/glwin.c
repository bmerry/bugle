/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2008  Bruce Merry
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

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <bugle/glwintypes.h>
#include <bugle/glwin.h>
#include <X11/xlib.h>
#include <stdbool.h>

/* Wrappers around GLX/WGL/EGL functions */
bugle_glwin_display bugle_get_current_display(void)
{
    return CALL(glXGetCurrentDisplay)();
}

bugle_glwin_context bugle_get_current_context(void)
{
    return CALL(glXGetCurrentContext)();
}

bugle_glwin_drawable bugle_get_current_drawable(void)
{
    return CALL(glXGetCurrentDrawable)();
}

bugle_glwin_drawable bugle_get_current_read_drawable(void)
{
    return CALL(glXGetCurrentReadDrawableSGI)();
}

bool bugle_make_context_current(bugle_glwin_display dpy, bugle_glwin_drawable draw,
                                bugle_glwin_drawable read, bugle_glwin_context ctx)
{
    return CALL(glXMakeCurrentReadSGIX)(dpy, draw, read, ctx);
}

/* Creates a new context with the same visual/pixel format/config as the
 * given one. If 'share' is true, it will share texture data etc, otherwise
 * it will not. Returns NULL on failure.
 *
 * This functionality is intended primarily for trackcontext to generate
 * aux contexts. Most user code should use those aux contexts rather than
 * generating lots of their own.
 */
bugle_glwin_context bugle_clone_current_context(bugle_glwin_display dpy,
                                                bugle_glwin_context ctx,
                                                bool share)
{
    int attribs[3] = {GLX_FBCONFIG_ID, 0, None};
    int screen, render_type;
    GLXFBConfigSGIX *configs = NULL;
    GLXContext new_ctx = NULL;

    CALL(glXQueryContextInfoEXT)(dpy, ctx, GLX_RENDER_TYPE_SGIX, &render_type);
    CALL(glXQueryContextInfoEXT)(dpy, ctx, GLX_SCREEN_EXT, &screen);
    CALL(glXQueryContextInfoEXT)(dpy, ctx, GLX_FBCONFIG_ID_SGIX, &attribs[1]);
    /* I haven't quite figured out the return values, but I'm guessing
     * that it is returning one of the GLX_RGBA_BIT values rather than
     * GL_RGBA_TYPE etc.
     */
    if (render_type == GLX_RGBA_BIT)
        render_type = GLX_RGBA_TYPE;
    else if (render_type == GLX_COLOR_INDEX_BIT)
        render_type = GLX_COLOR_INDEX_TYPE;
#ifdef GLX_ARB_fbconfig_float
    else if (render_type == GLX_RGBA_FLOAT_BIT_ARB)
        render_type = GLX_RGBA_FLOAT_TYPE_ARB;
#endif
    cfgs = CALL(glXChooseFBConfig)(dpy, screen, attribs, &n);
    if (cfgs == NULL || n == 0)
    {
        bugle_log("glx", "clone", BUGLE_LOG_WARNING,
                  "could not create an auxiliary context: no matching FBConfig");
        return NULL;
    }
    new_ctx = CALL(glXCreateContextWithConfigSGIX)(dpy, cfgs[0], render_type,
                                               shared ? ctx : NULL,
                                               CALL(glXIsDirect)(dpy, ctx));
    XFree(cfgs);
    if (new_ctx == NULL)
        bugle_log("glx", "clone", BUGLE_LOG_WARNING,
                  "could not create an auxiliary context: creation failed");
    return ctx;
}
