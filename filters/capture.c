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

bool screenshot_callback(function_call *call, void *data)
{
    /* FIXME: track the frameno in the context?
     * FIXME: use an aux context to control the pixelstore
     * FIXME: use glX to get the window size if possible
     * FIXME: async copy via textures
     * FIXME: recycle memory
     */
    static int frameno = 0;
    char *fname;
    FILE *out;
    GLint viewport[4];
    GLint w, h;
    GLint aw; /* aligned */
    GLubyte *frame, *cur;
    size_t count;

    if (canonical_call(call) == FUNC_glXSwapBuffers)
    {
        if (!begin_internal_render())
        {
            fputs("warning: glXSwapBuffers called inside begin/end. Dropping frame\n", stderr);
            return true;
        }
        frameno++;
        xasprintf(&fname, "%s%.4d%s", filebase, frameno, filesuffix);
        out = fopen(fname, "wb");
        if (!out)
        {
            perror("failed to open screenshot file");
            free(fname);
            return true;
        }
        free(fname);
        CALL_glGetIntegerv(GL_VIEWPORT, viewport);
        w = viewport[2];
        h = viewport[3];
        aw = (w + 3) & ~3;
        frame = xmalloc(aw * h * 3 * sizeof(GLubyte));
        cur = frame + aw * h * 3;
        CALL_glReadPixels(viewport[0], viewport[1], w, h, GL_RGB,
                          GL_UNSIGNED_BYTE, frame);
        fprintf(out, "P6\n%d %d\n255\n", (int) w, (int) h);
        while (h > 0)
        {
            h--;
            cur -= aw * 3;
            count = fwrite(cur, sizeof(GLubyte), w * 3, out);
            if (count < w * 3)
            {
                perror("write error");
                fclose(out);
                free(frame);
                return true;
            }
        }
        if (fclose(out) != 0)
            perror("write error");
        free(frame);
        end_internal_render("screenshot", true);
    }
    return true;
}

static bool initialise_screenshot(filter_set *handle)
{
    register_filter(handle, "screenshot", screenshot_callback);
    register_filter_depends("invoke", "screenshot");
    filter_set_renders("screenshot");
    return true;
}

static bool set_variable_screenshot(filter_set *handle,
                                    const char *name,
                                    const char *value)
{
    /* FIXME: take a filebase */
    return false;
}

void initialise_filter_library(void)
{
    register_filter_set("screenshot", initialise_screenshot, NULL,
                        set_variable_screenshot);
}
