/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2006  Bruce Merry
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
#include "src/filters.h"
#include "src/utils.h"
#include "src/tracker.h"
#include "src/objects.h"
#include "src/glexts.h"
#include "common/bool.h"
#include "common/hashtable.h"
#include "common/threads.h"
#include "common/safemem.h"
#include <assert.h>
#include <stddef.h>
#include <GL/glx.h>
#include <GL/gl.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

/* Some constructors like the context to be current when they are run.
 * To facilitate this, the object is not constructed until the first
 * use of the context. However, some of the useful information is passed
 * as parameters to glXCreate[New]Context, so they are put in
 * a trackcontext_data struct in the initial_values hash.
 */

bugle_object_class bugle_context_class;
bugle_object_class bugle_namespace_class;
static bugle_hashptr_table context_objects, namespace_objects;
static bugle_hashptr_table initial_values;
static bugle_object_view trackcontext_view;
static bugle_thread_mutex_t context_mutex = BUGLE_THREAD_MUTEX_INITIALIZER;

typedef struct
{
    GLuint texture;
    int char_width, char_height;
    int texture_width, texture_height;
} trackcontext_font;

typedef struct
{
    Display *dpy;
    GLXContext root_context;  /* context that owns the namespace - possibly self */
    GLXContext aux_shared;
    GLXContext aux_unshared;
    XVisualInfo visual_info;
    bool use_visual_info;

    trackcontext_font font;
} trackcontext_data;

static bool trackcontext_newcontext(function_call *call, const callback_data *data)
{
    Display *dpy;
    GLXContext self, parent;
    trackcontext_data *base, *up;

    switch (call->generic.group)
    {
#ifdef GLX_SGIX_fbconfig
    case GROUP_glXCreateContextWithConfigSGIX:
        dpy = *call->typed.glXCreateContextWithConfigSGIX.arg0;
        self = *call->typed.glXCreateContextWithConfigSGIX.retn;
        parent = *call->typed.glXCreateContextWithConfigSGIX.arg3;
        break;
#endif
    case GROUP_glXCreateContext:
        dpy = *call->typed.glXCreateContext.arg0;
        self = *call->typed.glXCreateContext.retn;
        parent = *call->typed.glXCreateContext.arg2;
        break;
    default:
        abort();
    }

    if (self)
    {
        bugle_thread_mutex_lock(&context_mutex);

        base = (trackcontext_data *) bugle_calloc(1, sizeof(trackcontext_data));
        base->dpy = dpy;
        base->aux_shared = NULL;
        base->aux_unshared = NULL;
        if (parent)
        {
            up = (trackcontext_data *) bugle_hashptr_get(&initial_values, parent);
            if (!up)
            {
                fprintf(stderr, "CRITICAL: share context %p unknown\n", (void *) parent);
                base->root_context = self;
            }
            else
                base->root_context = up->root_context;
        }
        switch (call->generic.group)
        {
#ifdef GLX_SGIX_fbconfig
        case GROUP_glXCreateContextWithConfigSGIX:
            base->use_visual_info = false;
            break;
#endif
        case GROUP_glXCreateContext:
            base->visual_info = **call->typed.glXCreateContext.arg1;
            base->use_visual_info = true;
            break;
        default:
            abort();
        }

        bugle_hashptr_set(&initial_values, self, base);
        bugle_thread_mutex_unlock(&context_mutex);
    }

    return true;
}

static bool trackcontext_callback(function_call *call, const callback_data *data)
{
    GLXContext ctx;
    bugle_object *obj, *ns;
    trackcontext_data *initial, *view;

    /* These calls may fail, so we must explicitly check for the
     * current context.
     */
    ctx = CALL_glXGetCurrentContext();
    if (!ctx)
        bugle_object_set_current(&bugle_context_class, NULL);
    else
    {
        bugle_thread_mutex_lock(&context_mutex);
        obj = bugle_hashptr_get(&context_objects, ctx);
        if (!obj)
        {
            obj = bugle_object_new(&bugle_context_class, ctx, true);
            bugle_hashptr_set(&context_objects, ctx, obj);
            initial = bugle_hashptr_get(&initial_values, ctx);
            if (initial == NULL)
            {
                fprintf(stderr, "CRITICAL: context %p used but not created\n",
                        (void *) ctx);
            }
            else
            {
                view = bugle_object_get_data(&bugle_context_class, obj, trackcontext_view);
                *view = *initial;
                ns = bugle_hashptr_get(&namespace_objects, view->root_context);
                if (!ns)
                {
                    ns = bugle_object_new(&bugle_namespace_class, ctx, true);
                    bugle_hashptr_set(&namespace_objects, ctx, ns);
                }
                else
                    bugle_object_set_current(&bugle_namespace_class, ns);
            }
        }
        else
            bugle_object_set_current(&bugle_context_class, obj);
        bugle_thread_mutex_unlock(&context_mutex);
    }
    return true;
}

static void destroy_trackcontext_data(void *data)
{
    trackcontext_data *d;

    d = (trackcontext_data *) data;
    if (d->aux_shared) glXDestroyContext(d->dpy, d->aux_shared);
    if (d->aux_unshared) glXDestroyContext(d->dpy, d->aux_unshared);
}

static bool initialise_trackcontext(filter_set *handle)
{
    filter *f;

    bugle_object_class_init(&bugle_context_class, NULL);
    bugle_object_class_init(&bugle_namespace_class, &bugle_context_class);
    bugle_hashptr_init(&context_objects, false);
    bugle_hashptr_init(&namespace_objects, false);
    bugle_hashptr_init(&initial_values, true);

    f = bugle_register_filter(handle, "trackcontext");
    bugle_register_filter_depends("trackcontext", "invoke");
    bugle_register_filter_catches(f, GROUP_glXMakeCurrent, true, trackcontext_callback);
    bugle_register_filter_catches(f, GROUP_glXCreateContext, true, trackcontext_newcontext);
#ifdef GLX_SGI_make_current_read
    bugle_register_filter_catches(f, GROUP_glXMakeCurrentReadSGI, true, trackcontext_callback);
#endif
#ifdef GLX_SGIX_fbconfig
    bugle_register_filter_catches(f, GROUP_glXCreateContextWithConfigSGIX, true, trackcontext_newcontext);
#endif
    trackcontext_view = bugle_object_class_register(&bugle_context_class,
                                                    NULL,
                                                    destroy_trackcontext_data,
                                                    sizeof(trackcontext_data));
    return true;
}

static void destroy_trackcontext(filter_set *handle)
{
    const bugle_hashptr_entry *i;

    for (i = bugle_hashptr_begin(&namespace_objects); i; i = bugle_hashptr_next(&namespace_objects, i))
        if (i->value)
            bugle_object_delete(&bugle_namespace_class, (bugle_object *) i->value);
    for (i = bugle_hashptr_begin(&context_objects); i; i = bugle_hashptr_next(&context_objects, i))
        if (i->value)
            bugle_object_delete(&bugle_context_class, (bugle_object *) i->value);
    bugle_hashptr_clear(&context_objects);
    bugle_hashptr_clear(&namespace_objects);
    bugle_hashptr_clear(&initial_values);
    bugle_object_class_clear(&bugle_namespace_class);
    bugle_object_class_clear(&bugle_context_class);
}

GLXContext bugle_get_aux_context(bool shared)
{
    trackcontext_data *data;
    GLXContext old_ctx, ctx;
    GLXContext *aux;
    int render_type = 0, screen;
    int n;
    int attribs[3] = {GLX_FBCONFIG_ID, 0, None};
    GLXFBConfig *cfgs;
    Display *dpy;
    int major = -1, minor = -1;

    data = bugle_object_get_current_data(&bugle_context_class, trackcontext_view);
    if (!data) return NULL; /* no current context, hence no aux context */
    aux = shared ? &data->aux_shared : &data->aux_unshared;
    if (*aux == NULL)
    {
        dpy = CALL_glXGetCurrentDisplay();
        old_ctx = CALL_glXGetCurrentContext();
        CALL_glXQueryVersion(dpy, &major, &minor);

#ifdef GLX_VERSION_1_3
        if (major >= 1 && (major > 1 || minor >= 3))
        {
            /* Have all the facilities to extract the necessary information */
            CALL_glXQueryContext(dpy, old_ctx, GLX_RENDER_TYPE, &render_type);
            CALL_glXQueryContext(dpy, old_ctx, GLX_SCREEN, &screen);
            CALL_glXQueryContext(dpy, old_ctx, GLX_FBCONFIG_ID, &attribs[1]);
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
            cfgs = CALL_glXChooseFBConfig(dpy, screen, attribs, &n);
            if (cfgs == NULL)
            {
                fprintf(stderr, "Warning: could not create an auxiliary context: no matching FBConfig\n");
                return NULL;
            }
            ctx = CALL_glXCreateNewContext(dpy, cfgs[0], render_type,
                                           shared ? old_ctx : NULL,
                                           CALL_glXIsDirect(dpy, old_ctx));
            XFree(cfgs);
            if (ctx == NULL)
                fprintf(stderr, "Warning: could not create an auxiliary context: creation failed\n");
        }
        else
#endif
        {
            if (data->use_visual_info)
            {
                ctx = CALL_glXCreateContext(dpy, &data->visual_info,
                                            shared ? old_ctx : NULL,
                                            CALL_glXIsDirect(dpy, old_ctx));
                if (ctx == NULL)
                    fprintf(stderr, "Warning: could not create an auxiliary context: creation failed\n");
            }
            else
            {
                fprintf(stderr, "Warning: could not create an auxiliary context: missing extensions\n");
                return NULL;
            }
        }
        *aux = ctx;
    }
    return *aux;
}

Display *bugle_get_current_display_internal(bool lock)
{
    trackcontext_data *data;
    GLXContext ctx;

    ctx = glXGetCurrentContext();
    if (!ctx)
        return NULL;
    if (lock) bugle_thread_mutex_lock(&context_mutex);
    data = (trackcontext_data *) bugle_hashptr_get(&initial_values, ctx);
    if (lock) bugle_thread_mutex_unlock(&context_mutex);
    if (data)
        return data->dpy;
#ifdef GLX_EXT_import_context
    else if (budgie_function_table[FUNC_glXGetCurrentDisplayEXT].real)
        return CALL_glXGetCurrentDisplayEXT();
#endif
#ifdef GLX_VERSION_1_3
    else if (budgie_function_table[FUNC_glXGetCurrentDisplay].real)
        return CALL_glXGetCurrentDisplay();
#endif
    return NULL;
}

Display *bugle_get_current_display(void)
{
    return bugle_get_current_display_internal(true);
}

GLXDrawable bugle_get_current_read_drawable(void)
{
#ifdef GLX_VERSION_1_3
    if (bugle_gl_has_extension(BUGLE_GLX_VERSION_1_3))
        return CALL_glXGetCurrentReadDrawable();
#endif
#ifdef GLX_SGI_make_current_read
    if (bugle_gl_has_extension(BUGLE_GLX_SGI_make_current_read))
        return CALL_glXGetCurrentReadDrawableSGI();
#endif
    return CALL_glXGetCurrentDrawable();
}

Bool bugle_make_context_current(Display *dpy, GLXDrawable draw,
                                GLXDrawable read, GLXContext ctx)
{
    /* FIXME: should depend on the capabilities of the target context */
#ifdef GLX_VERSION_1_3
    if (bugle_gl_has_extension(BUGLE_GLX_VERSION_1_3))
        return CALL_glXMakeContextCurrent(dpy, draw, read, ctx);
#endif
#ifdef GLX_SGI_make_current_read
    if (bugle_gl_has_extension(BUGLE_GLX_SGI_make_current_read))
        return CALL_glXMakeCurrentReadSGI(dpy, draw, read, ctx);
#endif
    return CALL_glXMakeCurrent(dpy, draw, ctx);
}


/* FIXME: corresponding shutdown code */
static bool trackcontext_font_init(trackcontext_font *font)
{
    Display *dpy;
    Pixmap pixmap;
    XImage *image;
    char **font_names;
    int font_count;
    XFontStruct *fs;
    XGCValues values;
    GC gc;
    int x, y;
    int i;
    GLubyte *texture_data;

    dpy = CALL_glXGetCurrentDisplay();
    if (!dpy) return NULL;

    /* Locate the font */
    font_names = XListFonts(dpy, "-*-courier-medium-r-normal--10-*", 1, &font_count);
    if (!font_count)
        return false;
    fs = XLoadQueryFont(dpy, font_names[0]);

    font->char_width = fs->max_bounds.rbearing - fs->min_bounds.lbearing;
    font->char_height = fs->max_bounds.ascent + fs->max_bounds.descent;
    /*
    font->char_x = -fs->min_bounds.lbearing;
    font->char_y = fs->descent - 1;
    */
    font->texture_width = 4;
    font->texture_height = 4;
    while (font->texture_width < 16 * font->char_width)
        font->texture_width *= 2;
    while (font->texture_height < 16 * font->char_height)
        font->texture_height *= 2;

    /* Allocate X structures */
    pixmap = XCreatePixmap(dpy, glXGetCurrentDrawable(),
                           font->texture_width, font->texture_height, 1);
    values.foreground = BlackPixel(dpy, DefaultScreen(dpy));
    values.foreground = WhitePixel(dpy, DefaultScreen(dpy));
    values.font = fs->fid;
    gc = XCreateGC(dpy, pixmap, GCForeground | GCBackground | GCFont, &values);
    /* Clear to black */
    XSetForeground(dpy, gc, 0);
    XFillRectangle(dpy, pixmap, gc, 0, 0, font->texture_width, font->texture_height);
    XSetForeground(dpy, gc, 1); /* white */
    for (i = 0; i < 256; i++)
    {
        int r, c;
        char ch;

        r = (i >> 4) & 0xf;
        c = i & 0xf;
        ch = i;
        XDrawString(dpy, pixmap, gc,
                    -fs->min_bounds.lbearing + font->char_width * c,
                    fs->max_bounds.ascent + font->texture_height - font->char_height * (r + 1),
                    &ch, 1);
    }

    image = XGetImage(dpy, pixmap, 0, 0, font->texture_width, font->texture_height, 1, XYPixmap);
    texture_data = (GLubyte *) bugle_calloc(font->texture_width * font->texture_height, sizeof(GLubyte));
    for (y = 0; y < font->texture_height; y++)
        for (x = 0; x < font->texture_width; x++)
            if (XGetPixel(image, x, y))
                texture_data[(font->texture_height - 1 - y) * font->texture_width + x] = 255;
    /* FIXME: use a proxy to check if texture size is supported */
    CALL_glGenTextures(1, &font->texture);
    CALL_glPushAttrib(GL_TEXTURE_BIT);
    CALL_glBindTexture(GL_TEXTURE_2D, font->texture);
    CALL_glTexImage2D(GL_TEXTURE_2D, 0, GL_INTENSITY4,
                      font->texture_width, font->texture_height, 0,
                      GL_LUMINANCE, GL_UNSIGNED_BYTE, texture_data);
    CALL_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    CALL_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    CALL_glPopAttrib();

    free(texture_data);
    XDestroyImage(image);
    XFreePixmap(dpy, pixmap);
    XFreeGC(dpy, gc);
    XUnloadFont(dpy, fs->fid);
    XFreeFontInfo(font_names, fs, 1);
    return true;
}

void bugle_text_render(const char *msg, int x, int y)
{
    trackcontext_data *data;
    int x0;

    data = bugle_object_get_current_data(&bugle_context_class, trackcontext_view);
    assert(data);

    if (!data->font.texture)
        trackcontext_font_init(&data->font);

    x0 = x;
    CALL_glPushAttrib(GL_CURRENT_BIT | GL_TEXTURE_BIT | GL_ENABLE_BIT);
    CALL_glBindTexture(GL_TEXTURE_2D, data->font.texture);
    CALL_glEnable(GL_TEXTURE_2D);
    CALL_glBegin(GL_QUADS);
    for (; *msg; msg++)
    {
        int r, c;
        GLfloat s1, s2, t1, t2;

        if (*msg == '\n')
        {
            x = x0;
            y -= data->font.char_height;
            continue;
        }
        r = (*msg >> 4) & 0xf;
        c = *msg & 0xf;
        s1 = data->font.char_width * c / (GLfloat) data->font.texture_width;
        s2 = data->font.char_width * (c + 1) / (GLfloat) data->font.texture_width;
        t1 = data->font.char_height * r / (GLfloat) data->font.texture_height;
        t2 = data->font.char_height * (r + 1) / (GLfloat) data->font.texture_height;
        CALL_glTexCoord2f(s1, t1);
        CALL_glVertex2i(x, y - data->font.char_height);
        CALL_glTexCoord2f(s2, t1);
        CALL_glVertex2i(x + data->font.char_width, y - data->font.char_height);
        CALL_glTexCoord2f(s2, t2);
        CALL_glVertex2i(x + data->font.char_width, y);
        CALL_glTexCoord2f(s1, t2);
        CALL_glVertex2i(x, y);
        x += data->font.char_width;
    }
    CALL_glEnd();
    CALL_glPopAttrib();
}

void trackcontext_initialise(void)
{
    static const filter_set_info trackcontext_info =
    {
        "trackcontext",
        initialise_trackcontext,
        destroy_trackcontext,
        NULL,
        NULL,
        NULL,
        0,
        NULL /* No documentation */
    };

    bugle_register_filter_set(&trackcontext_info);
}
