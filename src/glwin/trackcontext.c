/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2008  Bruce Merry
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
#include <bugle/bool.h>
#include <bugle/hashtable.h>
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <bugle/glwin/glwin.h>
#include <bugle/glwin/trackcontext.h>
#include <bugle/filters.h>
#include <bugle/objects.h>
#include <bugle/memory.h>
#include <bugle/log.h>
#include <budgie/types.h>
#include <budgie/addresses.h>
#include "budgielib/defines.h"
#include "platform/threads.h"

/* Some constructors like the context to be current when they are run.
 * To facilitate this, the object is not constructed until the first
 * use of the context. However, some of the useful information is passed
 * as parameters to glXCreate[New]Context, so they are put in
 * a trackcontext_data struct in the initial_values hash.
 */

object_class *bugle_context_class;
object_class *bugle_namespace_class;
static hashptr_table context_objects, namespace_objects;
static hashptr_table initial_values;
static object_view trackcontext_view;
bugle_thread_lock_define_initialized(static, context_mutex)

typedef struct
{
    glwin_context root_context;  /* context that owns the namespace - possibly self */
    glwin_context aux_shared;
    glwin_context aux_unshared;
    glwin_context_create *create;

    GLuint font_texture;
} trackcontext_data;

/* FIXME-GLES */
#if BUGLE_GLTYPE_GL
/* Extracted from Unifont - see LICENSE for details. Each byte is one 8-pixel
 * row of a character, highest bit left-most. The characters are packed into
 * a 128x128 texture, 16 per row and 8 per column, with the ASCII values
 * running top-down (i.e. from the END of this table) and left-right. The
 * control characters and space are not explicitly listed, so default to
 * all zeros (blank).
 */
static const GLubyte font_bitmap[128 * 16] =
{
    64, 2, 0, 0, 0, 0, 0, 0, 0, 60, 0, 0, 8, 0, 0, 208,
    64, 2, 0, 0, 0, 0, 0, 0, 0, 2, 0, 12, 8, 48, 0, 75,
    92, 58, 64, 60, 12, 58, 24, 54, 66, 2, 126, 16, 8, 8, 0, 16,
    98, 70, 64, 66, 16, 70, 24, 73, 66, 26, 64, 16, 8, 8, 0, 74,
    66, 66, 64, 2, 16, 66, 36, 73, 36, 38, 32, 8, 8, 16, 0, 208,
    66, 66, 64, 12, 16, 66, 36, 73, 24, 66, 16, 8, 8, 16, 0, 115,
    66, 66, 64, 48, 16, 66, 36, 73, 24, 66, 8, 16, 8, 8, 0, 0,
    66, 66, 66, 64, 16, 66, 66, 73, 36, 66, 4, 16, 8, 8, 0, 0,
    98, 70, 98, 66, 16, 66, 66, 73, 66, 66, 2, 8, 8, 16, 0, 0,
    92, 58, 92, 60, 124, 66, 66, 65, 66, 66, 126, 8, 8, 16, 0, 0,
    0, 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 16, 8, 8, 70, 0,
    0, 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 16, 8, 8, 73, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 12, 8, 48, 49, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 60, 0, 0, 48, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 66, 0, 0, 72, 0, 0, 0, 0, 0,
    0, 58, 92, 60, 58, 60, 16, 66, 66, 62, 4, 66, 62, 73, 66, 60,
    0, 70, 98, 66, 70, 66, 16, 60, 66, 8, 4, 68, 8, 73, 66, 66,
    0, 66, 66, 64, 66, 64, 16, 32, 66, 8, 4, 72, 8, 73, 66, 66,
    0, 66, 66, 64, 66, 64, 16, 56, 66, 8, 4, 80, 8, 73, 66, 66,
    0, 62, 66, 64, 66, 126, 16, 68, 66, 8, 4, 96, 8, 73, 66, 66,
    0, 2, 66, 64, 66, 66, 16, 68, 66, 8, 4, 80, 8, 73, 66, 66,
    0, 66, 98, 66, 70, 66, 124, 68, 98, 8, 4, 72, 8, 73, 98, 66,
    0, 60, 92, 60, 58, 60, 16, 58, 92, 24, 12, 68, 8, 118, 92, 60,
    0, 0, 64, 0, 2, 0, 16, 2, 64, 0, 0, 64, 8, 0, 0, 0,
    0, 0, 64, 0, 2, 0, 16, 0, 64, 8, 4, 64, 24, 0, 0, 0,
    8, 0, 64, 0, 2, 0, 12, 0, 64, 8, 4, 0, 0, 0, 0, 0,
    16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 14, 0, 112, 0, 127,
    64, 60, 66, 60, 8, 60, 8, 66, 66, 8, 126, 8, 2, 16, 0, 0,
    64, 102, 66, 66, 8, 66, 8, 66, 66, 8, 64, 8, 2, 16, 0, 0,
    64, 90, 68, 66, 8, 66, 20, 102, 36, 8, 64, 8, 4, 16, 0, 0,
    64, 66, 68, 2, 8, 66, 20, 102, 36, 8, 32, 8, 8, 16, 0, 0,
    64, 66, 72, 12, 8, 66, 34, 90, 24, 8, 16, 8, 8, 16, 0, 0,
    124, 66, 124, 48, 8, 66, 34, 90, 24, 20, 8, 8, 16, 16, 0, 0,
    66, 66, 66, 64, 8, 66, 34, 66, 36, 34, 4, 8, 16, 16, 0, 0,
    66, 66, 66, 66, 8, 66, 65, 66, 36, 34, 2, 8, 32, 16, 0, 0,
    66, 66, 66, 66, 8, 66, 65, 66, 66, 65, 2, 8, 64, 16, 0, 0,
    124, 60, 124, 60, 127, 66, 65, 66, 66, 65, 126, 8, 64, 16, 66, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 14, 0, 112, 36, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 24, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    30, 66, 124, 60, 120, 126, 64, 58, 66, 62, 56, 66, 126, 66, 66, 60,
    32, 66, 66, 66, 68, 64, 64, 70, 66, 8, 68, 68, 64, 66, 70, 66,
    78, 66, 66, 66, 66, 64, 64, 66, 66, 8, 68, 72, 64, 66, 70, 66,
    82, 66, 66, 64, 66, 64, 64, 66, 66, 8, 4, 80, 64, 66, 74, 66,
    82, 126, 66, 64, 66, 64, 64, 78, 66, 8, 4, 96, 64, 90, 74, 66,
    82, 66, 124, 64, 66, 124, 124, 64, 126, 8, 4, 96, 64, 90, 82, 66,
    86, 66, 66, 64, 66, 64, 64, 64, 66, 8, 4, 80, 64, 102, 82, 66,
    74, 36, 66, 66, 66, 64, 64, 66, 66, 8, 4, 72, 64, 102, 98, 66,
    34, 36, 66, 66, 68, 64, 64, 66, 66, 8, 4, 68, 64, 66, 98, 66,
    28, 24, 124, 60, 120, 126, 126, 60, 66, 62, 31, 66, 64, 66, 66, 60,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0, 0,
    24, 62, 126, 60, 4, 60, 60, 8, 60, 56, 0, 8, 2, 0, 64, 8,
    36, 8, 64, 66, 4, 66, 66, 8, 66, 4, 24, 8, 4, 0, 32, 8,
    66, 8, 64, 66, 4, 2, 66, 8, 66, 2, 24, 24, 8, 126, 16, 0,
    66, 8, 32, 2, 126, 2, 66, 8, 66, 2, 0, 0, 16, 0, 8, 8,
    66, 8, 16, 2, 68, 2, 66, 4, 66, 2, 0, 0, 32, 0, 4, 8,
    66, 8, 12, 28, 68, 124, 124, 4, 60, 62, 0, 0, 16, 0, 8, 4,
    66, 8, 2, 2, 36, 64, 64, 4, 66, 66, 24, 24, 8, 126, 16, 2,
    66, 40, 66, 66, 20, 64, 64, 2, 66, 66, 24, 24, 4, 0, 32, 66,
    36, 24, 66, 66, 12, 64, 32, 2, 66, 66, 0, 0, 2, 0, 64, 66,
    24, 8, 60, 60, 4, 126, 28, 126, 60, 60, 0, 0, 0, 0, 0, 60,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 4, 32, 0, 0, 8, 0, 0, 0,
    0, 8, 0, 72, 8, 70, 57, 0, 8, 16, 0, 0, 8, 0, 24, 64,
    0, 8, 0, 72, 62, 41, 70, 0, 8, 16, 8, 8, 24, 0, 24, 64,
    0, 0, 0, 72, 73, 41, 66, 0, 16, 8, 73, 8, 0, 0, 0, 32,
    0, 8, 0, 126, 9, 22, 69, 0, 16, 8, 42, 8, 0, 0, 0, 16,
    0, 8, 0, 36, 14, 8, 57, 0, 16, 8, 28, 127, 0, 126, 0, 16,
    0, 8, 0, 36, 56, 8, 28, 0, 16, 8, 42, 8, 0, 0, 0, 8,
    0, 8, 0, 126, 72, 52, 34, 0, 16, 8, 73, 8, 0, 0, 0, 8,
    0, 8, 0, 18, 73, 74, 34, 0, 16, 8, 8, 8, 0, 0, 0, 4,
    0, 8, 34, 18, 62, 74, 34, 8, 8, 16, 0, 0, 0, 0, 0, 2,
    0, 8, 34, 18, 8, 49, 28, 8, 8, 16, 0, 0, 0, 0, 0, 2,
    0, 0, 34, 0, 0, 0, 0, 8, 4, 32, 0, 0, 0, 0, 0, 0,
    0, 0, 34, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0
};

/* Generates a font texture and returns the ID, or 0 if there was a failure */
static GLuint trackcontext_font_init(void)
{
    GLubyte font_data[128 * 128];
    int i, j, k;
    GLuint texture;

    for (i = 0, j = 0; i < 128 * 16; i++)
    {
        GLubyte bits = font_bitmap[i];
        for (k = 0; k < 8; k++, j++)
        {
            font_data[j] = (bits & 0x80) ? 255 : 0;
            bits = (bits & 0x7f) << 1;
        }
    }

    /* FIXME: use a proxy to check if texture size is supported */
    CALL(glGenTextures)(1, &texture);
    CALL(glPushAttrib)(GL_TEXTURE_BIT);
    CALL(glBindTexture)(GL_TEXTURE_2D, texture);
    CALL(glTexImage2D)(GL_TEXTURE_2D, 0, GL_INTENSITY4, 128, 128, 0,
                       GL_LUMINANCE, GL_UNSIGNED_BYTE, font_data);
    CALL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    CALL(glTexParameteri)(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    CALL(glPopAttrib)();

    return texture;
}

void bugle_gl_text_render(const char *msg, int x, int y)
{
    trackcontext_data *data;
    int x0;

    data = bugle_object_get_current_data(bugle_context_class, trackcontext_view);
    assert(data);

    if (!data->font_texture)
    {
        data->font_texture = trackcontext_font_init();
        if (!data->font_texture) return;
    }

    x0 = x;
    CALL(glPushAttrib)(GL_CURRENT_BIT | GL_TEXTURE_BIT | GL_ENABLE_BIT);
    CALL(glBindTexture)(GL_TEXTURE_2D, data->font_texture);
    CALL(glEnable)(GL_TEXTURE_2D);
    CALL(glBegin)(GL_QUADS);
    for (; *msg; msg++)
    {
        int r, c;
        GLfloat s1, s2, t1, t2;

        if (*msg == '\n')
        {
            x = x0;
            y -= 16;
            continue;
        }
        if ((unsigned char) *msg > 0x7f)
        {
            x += 8;
            continue;  /* Not in the bitmap */
        }
        r = 7 - ((*msg >> 4) & 0x7);
        c = *msg & 0xf;
        s1 = c / 16.0;
        s2 = (c + 1) / 16.0;
        t1 = r / 8.0;
        t2 = (r + 1) / 8.0;
        CALL(glTexCoord2f)(s1, t1);
        CALL(glVertex2i)(x, y - 16);
        CALL(glTexCoord2f)(s2, t1);
        CALL(glVertex2i)(x + 8, y - 16);
        CALL(glTexCoord2f)(s2, t2);
        CALL(glVertex2i)(x + 8, y);
        CALL(glTexCoord2f)(s1, t2);
        CALL(glVertex2i)(x, y);
        x += 8;
    }
    CALL(glEnd)();
    CALL(glPopAttrib)();
}
#endif

static bugle_bool trackcontext_newcontext(function_call *call, const callback_data *data)
{
    trackcontext_data *base, *up;
    glwin_context_create *create;

    create = bugle_glwin_context_create_save(call);

    if (create)
    {
        bugle_thread_lock_lock(context_mutex);

        base = BUGLE_ZALLOC(trackcontext_data);
        base->aux_shared = NULL;
        base->aux_unshared = NULL;
        base->create = create;
        if (create->share)
        {
            up = (trackcontext_data *) bugle_hashptr_get(&initial_values, create->share);
            if (!up)
            {
                bugle_log_printf("trackcontext", "newcontext", BUGLE_LOG_WARNING,
                                 "share context %p unknown", (void *) create->share);
                base->root_context = create->ctx;
            }
            else
                base->root_context = up->root_context;
        }

        bugle_hashptr_set(&initial_values, create->ctx, base);
        bugle_thread_lock_unlock(context_mutex);
    }

    return BUGLE_TRUE;
}

static bugle_bool trackcontext_callback(function_call *call, const callback_data *data)
{
    glwin_context ctx;
    object *obj, *ns;
    trackcontext_data *initial, *view;

    /* These calls may fail, so we must explicitly check for the
     * current context.
     */
    ctx = bugle_glwin_get_current_context();
    if (!ctx)
        bugle_object_set_current(bugle_context_class, NULL);
    else
    {
        bugle_thread_lock_lock(context_mutex);
        obj = bugle_hashptr_get(&context_objects, ctx);
        if (!obj)
        {
            obj = bugle_object_new(bugle_context_class, ctx, BUGLE_TRUE);
            bugle_hashptr_set(&context_objects, ctx, obj);
            initial = bugle_hashptr_get(&initial_values, ctx);
            if (initial == NULL)
            {
                bugle_log_printf("trackcontext", "makecurrent", BUGLE_LOG_WARNING,
                                 "context %p used but not created",
                                 (void *) ctx);
            }
            else
            {
                view = bugle_object_get_data(obj, trackcontext_view);
                *view = *initial;
                ns = bugle_hashptr_get(&namespace_objects, view->root_context);
                if (!ns)
                {
                    ns = bugle_object_new(bugle_namespace_class, ctx, BUGLE_TRUE);
                    bugle_hashptr_set(&namespace_objects, ctx, ns);
                }
                else
                    bugle_object_set_current(bugle_namespace_class, ns);
            }
        }
        else
            bugle_object_set_current(bugle_context_class, obj);
        bugle_thread_lock_unlock(context_mutex);
    }
    return BUGLE_TRUE;
}

static bugle_bool trackcontext_destroycontext(function_call *call, const callback_data *data)
{
    object *obj;
    glwin_context ctx;

    /* FIXME: leaks from namespace associations */
    obj = bugle_object_get_current(bugle_context_class);
    ctx = bugle_glwin_get_context_destroy(call);
    if (ctx)
        bugle_hashptr_set(&context_objects, ctx, NULL);
    return BUGLE_TRUE;
}

static void trackcontext_data_clear(void *data)
{
    trackcontext_data *d;

    d = (trackcontext_data *) data;
#if 0
    /* This is disabled for now because it causes problems when the
     * X connection is shut down but the context is never explicitly
     * destroyed. SDL does this.
     *
     * Unfortunately, this means that an application that creates and
     * explicitly destroys many contexts will leak aux contexts.
     */
    if (d->aux_shared) CALL(glXDestroyContext)(d->dpy, d->aux_shared);
    if (d->aux_unshared) CALL(glXDestroyContext)(d->dpy, d->aux_unshared);
#endif
    free(d->create);
}

static bugle_bool trackcontext_filter_set_initialise(filter_set *handle)
{
    filter *f;

    bugle_context_class = bugle_object_class_new(NULL);
    bugle_namespace_class = bugle_object_class_new(bugle_context_class);
    bugle_hashptr_init(&context_objects, (void (*)(void *)) bugle_object_free);
    bugle_hashptr_init(&namespace_objects, (void (*)(void *)) bugle_object_free);
    bugle_hashptr_init(&initial_values, free);

    f = bugle_filter_new(handle, "trackcontext");
    bugle_filter_order("invoke", "trackcontext");
    bugle_glwin_filter_catches_make_current(f, BUGLE_TRUE, trackcontext_callback);
    bugle_glwin_filter_catches_create_context(f, BUGLE_TRUE, trackcontext_newcontext);

    f = bugle_filter_new(handle, "trackcontext_destroy");
    bugle_filter_order("trackcontext_destroy", "invoke");
    bugle_glwin_filter_catches_destroy_context(f, BUGLE_TRUE, trackcontext_destroycontext);

    trackcontext_view = bugle_object_view_new(bugle_context_class,
                                              NULL,
                                              trackcontext_data_clear,
                                              sizeof(trackcontext_data));
    return BUGLE_TRUE;
}

static void trackcontext_filter_set_shutdown(filter_set *handle)
{
    bugle_hashptr_clear(&context_objects);
    bugle_hashptr_clear(&namespace_objects);
    bugle_hashptr_clear(&initial_values);
    bugle_object_class_free(bugle_namespace_class);
    bugle_object_class_free(bugle_context_class);
}

glwin_context bugle_get_aux_context(bugle_bool shared)
{
    trackcontext_data *data;
    glwin_context old_ctx, ctx;
    glwin_context *aux;

    data = bugle_object_get_current_data(bugle_context_class, trackcontext_view);
    if (!data) return NULL; /* no current context, hence no aux context */
    aux = shared ? &data->aux_shared : &data->aux_unshared;
    if (*aux == NULL)
    {
        old_ctx = bugle_glwin_get_current_context();

        ctx = bugle_glwin_context_create_new(data->create, shared);
        if (ctx == NULL)
            bugle_log("trackcontext", "aux", BUGLE_LOG_WARNING,
                      "could not create an auxiliary context: creation failed");
        *aux = ctx;
    }
    return *aux;
}


void trackcontext_initialise(void)
{
    static const filter_set_info trackcontext_info =
    {
        "trackcontext",
        trackcontext_filter_set_initialise,
        trackcontext_filter_set_shutdown,
        NULL,
        NULL,
        NULL,
        NULL /* No documentation */
    };

    bugle_filter_set_new(&trackcontext_info);
}
