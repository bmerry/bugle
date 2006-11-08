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

/* I have no idea what I'm doing with the video encoding, since
 * libavformat has no documentation that I can find. It is probably full
 * of bugs.
 *
 * This file is also blatantly unsafe to use with multiple contexts.
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#define _XOPEN_SOURCE 500
#include "src/filters.h"
#include "src/utils.h"
#include "src/glutils.h"
#include "src/glfuncs.h"
#include "src/glexts.h"
#include "src/tracker.h"
#include "src/xevent.h"
#include "gl2ps/gl2ps.h"
#include "common/hashtable.h"
#include "common/safemem.h"
#include "common/bool.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <math.h>
#include <GL/glx.h>
#include <sys/time.h>

#if HAVE_LAVC
# include <inttypes.h>
# include <avcodec.h>
# include <avformat.h>
# define CAPTURE_AV_FMT PIX_FMT_RGB24
# define CAPTURE_GL_FMT GL_RGB
#endif
#define CAPTURE_GL_ELEMENTS 3

typedef struct
{
    int width, height;
    size_t stride;         /* bytes between rows */
    GLubyte *pixels;
    GLuint pbo;
    bool pbo_mapped;       /* true during glMapBuffer/glUnmapBuffer */
    int multiplicity;      /* number of times to write to video stream */
} screenshot_data;

/* Data that must be kept while in screenshot code, to allow restoration.
 * It is not directly related to an OpenGL context.
 */
typedef struct
{
    GLXContext old_context;
    GLXDrawable old_read, old_write;
} screenshot_context;

/* General settings */
static bool video = false;
static char *video_filename = NULL;
/* Still settings */
static xevent_key key_screenshot = { XK_S, ControlMask | ShiftMask | Mod1Mask, true };
/* Video settings */
static char *video_codec = NULL;
static bool video_sample_all = false;
static long video_bitrate = 7500000;
static long video_lag = 1;     /* latency between readpixels and encoding */

/* General data */
static int video_cur;  /* index of the next circular queue index to capture into */
static bool video_first;
static screenshot_data *video_data;
/* Still data */
static bool keypress_screenshot = false;
/* Video data */
static FILE *video_pipe = NULL;  /* Used for ppmtoy4m */
static bool video_done = false;
static double video_frame_time = 0.0;
static double video_frame_step = 1.0 / 30.0; /* FIXME: depends on frame rate */

static char *interpolate_filename(const char *pattern, int frame)
{
    char *out;

    if (strchr(pattern, '%'))
    {
        bugle_asprintf(&out, pattern, frame);
        return out;
    }
    else
        return bugle_strdup(pattern);
}

/* If data->pixels == NULL and pbo = 0,
 * or if data->width and data->height do not match the current frame,
 * new memory is allocated. Otherwise the existing memory is reused.
 * This function must be called from the aux context.
 */
static void prepare_screenshot_data(screenshot_data *data,
                                    int width, int height,
                                    int align, bool use_pbo)
{
    size_t stride;

    stride = ((width + align - 1) & ~(align - 1)) * CAPTURE_GL_ELEMENTS;
    if ((!data->pixels && !data->pbo)
        || data->width != width
        || data->height != height
        || data->stride != stride)
    {
        if (data->pixels) free(data->pixels);
#ifdef GL_EXT_pixel_buffer_object
        if (data->pbo) CALL_glDeleteBuffersARB(1, &data->pbo);
#endif
        data->width = width;
        data->height = height;
        data->stride = stride;
#ifdef GL_EXT_pixel_buffer_object
        if (use_pbo && bugle_gl_has_extension(BUGLE_GL_EXT_pixel_buffer_object))
        {
            CALL_glGenBuffersARB(1, &data->pbo);
            CALL_glBindBufferARB(GL_PIXEL_PACK_BUFFER_EXT, data->pbo);
            CALL_glBufferDataARB(GL_PIXEL_PACK_BUFFER_EXT, stride * height,
                                 NULL, GL_DYNAMIC_READ_ARB);
            CALL_glBindBufferARB(GL_PIXEL_PACK_BUFFER_EXT, 0);
            data->pixels = NULL;
        }
        else
#endif
        {
            data->pixels = bugle_malloc(stride * height);
            data->pbo = 0;
        }
    }
}

/* These two functions should bracket all screenshot-using code. They are
 * responsible for checking for in begin/end and switching to the aux
 * context. If screenshot_start returns false, do not continue.
 *
 * The argument must point to a structure which screenshot_start will
 * populate with data that should then be passed to screenshot_stop.
 */
static bool screenshot_start(screenshot_context *ssctx)
{
    GLXContext aux;
    Display *dpy;

    ssctx->old_context = CALL_glXGetCurrentContext();
    ssctx->old_write = CALL_glXGetCurrentDrawable();
    ssctx->old_read = CALL_glXGetCurrentReadDrawable();
    dpy = CALL_glXGetCurrentDisplay();
    aux = bugle_get_aux_context(false);
    if (!aux) return false;
    if (!bugle_begin_internal_render())
    {
        fputs("warning: glXSwapBuffers called inside begin/end - skipping frame\n", stderr);
        return false;
    }
    CALL_glXMakeContextCurrent(dpy, ssctx->old_write, ssctx->old_write, aux);
    return true;
}

static void screenshot_stop(screenshot_context *ssctx)
{
    Display *dpy;

    dpy = CALL_glXGetCurrentDisplay();
    CALL_glXMakeContextCurrent(dpy, ssctx->old_write, ssctx->old_read, ssctx->old_context);
}

/* FIXME: we do not currently free the PBO, since we have no way of knowing
 * whether we are in the right context, or for that matter if the context
 * has been deleted.
 */
static void free_screenshot_data(screenshot_data *data)
{
    if (data->pixels) free(data->pixels);
}

#if HAVE_LAVC
static AVFormatContext *video_context = NULL;
static AVStream *video_stream;
static AVFrame *video_raw, *video_yuv;
static uint8_t *video_buffer;
static size_t video_buffer_size = 2000000; /* FIXME: what should it be? */

static AVFrame *allocate_video_frame(int fmt, int width, int height,
                                     bool create)
{
    AVFrame *f;
    size_t size;
    void *buffer = NULL;

    f = avcodec_alloc_frame();
    if (!f)
    {
        fprintf(stderr, "failed to allocate frame\n");
        exit(1);
    }
    size = avpicture_get_size(fmt, width, height);
    if (create) buffer = bugle_malloc(size);
    avpicture_fill((AVPicture *) f, buffer, fmt, width, height);
    return f;
}

static bool initialise_lavc(int width, int height)
{
    AVOutputFormat *fmt;
    AVCodecContext *c;
    AVCodec *codec;

    av_register_all();
    fmt = guess_format(NULL, video_filename, NULL);
    if (!fmt) fmt = guess_format("avi", NULL, NULL);
    if (!fmt) return false;
#if LIBAVFORMAT_VERSION_INT >= 0x000409
    video_context = av_alloc_format_context();
#else
    video_context = av_mallocz(sizeof(AVFormatContext));
#endif
    if (!video_context) return false;
    video_context->oformat = fmt;
    snprintf(video_context->filename,
             sizeof(video_context->filename), "%s", video_filename);
    /* FIXME: what does the second parameter (id) do? */
    video_stream = av_new_stream(video_context, 0);
    if (!video_stream) return false;
    codec = avcodec_find_encoder_by_name(video_codec);
    if (!codec) codec = avcodec_find_encoder(CODEC_ID_HUFFYUV);
    if (!codec) return false;
#if LIBAVFORMAT_VERSION_INT >= 0x00310000  /* Version 50? */
    c = video_stream->codec;
#else
    c = &video_stream->codec;
#endif
    c->codec_type = CODEC_TYPE_VIDEO;
    c->codec_id = codec->id;
    if (c->codec_id == CODEC_ID_HUFFYUV) c->pix_fmt = PIX_FMT_YUV422P;
    else c->pix_fmt = PIX_FMT_YUV420P;
    if (c->codec_id == CODEC_ID_FFV1) c->strict_std_compliance = -1;
    c->bit_rate = video_bitrate;
    c->width = width;
    c->height = height;
#if LIBAVFORMAT_VERSION_INT >= 0x00310000 /* Version 50? */
    c->time_base.den = 30;
    c->time_base.num = 1;
#else
    c->frame_rate = 30;   /* FIXME: user should specify */
    c->frame_rate_base = 1;
#endif
    c->gop_size = 12;     /* FIXME: user should specify */
    /* FIXME: what does the NULL do? */
    if (av_set_parameters(video_context, NULL) < 0) return false;
    if (avcodec_open(c, codec) < 0) return false;
    video_buffer = bugle_malloc(video_buffer_size);
    video_raw = allocate_video_frame(CAPTURE_AV_FMT, width, height, false);
    video_yuv = allocate_video_frame(c->pix_fmt, width, height, true);
    if (url_fopen(&video_context->pb, video_filename, URL_WRONLY) < 0)
    {
        fprintf(stderr, "failed to open video output file %s\n", video_filename);
        return false;
    }
    av_write_header(video_context);
    return true;
}

static void finalise_lavc(void)
{
    int i;
    AVCodecContext *c;
    size_t out_size;

#if LIBAVFORMAT_VERSION_INT >= 0x00310000
    c = video_stream->codec;
#else
    c = &video_stream->codec;
#endif
    /* Write any delayed frames. On older ffmpeg's this seemed to cause
     * a segfault and needed different code, so we leave it out.
     */
#if LIBAVFORMAT_VERSION_INT >= 0x000409
    do
    {
        AVPacket pkt;
        int ret;

        out_size = avcodec_encode_video(c, video_buffer, video_buffer_size, NULL);
        if (out_size)
        {
            av_init_packet(&pkt);
            pkt.pts = c->coded_frame->pts;
            if (c->coded_frame->key_frame)
                pkt.flags |= PKT_FLAG_KEY;
            pkt.stream_index = video_stream->index;
            pkt.data = video_buffer;
            pkt.size = out_size;
            ret = av_write_frame(video_context, &pkt);
            if (ret != 0)
            {
                fprintf(stderr, "encoding failed\n");
                exit(1);
            }
        }
    } while (out_size);
#endif

    /* Close it all down */
    av_write_trailer(video_context);
#if LIBAVFORMAT_VERSION_INT >= 0x00310000
    avcodec_close(video_stream->codec);
#else
    avcodec_close(&video_stream->codec);
#endif
    av_free(video_yuv->data[0]);
    /* We don't free video_raw, since that memory belongs to video_data */
    av_free(video_yuv);
    av_free(video_raw);
    av_free(video_buffer);
    for (i = 0; i < video_context->nb_streams; i++)
        av_freep(&video_context->streams[i]);
    url_fclose(&video_context->pb);
    av_free(video_context);

    for (i = 0; i < video_lag; i++)
        free_screenshot_data(&video_data[i]);
    free(video_data);

    video_context = NULL;
}

#endif /* HAVE_LAVC */

/* There are three use cases:
 * 1. There is no PBO. There is nothing to do.
 * 2. Use glMapBufferARB. In this case, data->pixels is NULL on entry, and
 * we set it to the mapped value and set pbo_mapped to true. If glMapBufferARB
 * fails, we allocate system memory and fall back to case 3.
 * 3. We use glGetBufferSubDataARB. In this case, data->pixels is non-NULL on
 * entry and of the correct size, and we read straight into it and set
 * pbo_mapped to false.
 */
static bool map_screenshot(screenshot_data *data)
{
#ifdef GL_EXT_pixel_buffer_object
    GLint size = 0;
    if (data->pbo)
    {
        CALL_glBindBufferARB(GL_PIXEL_PACK_BUFFER_EXT, data->pbo);

        if (!data->pixels)
        {
            data->pixels = CALL_glMapBufferARB(GL_PIXEL_PACK_BUFFER_EXT, GL_READ_ONLY_ARB);
            if (!data->pixels)
                CALL_glGetError(); /* hide the error from end_internal_render() */
            else
            {
                data->pbo_mapped = true;
                bugle_end_internal_render("map_screenshot", true);
                CALL_glBindBufferARB(GL_PIXEL_PACK_BUFFER_EXT, 0);
                return true;
            }
        }
        // If we get here, we're in case 3
        CALL_glGetBufferParameterivARB(GL_PIXEL_PACK_BUFFER_EXT, GL_BUFFER_SIZE_ARB, &size);
        if (!data->pixels)
            data->pixels = bugle_malloc(size);
        CALL_glGetBufferSubDataARB(GL_PIXEL_PACK_BUFFER_EXT, 0, size, data->pixels);
        data->pbo_mapped = false;
        CALL_glBindBufferARB(GL_PIXEL_PACK_BUFFER_EXT, 0);
        bugle_end_internal_render("map_screenshot", true);
    }
#endif
    return true;
}

static bool unmap_screenshot(screenshot_data *data)
{
#ifdef GL_EXT_pixel_buffer_object
    if (data->pbo && data->pbo_mapped)
    {
        bool ret;

        CALL_glBindBufferARB(GL_PIXEL_PACK_BUFFER_EXT, data->pbo);
        ret = CALL_glUnmapBufferARB(GL_PIXEL_PACK_BUFFER_EXT);
        CALL_glBindBufferARB(GL_PIXEL_PACK_BUFFER_EXT, 0);
        bugle_end_internal_render("unmap_screenshot", true);
        data->pixels = NULL;
        return ret;
    }
    else
#endif
    {
        return true;
    }
}

static bool do_screenshot(GLenum format, int test_width, int test_height,
                          screenshot_data **data)
{
    GLXDrawable drawable;
    Display *dpy;
    screenshot_data *cur;
    unsigned int w, h;
    int width, height;

    *data = &video_data[(video_cur + video_lag - 1) % video_lag];
    cur = &video_data[video_cur];
    video_cur = (video_cur + 1) % video_lag;

    drawable = CALL_glXGetCurrentDrawable();
    dpy = CALL_glXGetCurrentDisplay();
    CALL_glXQueryDrawable(dpy, drawable, GLX_WIDTH, &w);
    CALL_glXQueryDrawable(dpy, drawable, GLX_HEIGHT, &h);
    width = w;
    height = h;
    if (test_width != -1 || test_height != -1)
        if (width != test_width || height != test_height)
        {
            fprintf(stderr, "size changed from %dx%d to %dx%d, stopping recording\n",
                    test_width, test_height, width, height);
            return false;
        }

    prepare_screenshot_data(cur, width, height, 4, true);

    if (!bugle_begin_internal_render()) return false;
#ifdef GL_EXT_pixel_buffer_object
    if (cur->pbo)
        CALL_glBindBufferARB(GL_PIXEL_PACK_BUFFER_EXT, cur->pbo);
#endif
    CALL_glReadPixels(0, 0, width, height, format,
                      GL_UNSIGNED_BYTE, cur->pbo ? NULL : cur->pixels);
#ifdef GL_EXT_pixel_buffer_object
    if (cur->pbo)
        CALL_glBindBufferARB(GL_PIXEL_PACK_BUFFER_EXT, 0);
#endif
    bugle_end_internal_render("do_screenshot", true);

    return true;
}

static bool screenshot_stream(FILE *out, bool check_size)
{
    screenshot_data *fetch;
    GLubyte *cur;
    size_t size, count;
    bool ret = true;
    int i;

    if (check_size && !video_first)
        video_done = !do_screenshot(GL_RGB, video_data[0].width, video_data[0].height, &fetch);
    else
        do_screenshot(GL_RGB, -1, -1, &fetch);
    video_first = false;

    if (fetch->width > 0)
    {
        if (!map_screenshot(fetch)) return false;
        fprintf(out, "P6\n%d %d\n255\n",
                fetch->width, fetch->height);
        cur = fetch->pixels + fetch->stride * (fetch->height - 1);
        size = fetch->width * 3;
        for (i = 0; i < fetch->height; i++)
        {
            count = fwrite(cur, sizeof(GLubyte), size, out);
            if (count != size)
            {
                perror("write error");
                ret = false;
                break;
            }
            cur -= fetch->stride;
        }
        unmap_screenshot(fetch);
    }
    return ret;
}

#if HAVE_LAVC
static void screenshot_video()
{
    screenshot_data *fetch;
    AVCodecContext *c;
    size_t out_size;
    int i, ret;
    struct timeval tv;
    double t = 0.0;
    screenshot_context ssctx;

    if (!video_sample_all)
    {
        gettimeofday(&tv, NULL);
        t = tv.tv_sec + 1e-6 * tv.tv_usec;
        if (video_first) /* first frame */
            video_frame_time = t;
        else if (t < video_frame_time)
            return; /* drop the frame because it is too soon */

        /* Repeat frames to make up for low app framerate */
        video_data[video_cur].multiplicity = 0;
        while (t >= video_frame_time)
        {
            video_frame_time += video_frame_step;
            video_data[video_cur].multiplicity++;
        }
    }
    else
        video_data[video_cur].multiplicity = 1;

    /* We only do this here, because it is potentially expensive and if we
     * are rendering faster than capturing we don't want the hit if we're
     * just dropping the frame.
     */
    if (!screenshot_start(&ssctx)) return;

    if (!video_first)
        video_done = !do_screenshot(CAPTURE_GL_FMT, video_data[0].width, video_data[0].height, &fetch);
    else
        do_screenshot(CAPTURE_GL_FMT, -1, -1, &fetch);
    video_first = false;

    if (fetch->width > 0)
    {
        if (!video_context)
            initialise_lavc(fetch->width, fetch->height);
#if LIBAVFORMAT_VERSION_INT >= 0x00310000
        c = video_stream->codec;
#else
        c = &video_stream->codec;
#endif
        if (!map_screenshot(fetch))
        {
            screenshot_stop(&ssctx);
            return;
        }
        video_raw->data[0] = fetch->pixels + fetch->stride * (fetch->height - 1);
        video_raw->linesize[0] = -fetch->stride;
        img_convert((AVPicture *) video_yuv, c->pix_fmt,
                (AVPicture *) video_raw, CAPTURE_AV_FMT,
                fetch->width, fetch->height);
        for (i = 0; i < fetch->multiplicity; i++)
        {
#if LIBAVFORMAT_VERSION_INT >= 0x00310000
            out_size = avcodec_encode_video(video_stream->codec,
                                            video_buffer, video_buffer_size,
                                            video_yuv);
#else
            out_size = avcodec_encode_video(&video_stream->codec,
                                            video_buffer, video_buffer_size,
                                            video_yuv);
#endif
            if (out_size != 0)
            {
#if LIBAVFORMAT_VERSION_INT >= 0x000409
                AVPacket pkt;

                av_init_packet(&pkt);
                pkt.pts = c->coded_frame->pts;
                if (c->coded_frame->key_frame)
                    pkt.flags |= PKT_FLAG_KEY;
                pkt.stream_index = video_stream->index;
                pkt.data = video_buffer;
                pkt.size = out_size;
                ret = av_write_frame(video_context, &pkt);
#else
                ret = av_write_frame(video_context, video_stream->index,
                                     video_buffer, out_size);
#endif
                if (ret != 0)
                {
                    fprintf(stderr, "encoding failed\n");
                    exit(1);
                }
            }
        }
        unmap_screenshot(fetch);
    }
    screenshot_stop(&ssctx);
}

#else /* !HAVE_LAVC */

static void screenshot_video(int frameno)
{
    screenshot_context ssctx;
    if (!screenshot_start(&ssctx)) return;

    if (!screenshot_stream(video_pipe, true))
    {
        pclose(video_pipe);
        video_pipe = NULL;
    }
    screenshot_stop(&ssctx);
}

#endif /* !HAVE_LAVC */

static void screenshot_file(int frameno)
{
    char *fname;
    FILE *out;
    screenshot_context ssctx;

    if (!screenshot_start(&ssctx)) return;
    fname = interpolate_filename(video_filename, frameno);
    out = fopen(fname, "wb");
    free(fname);
    if (!out)
    {
        perror("failed to open screenshot file");
        screenshot_stop(&ssctx);
        return;
    }
    screenshot_stream(out, false);
    if (fclose(out) != 0)
        perror("write error");
    screenshot_stop(&ssctx);
}

bool screenshot_callback(function_call *call, const callback_data *data)
{
    /* FIXME: track the frameno in the context?
     */
    static int frameno = 0;

    if (video)
    {
        if (!video_done)
            screenshot_video(frameno);
    }
    else if (keypress_screenshot)
    {
        screenshot_file(frameno);
        keypress_screenshot = false;
    }
    frameno++;
    return true;
}

static bool initialise_screenshot(filter_set *handle)
{
    filter *f;

    f = bugle_register_filter(handle, "screenshot");
    bugle_register_filter_catches(f, GROUP_glXSwapBuffers, false, screenshot_callback);
    bugle_register_filter_depends("invoke", "screenshot");

    video_data = bugle_calloc(video_lag, sizeof(screenshot_data));
    video_cur = 0;
    video_first = true;
    if (video)
    {
        video_done = false; /* becomes true if we resize */
        if (!video_filename)
            video_filename = bugle_strdup("/tmp/bugle.avi");
#if !HAVE_LAVC
        char *cmdline;

        bugle_asprintf(&cmdline, "ppmtoy4m | ffmpeg -f yuv4mpegpipe -i - -vcodec %s -strict -1 -y %s",
                       video_codec, video_filename);
        video_pipe = popen(cmdline, "w");
        free(cmdline);
        if (!video_pipe) return false;
#endif
        /* Note: we only initialise libavcodec on the first frame, because
         * we need the frame size.
         */
    }
    else
    {
        if (!video_filename)
            video_filename = bugle_strdup("/tmp/bugle.ppm");
        video_lag = 1;
        /* FIXME: should only intercept the key when enabled */
        bugle_register_xevent_key(&key_screenshot, NULL, bugle_xevent_key_callback_flag, &keypress_screenshot);
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
    if (video_codec) free(video_codec);
}

/* The values are set to &seen_extensions to indicate presence, then
 * lazily deleted by setting to NULL.
 */
static bugle_hash_table seen_extensions;
static const char *gl_version = "GL_VERSION_1_1";
static const char *glx_version = "GLX_VERSION_1_2";

static bool showextensions_callback(function_call *call, const callback_data *data)
{
    size_t i;
    const group_data *info;
    const gl_function *glinfo;

    info = &budgie_group_table[call->generic.group];
    glinfo = &bugle_gl_function_table[call->generic.id];
    if (glinfo->extension)
        bugle_hash_set(&seen_extensions, glinfo->extension, &seen_extensions);
    else
    {
        if (glinfo->version && glinfo->version[2] == 'X' && strcmp(glinfo->version, glx_version) > 0)
            glx_version = glinfo->version;
        if (glinfo->version && glinfo->version[2] == '_' && strcmp(glinfo->version, gl_version) > 0)
            gl_version = glinfo->version;
    }

    for (i = 0; i < info->num_parameters; i++)
    {
        if (info->parameters[i].type == TYPE_6GLenum)
        {
            GLenum e;
            const gl_token *t;

            e = *(const GLenum *) call->generic.args[i];
            t = bugle_gl_enum_to_token_struct(e);
            if (t && t->extension)
                bugle_hash_set(&seen_extensions, t->extension, &seen_extensions);
        }
    }
    return true;
}

static bool initialise_showextensions(filter_set *handle)
{
    filter *f;

    f = bugle_register_filter(handle, "showextensions");
    bugle_register_filter_catches_all(f, false, showextensions_callback);
    /* The order mainly doesn't matter, but making it a pre-filter
     * reduces the risk of another filter aborting the call.
     */
    bugle_register_filter_depends("invoke", "showextensions");
    bugle_hash_init(&seen_extensions, false);
    return true;
}

static void destroy_showextensions(filter_set *handle)
{
    int i;
    budgie_function f;

    printf("Min GL version: %s\n", gl_version);
    printf("Min GLX version: %s\n", glx_version);
    printf("Used extensions:");
    for (i = 0; i < bugle_gl_token_count; i++)
    {
        const char *ver, *ext;

        ver = bugle_gl_tokens_name[i].version;
        ext = bugle_gl_tokens_name[i].extension;
        if ((!ver || strcmp(ver, gl_version) > 0)
            && ext && bugle_hash_get(&seen_extensions, ext) == &seen_extensions)
        {
            printf(" %s", ext);
            bugle_hash_set(&seen_extensions, ext, NULL);
        }
    }
    for (f = 0; f < NUMBER_OF_FUNCTIONS; f++)
    {
        const char *ext;

        ext = bugle_gl_function_table[f].extension;
        if (ext && bugle_hash_get(&seen_extensions, ext) == &seen_extensions)
        {
            printf(" %s", ext);
            bugle_hash_set(&seen_extensions, ext, NULL);
        }
    }

    bugle_hash_clear(&seen_extensions);
    printf("\n");
}

typedef struct
{
    bool capture;            /* Set to true when in feedback mode */
    size_t frame;
    FILE *stream;
} eps_struct;

static bugle_object_view eps_view;
static bool keypress_eps = false;
static xevent_key key_eps = { XK_W, ControlMask | ShiftMask | Mod1Mask, true };
static char *eps_filename = NULL;
static char *eps_title = NULL;
static bool eps_bsp = false;
static long eps_feedback_size = 0x100000;

static void initialise_eps_context(const void *key, void *data)
{
    eps_struct *d;

    d = (eps_struct *) data;
    d->frame = 0;
    d->stream = NULL;
}

static bool eps_glXSwapBuffers(function_call *call, const callback_data *data)
{
    size_t frame;
    eps_struct *d;

    d = (eps_struct *) bugle_object_get_current_data(&bugle_context_class, eps_view);
    if (!d) return true;
    frame = d->frame++;
    if (d->capture)
    {
        if (bugle_begin_internal_render())
        {
            GLint status;

            status = gl2psEndPage();
            switch (status)
            {
            case GL2PS_OVERFLOW:
                fprintf(stderr, "Feedback buffer overflowed; size will be doubled (can be increased in configuration)\n");
                break;
            case GL2PS_NO_FEEDBACK:
                fprintf(stderr, "No primitives were generated!\n");
                break;
            case GL2PS_UNINITIALIZED:
                fprintf(stderr, "gl2ps was not initialised. This indicates a bug in bugle.\n");
                break;
            case GL2PS_ERROR:
                fprintf(stderr, "An unknown gl2ps error occurred.\n");
                break;
            case GL2PS_SUCCESS:
                break;
            }
            fclose(d->stream);
            d->capture = false;
            return false; /* Don't swap, since it isn't a real frame */
        }
        else
            fprintf(stderr, "Warning: glXSwapBuffers called from inside glBegin/glEnd\nSnapshot may be corrupted.\n");
    }
    else if (keypress_eps && bugle_begin_internal_render())
    {
        FILE *f;
        char *fname, *end;
        GLint format, status;
        GLfloat size;

        keypress_eps = false;

        fname = interpolate_filename(eps_filename, frame);
        end = fname + strlen(fname);
        format = GL2PS_EPS;
        if (strlen(fname) >= 3 && !strcmp(end - 3, ".ps")) format = GL2PS_PS;
        if (strlen(fname) >= 4 && !strcmp(end - 4, ".eps")) format = GL2PS_EPS;
        if (strlen(fname) >= 4 && !strcmp(end - 4, ".pdf")) format = GL2PS_PDF;
        if (strlen(fname) >= 4 && !strcmp(end - 4, ".svg")) format = GL2PS_SVG;
        f = fopen(eps_filename, "wb");
        if (!f)
        {
            free(fname);
            fprintf(stderr, "Cannot open %s\n", eps_filename);
            return true;
        }

        status = gl2psBeginPage(eps_title ? eps_title : "Unnamed scene", "bugle",
                                NULL, format,
                                eps_bsp ? GL2PS_BSP_SORT : GL2PS_SIMPLE_SORT,
                                GL2PS_NO_PS3_SHADING | GL2PS_OCCLUSION_CULL | GL2PS_USE_CURRENT_VIEWPORT,
                                GL_RGBA, 0, 0, 0, 0, 0, eps_feedback_size,
                                f, fname);
        if (status != GL2PS_SUCCESS)
        {
            fprintf(stderr, "gl2psBeginPage failed\n");
            fclose(f);
            free(fname);
            return true;
        }
        CALL_glGetFloatv(GL_POINT_SIZE, &size);
        gl2psPointSize(size);
        CALL_glGetFloatv(GL_LINE_WIDTH, &size);
        gl2psLineWidth(size);

        d->stream = f;
        d->capture = true;
        free(fname);
        bugle_end_internal_render("eps_glXSwapBuffers", true);
    }
    return true;
}

static bool eps_glPointSize(function_call *call, const callback_data *data)
{
    eps_struct *d;

    d = (eps_struct *) bugle_object_get_current_data(&bugle_context_class, eps_view);
    if (d && d->capture && bugle_begin_internal_render())
    {
        GLfloat size;
        CALL_glGetFloatv(GL_POINT_SIZE, &size);
        gl2psPointSize(size);
        bugle_end_internal_render("eps_glPointSize", true);
    }
    return true;
}

static bool eps_glLineWidth(function_call *call, const callback_data *data)
{
    eps_struct *d;

    d = (eps_struct *) bugle_object_get_current_data(&bugle_context_class, eps_view);
    if (d && d->capture && bugle_begin_internal_render())
    {
        GLfloat width;
        CALL_glGetFloatv(GL_LINE_WIDTH, &width);
        gl2psPointSize(width);
        bugle_end_internal_render("eps_glLineWidth", true);
    }
    return true;
}

static bool initialise_eps(filter_set *handle)
{
    filter *f;

    f = bugle_register_filter(handle, "eps_pre");
    bugle_register_filter_catches(f, GROUP_glXSwapBuffers, false, eps_glXSwapBuffers);
    f = bugle_register_filter(handle, "eps");
    bugle_register_filter_catches(f, GROUP_glPointSize, false, eps_glPointSize);
    bugle_register_filter_catches(f, GROUP_glLineWidth, false, eps_glLineWidth);
    bugle_register_filter_depends("invoke", "eps_pre");
    bugle_register_filter_depends("eps", "invoke");
    bugle_register_filter_post_renders("eps");
    eps_view = bugle_object_class_register(&bugle_context_class, initialise_eps_context,
                                           NULL, sizeof(eps_struct));
    bugle_register_xevent_key(&key_eps, NULL, bugle_xevent_key_callback_flag, &keypress_eps);
    return true;
}

void bugle_initialise_filter_library(void)
{
    static const filter_set_variable_info screenshot_variables[] =
    {
        { "video", "record a video", FILTER_SET_VARIABLE_BOOL, &video, NULL },
        { "filename", "file to write video/screenshot to [/tmp/bugle.avi | /tmp/bugle.ppm]", FILTER_SET_VARIABLE_STRING, &video_filename, NULL },
        { "codec", "video codec to use [mpeg4]", FILTER_SET_VARIABLE_STRING, &video_codec, NULL },
        { "bitrate", "video bitrate (bytes/s) [7.5MB/s]", FILTER_SET_VARIABLE_POSITIVE_INT, &video_bitrate, NULL },
        { "allframes", "capture every frame, ignoring framerate [no]", FILTER_SET_VARIABLE_BOOL, &video_sample_all, NULL },
        { "lag", "length of capture pipeline (set higher for better throughput) [1]", FILTER_SET_VARIABLE_POSITIVE_INT, &video_lag, NULL },
        { "key_screenshot", "key to take a screenshot [C-A-S-S]", FILTER_SET_VARIABLE_KEY, &key_screenshot, NULL },
        { NULL, NULL, 0, NULL, NULL }
    };

    static const filter_set_info screenshot_info =
    {
        "screenshot",
        initialise_screenshot,
        destroy_screenshot,
        NULL,
        NULL,
        screenshot_variables,
        0,
        "captures screenshots every frame, or a video clip"
    };

    static const filter_set_info showextensions_info =
    {
        "showextensions",
        initialise_showextensions,
        destroy_showextensions,
        NULL,
        NULL,
        NULL,
        0,
        "reports extensions used at program termination"
    };

    static const filter_set_variable_info eps_variables[] =
    {
        { "filename", "file to write to (extension determines format) [/tmp/bugle.eps]", FILTER_SET_VARIABLE_STRING, &eps_filename, NULL },
        { "key_eps", "key to trigger snapshot [C-A-S-W]", FILTER_SET_VARIABLE_KEY, &key_eps, NULL },
        { "title", "title in EPS file [Unnamed scene]", FILTER_SET_VARIABLE_STRING, &eps_title, NULL },
        { "bsp", "use BSP sorting (slower but more accurate) [no]", FILTER_SET_VARIABLE_BOOL, &eps_bsp, NULL },
        { "buffer", "feedback buffer size [1M]", FILTER_SET_VARIABLE_UINT, &eps_feedback_size, NULL },
        { NULL, NULL, 0, NULL, NULL }
    };

    static const filter_set_info eps_info =
    {
        "eps",
        initialise_eps,
        NULL,
        NULL,
        NULL,
        eps_variables,
        0,
        "dumps scene to EPS/PS/PDF/SVG format"
    };

    video_codec = bugle_strdup("mpeg4");
    eps_filename = bugle_strdup("/tmp/bugle.eps");

    bugle_register_filter_set(&screenshot_info);
    bugle_register_filter_set(&showextensions_info);
    bugle_register_filter_set(&eps_info);

    bugle_register_filter_set_renders("screenshot");
    bugle_register_filter_set_depends("screenshot", "trackextensions");
    bugle_register_filter_set_renders("eps");
    bugle_register_filter_set_depends("eps", "trackcontext");
}
