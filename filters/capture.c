/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004  Bruce Merry
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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
#include "src/types.h"
#include "src/canon.h"
#include "src/glutils.h"
#include "common/safemem.h"
#include "common/bool.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <GL/glx.h>

static char *filebase = "frame";
static const char *filesuffix = ".ppm";
static char *videofile = NULL;
static FILE *videopipe = NULL;
static char *videocodec = "huffyuv";

typedef struct
{
    int width, height, stride;
    GLubyte *pixels;
} screenshot_data;

static bool get_screenshot(screenshot_data *data)
{
    GLXContext aux, real;
    GLXDrawable old_read, old_write;
    Display *dpy;

    aux = get_aux_context();
    if (!aux) return false;

    if (!begin_internal_render())
    {
        fputs("warning: glXSwapBuffers called inside begin/end. Dropping frame\n", stderr);
        return false;
    }

    real = glXGetCurrentContext();
    old_write = glXGetCurrentDrawable();
    old_read = glXGetCurrentReadDrawable();
    dpy = glXGetCurrentDisplay();
    glXMakeContextCurrent(dpy, old_write, old_write, aux);
    glXQueryDrawable(dpy, old_write, GLX_WIDTH, &data->width);
    glXQueryDrawable(dpy, old_write, GLX_HEIGHT, &data->height);

    data->stride = (data->width + 3) & ~3;
    data->pixels = xmalloc(data->stride * data->height * 3 * sizeof(GLubyte));
    CALL_glReadPixels(0, 0, data->width, data->height, GL_RGB,
                      GL_UNSIGNED_BYTE, data->pixels);

    glXMakeContextCurrent(dpy, old_write, old_read, real);
    end_internal_render("screenshot", true);
    return true;
}

bool screenshot_callback(function_call *call, void *data)
{
    /* FIXME: track the frameno in the context?
     * FIXME: async copy via textures or PBO
     * FIXME: recycle memory
     */
    static int frameno = 0;
    int h;
    char *fname;
    GLubyte *cur;
    FILE *out;
    size_t count;
    screenshot_data shot;

    if (canonical_call(call) == FUNC_glXSwapBuffers)
    {
        if (!get_screenshot(&shot)) return true;

        frameno++;
        xasprintf(&fname, "%s%.4d%s", filebase, frameno, filesuffix);
        if (videopipe) out = videopipe;
        else out = fopen(fname, "wb");
        if (!out)
        {
            perror("failed to open screenshot file");
            free(fname);
            return true;
        }
        free(fname);

        h = shot.height;
        cur = shot.pixels + shot.stride * shot.height * 3;
        fprintf(out, "P6\n%d %d\n255\n", (int) shot.width, (int) shot.height);
        while (h > 0)
        {
            h--;
            cur -= shot.stride * 3;
            count = fwrite(cur, sizeof(GLubyte), shot.width * 3, out);
            if (count < shot.width * 3)
            {
                perror("write error");
                if (out != videopipe) fclose(out);
                free(shot.pixels);
                return true;
            }
        }
        if (out != videopipe && fclose(out) != 0)
            perror("write error");
        free(shot.pixels);
    }
    return true;
}

static bool initialise_screenshot(filter_set *handle)
{
    register_filter(handle, "screenshot", screenshot_callback);
    register_filter_depends("invoke", "screenshot");
    filter_set_renders("screenshot");
    if (videofile)
    {
        char *cmdline;

        xasprintf(&cmdline, "ppmtoy4m | ffmpeg -f yuv4mpegpipe -i - -vcodec %s -strict -1 -y %s",
                  videocodec, videofile);
        videopipe = popen(cmdline, "w");
        free(cmdline);
        if (!videopipe) return false;
    }
    return true;
}

static void destroy_screenshot(filter_set *handle)
{
    if (videopipe) pclose(videopipe);
}

static bool set_variable_screenshot(filter_set *handle,
                                    const char *name,
                                    const char *value)
{
    /* FIXME: take a filebase */
    if (strcmp(name, "video") == 0)
        videofile = xstrdup(value);
    else if (strcmp(name, "codec") == 0)
        videocodec = xstrdup(value);
    else
        return false;
    return true;
}

void initialise_filter_library(void)
{
    register_filter_set("screenshot", initialise_screenshot, destroy_screenshot,
                        set_variable_screenshot);
}
