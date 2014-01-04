/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2012, 2014  Bruce Merry
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
#include <bugle/filters.h>
#include <bugle/glwin/trackcontext.h>
#include <bugle/glwin/glwin.h>
#include <bugle/gl/glutils.h>
#include <bugle/bool.h>
#include <bugle/log.h>
#include <bugle/memory.h>
#include <budgie/call.h>
#include <budgie/reflect.h>
#include <string.h>

static long force_major_version;
static long force_minor_version;
static int add_flags = 0, remove_flags = 0;
static int add_profiles = 0, remove_profiles = 0;

static bugle_bool have_major_version = BUGLE_FALSE;
static bugle_bool have_minor_version = BUGLE_FALSE;

static bugle_bool contextattribs_set_major_version(
    const filter_set_variable_info *var, const char *text, const void *value)
{
    have_major_version = BUGLE_TRUE;
    return BUGLE_TRUE;
}

static bugle_bool contextattribs_set_minor_version(
    const filter_set_variable_info *var, const char *text, const void *value)
{
    have_minor_version = BUGLE_TRUE;
    return BUGLE_TRUE;
}

static bugle_bool contextattribs_set_flag(
    const filter_set_variable_info *var, const char *text, const void *value)
{
    bugle_bool flip = BUGLE_FALSE;
    if (text[0] == '!')
    {
        text++;
        flip = BUGLE_TRUE;
    }

    int flag = 0;
    if (0 == strcmp(text, "forward"))
        flag = GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB;
    else if (0 == strcmp(text, "debug"))
        flag = GLX_CONTEXT_DEBUG_BIT_ARB;
    else if (0 == strcmp(text, "robust"))
        flag = GLX_CONTEXT_ROBUST_ACCESS_BIT_ARB;
    else
        return BUGLE_FALSE;

    if (!flip)
        add_flags |= flag;
    else
        remove_flags |= flag;
    return BUGLE_TRUE;
}

static bugle_bool contextattribs_set_profile(
    const filter_set_variable_info *var, const char *text, const void *value)
{
    bugle_bool flip = BUGLE_FALSE;
    if (text[0] == '!')
    {
        text++;
        flip = BUGLE_TRUE;
    }

    int mask = 0;
    if (0 == strcmp(text, "core"))
        mask = GLX_CONTEXT_CORE_PROFILE_BIT_ARB;
    else if (0 == strcmp(text, "compatibility"))
        mask = GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB;
    else if (0 == strcmp(text, "es2"))
        mask = GLX_CONTEXT_ES2_PROFILE_BIT_EXT;
    else
        return BUGLE_FALSE;

    if (!flip)
        add_profiles |= mask;
    else
        remove_profiles |= mask;
    return BUGLE_TRUE;
}

/* The strategy for all context creation calls is to
 * 1. Allow the original call to proceed, to decide whether it is sane.
 * 2. Create the replacement context.
 * 3. Destroy and replace the original one.
 * 4. Fake up a function_call structure corresponding to the replacement call, and
 *    use it with bugle_glwin_context_create_save and bugle_glwin_newcontext to
 *    ensure that trackcontext gets the right information.
 */

/* Check for the necessary extensions */
static bugle_bool contextattribs_support(Display *dpy, int screen)
{
    int glx_major, glx_minor;
    const char *glx_extensions;

    if (!CALL(glXQueryVersion)(dpy, &glx_major, &glx_minor))
    {
        bugle_log("contextattribs", "create", BUGLE_LOG_WARNING,
                  "Could not query GLX version, skipping");
        return BUGLE_FALSE;
    }
    if (glx_major < 1 || glx_minor < 3)
    {
        bugle_log("contextattribs", "create", BUGLE_LOG_WARNING,
                  "GLX 1.3 not present, skipping");
        return BUGLE_FALSE;
    }
    glx_extensions = CALL(glXQueryExtensionsString)(dpy, screen);
    if (!strstr(glx_extensions, "GLX_ARB_create_context"))
    {
        bugle_log("contextattribs", "create", BUGLE_LOG_WARNING,
                  "GLX_ARB_create_context extension not found, skipping");
        return BUGLE_FALSE;
    }
    return BUGLE_TRUE;
}

static int *contextattribs_make_attribs(const int *base_attribs)
{
    int *attribs;
    bugle_bool did_major_version = BUGLE_FALSE, did_minor_version = BUGLE_FALSE;
    bugle_bool did_flags = BUGLE_FALSE, did_profile_mask = BUGLE_FALSE;
    int nattribs = 0;
    int i;

    for (i = 0; base_attribs && base_attribs[i]; i += 2)
        nattribs++;

    /* The +9 is for space to add overrides, and the terminator */
    attribs = BUGLE_CALLOC(nattribs * 2 + 9, int);

    for (i = 0; base_attribs && base_attribs[i]; i += 2)
    {
        int key = base_attribs[i];
        int value = base_attribs[i + 1];

        if (key == GLX_CONTEXT_MAJOR_VERSION_ARB && have_major_version)
        {
            value = force_major_version;
            did_major_version = BUGLE_TRUE;
        }
        else if (key == GLX_CONTEXT_MINOR_VERSION_ARB && have_minor_version)
        {
            value = force_minor_version;
            did_minor_version = BUGLE_TRUE;
        }
        else if (key == GLX_CONTEXT_FLAGS_ARB && (add_flags != 0 || remove_flags != 0))
        {
            value |= add_flags;
            value &= ~remove_flags;
            did_flags = BUGLE_TRUE;
        }
        else if (key == GLX_CONTEXT_PROFILE_MASK_ARB
                 && (add_profiles != 0 || remove_profiles != 0))
        {
            value |= add_profiles;
            value &= ~remove_profiles;
            did_profile_mask = BUGLE_TRUE;
        }

        attribs[i] = key;
        attribs[i + 1] = value;
    }

    if (!did_major_version && have_major_version)
    {
        attribs[i++] = GLX_CONTEXT_MAJOR_VERSION_ARB;
        attribs[i++] = force_major_version;
    }
    if (!did_minor_version && have_minor_version)
    {
        attribs[i++] = GLX_CONTEXT_MINOR_VERSION_ARB;
        attribs[i++] = force_minor_version;
    }
    if (!did_flags && (add_flags != 0 || remove_flags != 0))
    {
        attribs[i++] = GLX_CONTEXT_FLAGS_ARB;
        attribs[i++] = add_flags & ~remove_flags;
    }
    if (!did_profile_mask && (add_profiles != 0 || remove_profiles != 0))
    {
        int mask = GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB; /* the default */
        mask |= add_profiles;
        mask &= ~remove_profiles;
        attribs[i++] = GLX_CONTEXT_PROFILE_MASK_ARB;
        attribs[i++] = mask;
    }
    attribs[i] = None;
    return attribs;
}

static void contextattribs_replace(
    function_call *call,
    Display *dpy, GLXFBConfig config, GLXContext share_context, Bool direct,
    const int *base_attribs)
{
    int *attribs;
    GLXContext new_ctx;
    function_call fake_call;
    glwin_context_create *create;

    attribs = contextattribs_make_attribs(base_attribs);
    new_ctx = CALL(glXCreateContextAttribsARB)(dpy, config, share_context, direct, attribs);
    if (new_ctx == NULL)
    {
        bugle_log("contextattribs", "create", BUGLE_LOG_WARNING,
                  "Replacement context creation call failed; falling back");
        bugle_free(attribs);
        return;
    }
    CALL(glXDestroyContext)(dpy, *(GLXContext *) call->generic.retn);
    *(GLXContext *) call->generic.retn = new_ctx;

    fake_call.generic.id = budgie_function_id("glXCreateContextAttribsARB");
    fake_call.generic.group = budgie_function_group(fake_call.generic.id);
    fake_call.generic.num_args = 5;
    fake_call.generic.args[0] = &dpy;
    fake_call.generic.args[1] = &config;
    fake_call.generic.args[2] = &share_context;
    fake_call.generic.args[3] = &direct;
    fake_call.generic.args[4] = &attribs;
    fake_call.generic.retn = &new_ctx;

    create = bugle_glwin_context_create_save(&fake_call);
    if (create != NULL)
        bugle_glwin_newcontext(create);

    bugle_free(attribs);
}

static bugle_bool contextattribs_glXCreateContext(function_call *call, const callback_data *data)
{
    Display *dpy = *call->glXCreateContext.arg0;
    XVisualInfo *vis = *call->glXCreateContext.arg1;
    GLXContext share_context = *call->glXCreateContext.arg2;
    Bool direct = *call->glXCreateContext.arg3;
    GLXFBConfig *configs;
    int nconfigs;
    int i;
    const int base_attribs[3] =
    {
        GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
        None
    };

    if (dpy == NULL || vis == NULL)
        return BUGLE_TRUE; /* It should have exploded on its own */

    if (*call->glXCreateContext.retn == NULL)
        return BUGLE_TRUE; /* It failed */

    if (!contextattribs_support(dpy, vis->screen))
        return BUGLE_TRUE;

    configs = CALL(glXGetFBConfigs)(dpy, vis->screen, &nconfigs);
    for (i = 0; i < nconfigs; i++)
    {
        int visual_id;
        CALL(glXGetFBConfigAttrib)(dpy, configs[i], GLX_VISUAL_ID, &visual_id);
        if (visual_id == vis->visualid)
            break;
    }
    if (i == nconfigs)
    {
        bugle_log("contextattribs", "create", BUGLE_LOG_WARNING,
                  "No matching FBConfig found for the Visual ID, skipping");
        return BUGLE_TRUE;
    }

    contextattribs_replace(call, dpy, configs[i], share_context, direct, base_attribs);
    return BUGLE_TRUE;
}

static bugle_bool contextattribs_glXCreateNewContext(function_call *call, const callback_data *data)
{
    Display *dpy = *call->glXCreateNewContext.arg0;
    GLXFBConfig config = *call->glXCreateNewContext.arg1;
    int render_type = *call->glXCreateNewContext.arg2;
    GLXContext share_context = *call->glXCreateNewContext.arg3;
    Bool direct = *call->glXCreateNewContext.arg4;
    const int base_attribs[3] =
    {
        GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
        None
    };

    if (dpy == NULL)
        return BUGLE_TRUE;  /* It should have exploded on its own */

    if (*call->glXCreateNewContext.retn == NULL)
        return BUGLE_TRUE;  /* It failed */

    /* TODO: should use a screen corresponding to the config, by
     * using ScreenCount() to determine the number of screens and
     * then glXGetFBConfigs on each until a match is found.
     */
    if (!contextattribs_support(dpy, DefaultScreen(dpy)))
        return BUGLE_TRUE;

    if (render_type != GLX_RGBA_TYPE)
    {
        bugle_log("contextattribs", "create", BUGLE_LOG_WARNING,
                  "Unsupported render_type in glXCreateNewContext");
        return BUGLE_TRUE;
    }

    contextattribs_replace(call, dpy, config, share_context, direct, base_attribs);
    return BUGLE_TRUE;
}

static bugle_bool contextattribs_glXCreateContextAttribsARB(function_call *call, const callback_data *data)
{
    Display *dpy = *call->glXCreateContextAttribsARB.arg0;
    GLXFBConfig config = *call->glXCreateContextAttribsARB.arg1;
    GLXContext share_context = *call->glXCreateContextAttribsARB.arg2;
    Bool direct = *call->glXCreateContextAttribsARB.arg3;
    const int *base_attribs = *call->glXCreateContextAttribsARB.arg4;

    if (*call->glXCreateContextAttribsARB.retn == NULL)
        return BUGLE_TRUE;

    contextattribs_replace(call, dpy, config, share_context, direct, base_attribs);
    return BUGLE_TRUE;
}

static bugle_bool contextattribs_initialise(filter_set *handle)
{
    filter *f;
    f = bugle_filter_new(handle, "contextattribs");
    bugle_filter_catches(f, "glXCreateContext", BUGLE_FALSE, contextattribs_glXCreateContext);
    bugle_filter_catches(f, "glXCreateNewContext", BUGLE_FALSE, contextattribs_glXCreateNewContext);
    bugle_filter_catches(f, "glXCreateContextAttribsARB", BUGLE_FALSE, contextattribs_glXCreateContextAttribsARB);
    bugle_filter_order("invoke", "contextattribs");
    bugle_filter_order("contextattribs", "trackcontext");
    bugle_gl_filter_post_renders("contextattribs");

    f = bugle_filter_new(handle, "contextattribs_free");
#if 0 /* TODO */
    bugle_filter_set_catches(f, "glXCreateContext", BUGLE_FALSE, contextattribs_free_attribs);
    bugle_filter_set_catches(f, "glXCreateNewContext", BUGLE_FALSE, contextattribs_free_attribs);
    bugle_filter_set_catches(f, "glXCreateContextAttribsARB", BUGLE_FALSE, contextattribs_free_attribs);
#endif
    /* It's important to only free the attribs after any other filter-set has examined them. */
    bugle_filter_order("contextattribs", "contextattribs_free");
    bugle_filter_order("trackcontext", "contextattribs_free");
    bugle_filter_order("debugger_error", "contextattribs_free");
    bugle_filter_order("trace", "contextattribs_free");
    bugle_filter_order("exe", "contextattribs_free");

    return BUGLE_TRUE;
}

void bugle_initialise_filter_library(void)
{
    static const filter_set_variable_info contextattribs_variables[] =
    {
        { "major", "GL major version number", FILTER_SET_VARIABLE_POSITIVE_INT, &force_major_version, contextattribs_set_major_version },
        { "minor", "GL minor version number", FILTER_SET_VARIABLE_UINT, &force_minor_version, contextattribs_set_minor_version },
        { "flag", "Context flag ([!]forward, [!]debug, or [!]robust)", FILTER_SET_VARIABLE_CUSTOM, NULL, contextattribs_set_flag },
        { "profile", "GL profile ([!]core, [!]compatibility, [!]es2)", FILTER_SET_VARIABLE_CUSTOM, NULL, contextattribs_set_profile },
        { NULL, NULL, 0, NULL, NULL }
    };

    static const filter_set_info contextattribs_info =
    {
        "contextattribs",
        contextattribs_initialise,
        NULL,
        NULL,
        NULL,
        contextattribs_variables,
        "Override context creation attributes"
    };

    bugle_filter_set_new(&contextattribs_info);
    bugle_filter_set_depends("contextattribs", "trackcontext");
    bugle_gl_filter_set_renders("contextattribs");
}
