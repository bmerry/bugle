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

#if HAVE_LAVC
# include <inttypes.h>
# include <ffmpeg/avcodec.h>
# include <ffmpeg/avformat.h>
# define CAPTURE_AV_FMT PIX_FMT_BGR24
# define CAPTURE_GL_FMT GL_BGR
#endif

typedef struct
{
    int width, height;
    size_t stride;         /* bytes between rows */
    GLubyte *pixels;
} screenshot_data;

static char *file_base = "frame";
static const char *file_suffix = ".ppm";
static char *video_codec = "huffyuv";
static int video_bitrate = 1000000;
static int start_frameno = 0;

static char *video_file = NULL;
static FILE *video_pipe = NULL;
static screenshot_data video_data = {0, 0, 0, NULL};
static bool video_done;

#if HAVE_LAVC
static AVFormatContext *video_context = NULL;
static AVStream *video_stream;
static AVFrame *video_raw, *video_yuv;
static uint8_t *video_buffer;
static size_t video_buffer_size = 2000000; /* FIXME: what should it be? */

static AVFrame *allocate_video_frame(int fmt, int width, int height,
                                     uint8_t *buffer)
{
    AVFrame *f;
    size_t size;

    f = avcodec_alloc_frame();
    if (!f)
    {
        fprintf(stderr, "failed to allocate frame\n");
        exit(1);
    }
    if (!buffer)
    {
        size = avpicture_get_size(fmt, width, height);
        buffer = xmalloc(size);
    }
    avpicture_fill((AVPicture *) f, buffer, fmt, width, height);
    return f;
}

static bool initialise_lavc(int width, int height)
{
    AVOutputFormat *fmt;
    AVCodecContext *c;
    AVCodec *codec;

    av_register_all();
    fmt = guess_format(NULL, video_file, NULL);
    if (!fmt) fmt = guess_format("avi", NULL, NULL);
    if (!fmt) return false;
    video_context = av_mallocz(sizeof(AVFormatContext));
    if (!video_context) return false;
    video_context->oformat = fmt;
    snprintf(video_context->filename,
             sizeof(video_context->filename), "%s", video_file);
    /* FIXME: what does the second parameter (id) do? */
    video_stream = av_new_stream(video_context, 0);
    if (!video_stream) return false;
    codec = avcodec_find_encoder_by_name(video_codec);
    if (!codec) codec = avcodec_find_encoder(CODEC_ID_HUFFYUV);
    if (!codec) return false;
    c = &video_stream->codec;
    c->codec_type = CODEC_TYPE_VIDEO;
    c->codec_id = codec->id;
    if (c->codec_id == CODEC_ID_HUFFYUV) c->pix_fmt = PIX_FMT_YUV422P;
    if (c->codec_id == CODEC_ID_FFV1) c->strict_std_compliance = -1;
    c->bit_rate = video_bitrate;
    c->width = width;
    c->height = height;
    c->frame_rate = 30;   /* FIXME: user should specify */
    c->frame_rate_base = 1;
    c->gop_size = 12;     /* FIXME: user should specify */
    /* FIXME: what does the NULL do? */
    if (av_set_parameters(video_context, NULL) < 0) return false;
    if (avcodec_open(c, codec) < 0) return false;
    video_buffer = xmalloc(video_buffer_size);

    video_data.width = width;
    video_data.height = height;
    video_data.stride = video_data.width * 3;
    video_data.pixels = xmalloc(video_data.stride * video_data.height);
    video_raw = allocate_video_frame(CAPTURE_AV_FMT, width, height,
                                     video_data.pixels);
    video_yuv = allocate_video_frame(c->pix_fmt, width, height, NULL);
    if (url_fopen(&video_context->pb, video_file, URL_WRONLY) < 0)
    {
        fprintf(stderr, "failed to open video output file %s\n", video_file);
        return false;
    }
    av_write_header(video_context);
    return true;
}

static void finalise_lavc(void)
{
    int i;

    /* Write any delayed frames.
     * FIXME: For some reason this causes a segfault, so we disable it.
     */
#if 0
    size_t out_size;
    do
    {
        out_size = avcodec_encode_video(&video_stream->codec,
                                        video_buffer, video_buffer_size, NULL);
        if (out_size)
            if (av_write_frame(video_context, video_stream->index,
                               video_buffer, out_size) != 0)
            {
                fprintf(stderr, "encoding failed\n");
                exit(1);
            }
    } while (out_size);
#endif

    /* Close it all down */
    av_write_trailer(video_context);
    avcodec_close(&video_stream->codec);
    av_free(video_yuv->data[0]);
    /* We don't free video_raw, since that memory belongs to video_data */
    av_free(video_yuv);
    av_free(video_raw);
    av_free(video_buffer);
    for (i = 0; i < video_context->nb_streams; i++)
        av_freep(&video_context->streams[i]);
    url_fclose(&video_context->pb);
    av_free(video_context);

    video_context = NULL;
}

#endif

/* If data->pixels == NULL, or if data->width and data->height do not
 * match the current frame, new memory is allocated. Otherwise the
 * existing memory is reused.
 */
static void prepare_data(screenshot_data *data, int width, int height, int align)
{
    int stride;

    stride = ((width + align - 1) & ~(align - 1)) * 3;
    if (!data->pixels
        || data->width != width
        || data->height != height
        || data->stride != stride)
    {
        if (data->pixels) free(data->pixels);
        data->pixels = xmalloc(stride * height);
        data->width = width;
        data->height = height;
        data->stride = stride;
    }
}

static bool get_screenshot(screenshot_data *data, GLenum format)
{
    static screenshot_data my_data = {0, 0, 0, NULL};
    GLXContext aux, real;
    GLXDrawable old_read, old_write;
    Display *dpy;
    GLubyte *cur_in, *cur_out;
    int width, height;
    int i;

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
    glXQueryDrawable(dpy, old_write, GLX_WIDTH, &width);
    glXQueryDrawable(dpy, old_write, GLX_HEIGHT, &height);

    prepare_data(&my_data, width, height, 4);
    prepare_data(data, width, height, 1);

    CALL_glReadPixels(0, 0, width, height, format,
                      GL_UNSIGNED_BYTE, my_data.pixels);
    cur_in = my_data.pixels;
    cur_out = data->pixels + data->stride * (data->height - 1);
    for (i = 0; i < height; i++)
    {
        memcpy(cur_out, cur_in, data->stride);
        cur_in += my_data.stride;
        cur_out -= data->stride;
    }

    glXMakeContextCurrent(dpy, old_write, old_read, real);
    end_internal_render("screenshot", true);
    return true;
}

static bool screenshot_stream(FILE *out, bool check_size)
{
    screenshot_data old_data;
    size_t size, count;

    old_data = video_data;
    if (!get_screenshot(&video_data, GL_RGB)) return false;
    if (check_size && old_data.pixels && old_data.pixels != video_data.pixels)
    {
        fprintf(stderr, "size changed from %dx%d to %dx%d; halting recording\n",
                old_data.width, old_data.height,
                video_data.width, video_data.height);
        video_done = true;
        return false;
    }

    fprintf(out, "P6\n%d %d\n255\n",
            (int) video_data.width, (int) video_data.height);
    size = video_data.stride * video_data.height;
    count = fwrite(video_data.pixels, sizeof(GLubyte),
                   size, out);
    if (count != size)
    {
        perror("write error");
        return false;
    }
    return true;
}

#if HAVE_LAVC
static void screenshot_video()
{
    size_t out_size;
    screenshot_data old_data;
    AVCodecContext *c;

    old_data = video_data;
    if (!get_screenshot(&video_data, CAPTURE_GL_FMT)) return;
    if (!video_context)
        initialise_lavc(video_data.width, video_data.height);
    if (old_data.pixels && old_data.pixels != video_data.pixels)
    {
        fprintf(stderr, "size changed from %dx%d to %dx%d; halting recording\n",
                old_data.width, old_data.height,
                video_data.width, video_data.height);
        video_done = true;
        finalise_lavc();
        return;
    }
    c = &video_stream->codec;
    img_convert((AVPicture *) video_yuv, c->pix_fmt,
                (AVPicture *) video_raw, CAPTURE_AV_FMT,
                video_data.width, video_data.height);
    out_size = avcodec_encode_video(&video_stream->codec,
                                    video_buffer, video_buffer_size,
                                    video_yuv);
    if (out_size != 0)
        if (av_write_frame(video_context, video_stream->index,
                           video_buffer, out_size) != 0)
        {
            fprintf(stderr, "encoding failed\n");
            exit(1);
        }
}

#else /* !HAVE_LAVC */

static void screenshot_video(int frameno)
{
    if (!screenshot_stream(video_pipe, true))
    {
        pclose(video_pipe);
        video_pipe = NULL;
    }
}

#endif /* !HAVE_LAVC */

static void screenshot_file(int frameno)
{
    char *fname;
    FILE *out;

    xasprintf(&fname, "%s%.4d%s", file_base, frameno, file_suffix);
    out = fopen(fname, "wb");
    free(fname);
    if (!out)
    {
        perror("failed to open screenshot file");
        free(fname);
        return;
    }
    if (!screenshot_stream(out, false)) return;
    if (fclose(out) != 0)
        perror("write error");
}

bool screenshot_callback(function_call *call, void *data)
{
    /* FIXME: track the frameno in the context?
     * FIXME: async copy via textures or PBO
     * FIXME: recycle memory
     * FIXME: thread safety
     */
    static int frameno = 0;

    if (canonical_call(call) == FUNC_glXSwapBuffers)
    {
        if (frameno >= start_frameno)
        {
            if (video_file)
            {
                if (!video_done) screenshot_video(frameno);
            }
            else screenshot_file(frameno);
        }
        frameno++;
    }
    return true;
}

static bool initialise_screenshot(filter_set *handle)
{
    register_filter(handle, "screenshot", screenshot_callback);
    register_filter_depends("invoke", "screenshot");
    filter_set_renders("screenshot");
    if (video_file)
    {
        video_done = false; /* becomes true if we resize */
#if !HAVE_LAVC
        char *cmdline;

        xasprintf(&cmdline, "ppmtoy4m | ffmpeg -f yuv4mpegpipe -i - -vcodec %s -strict -1 -y %s",
                  video_codec, video_file);
        video_pipe = popen(cmdline, "w");
        free(cmdline);
        if (!video_pipe) return false;
#endif
        /* Note: we only initialise libavcodec on the first frame, because
         * we need the frame size.
         */
    }
    return true;
}

static void destroy_screenshot(filter_set *handle)
{
#if HAVE_LAVC
    if (video_context)
        finalise_lavc();
#endif
    if (video_pipe) pclose(video_pipe);
    if (video_data.pixels)
    {
        free(video_data.pixels);
        video_data.width = video_data.height = 0;
        video_data.stride = 0;
        video_data.pixels = NULL;
    }
}

static bool set_variable_screenshot(filter_set *handle,
                                    const char *name,
                                    const char *value)
{
    /* FIXME: take a filebase */
    if (strcmp(name, "video") == 0)
        video_file = xstrdup(value);
    else if (strcmp(name, "codec") == 0)
        video_codec = xstrdup(value);
    else if (strcmp(name, "start") == 0)
        start_frameno = atoi(value);
    else if (strcmp(name, "bitrate") == 0)
        video_bitrate = atoi(value);
    else
        return false;
    return true;
}

void initialise_filter_library(void)
{
    register_filter_set("screenshot", initialise_screenshot, destroy_screenshot,
                        set_variable_screenshot);
}
