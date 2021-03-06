/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2008, 2010-2011  Bruce Merry
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
#include <bugle/bool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <math.h>
#include <bugle/gl/glheaders.h>
#include <bugle/glwin/glwin.h>
#include <bugle/glwin/trackcontext.h>
#include <bugle/gl/glutils.h>
#include <bugle/gl/glextensions.h>
#include <bugle/hashtable.h>
#include <bugle/filters.h>
#include <bugle/apireflect.h>
#include <bugle/input.h>
#include <bugle/log.h>
#include <bugle/memory.h>
#include <bugle/string.h>
#include <bugle/time.h>
#include <budgie/addresses.h>
#include <budgie/reflect.h>

#if HAVE_LAVC
# include <inttypes.h>
# if HAVE_LIBAVCODEC_AVCODEC_H
#  include <libavcodec/avcodec.h>
# else
#  include <avcodec.h>
# endif
# if HAVE_LIBAVFORMAT_AVFORMAT_H
#  include <libavformat/avformat.h>
# else
#  include <avformat.h>
# endif
# if HAVE_LIBSWSCALE
#  if HAVE_LIBSWSCALE_SWSCALE_H
#   include <libswscale/swscale.h>
#  else
#   include <swscale.h>
#  endif
# endif
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
    bugle_bool pbo_mapped;       /* BUGLE_TRUE during glMapBuffer/glUnmapBuffer */
    int multiplicity;      /* number of times to write to video stream */
} screenshot_data;

/* Data that must be kept while in screenshot code, to allow restoration.
 * It is not directly related to an OpenGL context.
 */
typedef struct
{
    glwin_context old_context;
    glwin_drawable old_read, old_write;
} screenshot_context;

/* General settings */
static bugle_bool video = BUGLE_FALSE;
static char *video_filename = NULL;
/* Still settings */
static bugle_input_key key_screenshot = { BUGLE_INPUT_NOSYMBOL, 0, BUGLE_TRUE };
/* Video settings */
static char *video_codec = NULL;
static bugle_bool video_sample_all = BUGLE_FALSE;
static long video_bitrate = 7500000;
static long video_lag = 1;     /* latency between readpixels and encoding */

/* General data */
static int video_cur;  /* index of the next circular queue index to capture into */
static bugle_bool video_first;
static screenshot_data *video_data;
/* Still data */
static bugle_bool keypress_screenshot = BUGLE_FALSE;
/* Video data */
static FILE *video_pipe = NULL;  /* Used for ppmtoy4m */
static bugle_bool video_done = BUGLE_FALSE;
static double video_frame_time = 0.0;
static double video_frame_step = 1.0 / 30.0; /* FIXME: depends on frame rate */

static char *interpolate_filename(const char *pattern, int frame)
{
    if (strchr(pattern, '%'))
    {
        return bugle_asprintf(pattern, frame);
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
                                    int align, bugle_bool use_pbo)
{
    size_t stride;

    stride = width * CAPTURE_GL_ELEMENTS;
    stride = (stride + align - 1) & ~(align - 1);
    if ((!data->pixels && !data->pbo)
        || data->width != width
        || data->height != height
        || data->stride != stride)
    {
        if (data->pixels) bugle_free(data->pixels);
#ifdef GL_EXT_pixel_buffer_object
        if (data->pbo) CALL(glDeleteBuffersARB)(1, &data->pbo);
#endif
        data->width = width;
        data->height = height;
        data->stride = stride;
#ifdef GL_EXT_pixel_buffer_object
        if (use_pbo && BUGLE_GL_HAS_EXTENSION(GL_EXT_pixel_buffer_object))
        {
            CALL(glGenBuffersARB)(1, &data->pbo);
            CALL(glBindBufferARB)(GL_PIXEL_PACK_BUFFER_EXT, data->pbo);
            CALL(glBufferDataARB)(GL_PIXEL_PACK_BUFFER_EXT, stride * height,
                                 NULL, GL_DYNAMIC_READ_ARB);
            CALL(glBindBufferARB)(GL_PIXEL_PACK_BUFFER_EXT, 0);
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
 * context. If screenshot_start returns BUGLE_FALSE, do not continue.
 *
 * The argument must point to a structure which screenshot_start will
 * populate with data that should then be passed to screenshot_stop.
 */
static bugle_bool screenshot_start(screenshot_context *ssctx)
{
    glwin_context aux;
    glwin_display dpy;

    ssctx->old_context = bugle_glwin_get_current_context();
    ssctx->old_write = bugle_glwin_get_current_drawable();
    ssctx->old_read = bugle_glwin_get_current_read_drawable();
    dpy = bugle_glwin_get_current_display();
    aux = bugle_get_aux_context(BUGLE_FALSE);
    if (!aux) return BUGLE_FALSE;
    if (!bugle_gl_begin_internal_render())
    {
        bugle_log("screenshot", "grab", BUGLE_LOG_NOTICE,
                  "swap_buffers called inside begin/end; skipping frame");
        return BUGLE_FALSE;
    }
    bugle_glwin_make_context_current(dpy, ssctx->old_write, ssctx->old_write, aux);
    return BUGLE_TRUE;
}

static void screenshot_stop(screenshot_context *ssctx)
{
    glwin_display dpy;

    dpy = bugle_glwin_get_current_display();
    bugle_glwin_make_context_current(dpy, ssctx->old_write, ssctx->old_read, ssctx->old_context);
}

/* FIXME: we do not currently free the PBO, since we have no way of knowing
 * whether we are in the right context, or for that matter if the context
 * has been deleted.
 */
static void free_screenshot_data(screenshot_data *data)
{
    if (data->pixels) bugle_free(data->pixels);
}

#if HAVE_LAVC
static AVFormatContext *video_context = NULL;
static AVStream *video_stream;
static AVFrame *video_raw, *video_yuv;
static uint8_t *video_buffer;
static size_t video_buffer_size = 2000000; /* FIXME: what should it be? */
#if HAVE_LIBSWSCALE
static struct SwsContext *sws_context = NULL;
#endif

static AVFrame *allocate_video_frame(int fmt, int width, int height,
                                     bugle_bool create)
{
    AVFrame *f;
    size_t size;
    void *buffer = NULL;

    f = avcodec_alloc_frame();
    if (!f)
    {
        bugle_log("screenshot", "video", BUGLE_LOG_ERROR,
                  "failed to allocate frame");
        exit(1);
    }
    size = avpicture_get_size(fmt, width, height);
    if (create) buffer = bugle_malloc(size);
    avpicture_fill((AVPicture *) f, buffer, fmt, width, height);
    return f;
}

static bugle_bool lavc_initialise(int width, int height)
{
    AVOutputFormat *fmt;
    AVCodecContext *c;
    AVCodec *codec;

    av_register_all();
#if LIBAVFORMAT_VERSION_INT >= 0x00342D00 /* Major 52, minor 45 */
    fmt = av_guess_format(NULL, video_filename, NULL);
    if (!fmt)
        fmt = av_guess_format("avi", NULL, NULL);
#else
    fmt = guess_format(NULL, video_filename, NULL);
    if (!fmt)
        fmt = guess_format("avi", NULL, NULL);
#endif
    if (!fmt)
        return BUGLE_FALSE;
    video_context = avformat_alloc_context();
    if (!video_context)
        return BUGLE_FALSE;
    video_context->oformat = fmt;
    snprintf(video_context->filename,
             sizeof(video_context->filename), "%s", video_filename);
    video_stream = avformat_new_stream(video_context, NULL);
    if (!video_stream)
        return BUGLE_FALSE;
    video_stream->id = 0; /* FIXME: what does this parameter do? */
    codec = avcodec_find_encoder_by_name(video_codec);
    if (!codec) codec = avcodec_find_encoder(CODEC_ID_HUFFYUV);
    if (!codec)
        return BUGLE_FALSE;
    c = video_stream->codec;
#if LIBAVFORMAT_BUILD < 4621
    c->codec_type = CODEC_TYPE_VIDEO;
#else
    c->codec_type = AVMEDIA_TYPE_VIDEO;
#endif
    c->codec_id = codec->id;
    if (c->codec_id == CODEC_ID_HUFFYUV)
        c->pix_fmt = PIX_FMT_YUV422P;
    else
        c->pix_fmt = PIX_FMT_YUV420P;
    if (c->codec_id == CODEC_ID_FFV1)
        c->strict_std_compliance = -1;
    c->bit_rate = video_bitrate;
    c->width = width;
    c->height = height;
    c->time_base.den = 30;
    c->time_base.num = 1;
    c->gop_size = 12;     /* FIXME: user should specify */
    if (avcodec_open2(c, codec, NULL) < 0)
        return BUGLE_FALSE;
    video_buffer = bugle_malloc(video_buffer_size);
    video_raw = allocate_video_frame(CAPTURE_AV_FMT, width, height, BUGLE_FALSE);
    video_yuv = allocate_video_frame(c->pix_fmt, width, height, BUGLE_TRUE);
#if LIBAVFORMAT_VERSION_INT >= 0x00350000 /* major of 53 */
    if (avio_open(&video_context->pb, video_filename, AVIO_FLAG_WRITE) < 0)
#else
    if (url_fopen(&video_context->pb, video_filename, URL_WRONLY) < 0)
#endif
    {
        bugle_log_printf("screenshot", "video", BUGLE_LOG_ERROR,
                         "failed to open video output file %s", video_filename);
        exit(1);
    }
#if LIBAVFORMAT_VERSION_INT >= 0x00350200 /* major 53, minor 2 */
    avformat_write_header(video_context, NULL);
#else
    av_write_header(video_context);
#endif
    return BUGLE_TRUE;
}

static void lavc_shutdown(void)
{
    int i;
    AVCodecContext *c;
    size_t out_size;

    c = video_stream->codec;
    /* Write any delayed frames. */
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
            {
#if LIBAVFORMAT_BUILD < 4621
                pkt.flags |= PKT_FLAG_KEY;
#else
                pkt.flags |= AV_PKT_FLAG_KEY;
#endif
            }
            pkt.stream_index = video_stream->index;
            pkt.data = video_buffer;
            pkt.size = out_size;
            ret = av_write_frame(video_context, &pkt);
            if (ret != 0)
            {
                bugle_log("screenshot", "video", BUGLE_LOG_ERROR,
                          "encoding failed");
                exit(1);
            }
        }
    } while (out_size);

    /* Close it all down */
    av_write_trailer(video_context);
    avcodec_close(video_stream->codec);
    av_free(video_yuv->data[0]);
    /* We don't free video_raw, since that memory belongs to video_data */
    av_free(video_yuv);
    av_free(video_raw);
    av_free(video_buffer);
    for (i = 0; i < (int) video_context->nb_streams; i++)
        av_freep(&video_context->streams[i]);
#if LIBAVFORMAT_VERSION_INT >= 0x00350000 /* major of 53 */
    avio_close(video_context->pb);
#elif LIBAVFORMAT_VERSION_INT >= 0x00340000 /* major of 52 */
    url_fclose(video_context->pb);
#else
    url_fclose(&video_context->pb);
#endif
    av_free(video_context);

    for (i = 0; i < video_lag; i++)
        free_screenshot_data(&video_data[i]);
    bugle_free(video_data);
#if HAVE_LIBSWSCALE
    sws_freeContext(sws_context);
#endif

    video_context = NULL;
}

#endif /* HAVE_LAVC */

/* There are three use cases:
 * 1. There is no PBO. There is nothing to do.
 * 2. Use glMapBufferARB. In this case, data->pixels is NULL on entry, and
 * we set it to the mapped value and set pbo_mapped to BUGLE_TRUE. If glMapBufferARB
 * fails, we allocate system memory and fall back to case 3.
 * 3. We use glGetBufferSubDataARB. In this case, data->pixels is non-NULL on
 * entry and of the correct size, and we read straight into it and set
 * pbo_mapped to BUGLE_FALSE.
 */
static bugle_bool map_screenshot(screenshot_data *data)
{
#ifdef GL_EXT_pixel_buffer_object
    GLint size = 0;
    if (data->pbo)
    {
        CALL(glBindBufferARB)(GL_PIXEL_PACK_BUFFER_EXT, data->pbo);

        if (!data->pixels)
        {
            data->pixels = CALL(glMapBufferARB)(GL_PIXEL_PACK_BUFFER_EXT, GL_READ_ONLY_ARB);
            if (!data->pixels)
                CALL(glGetError)(); /* hide the error from end_internal_render() */
            else
            {
                data->pbo_mapped = BUGLE_TRUE;
                bugle_gl_end_internal_render("map_screenshot", BUGLE_TRUE);
                CALL(glBindBufferARB)(GL_PIXEL_PACK_BUFFER_EXT, 0);
                return BUGLE_TRUE;
            }
        }
        /* If we get here, we're in case 3 */
        CALL(glGetBufferParameterivARB)(GL_PIXEL_PACK_BUFFER_EXT, GL_BUFFER_SIZE_ARB, &size);
        if (!data->pixels)
            data->pixels = bugle_malloc(size);
        CALL(glGetBufferSubDataARB)(GL_PIXEL_PACK_BUFFER_EXT, 0, size, data->pixels);
        data->pbo_mapped = BUGLE_FALSE;
        CALL(glBindBufferARB)(GL_PIXEL_PACK_BUFFER_EXT, 0);
        bugle_gl_end_internal_render("map_screenshot", BUGLE_TRUE);
    }
#endif
    return BUGLE_TRUE;
}

static bugle_bool unmap_screenshot(screenshot_data *data)
{
#ifdef GL_EXT_pixel_buffer_object
    if (data->pbo && data->pbo_mapped)
    {
        bugle_bool ret;

        CALL(glBindBufferARB)(GL_PIXEL_PACK_BUFFER_EXT, data->pbo);
        ret = CALL(glUnmapBufferARB)(GL_PIXEL_PACK_BUFFER_EXT);
        CALL(glBindBufferARB)(GL_PIXEL_PACK_BUFFER_EXT, 0);
        bugle_gl_end_internal_render("unmap_screenshot", BUGLE_TRUE);
        data->pixels = NULL;
        return ret;
    }
    else
#endif
    {
        return BUGLE_TRUE;
    }
}

static bugle_bool do_screenshot(GLenum format, int test_width, int test_height,
                          screenshot_data **data)
{
    glwin_drawable drawable;
    glwin_display dpy;
    screenshot_data *cur;
    int width, height;

    *data = &video_data[(video_cur + video_lag - 1) % video_lag];
    cur = &video_data[video_cur];
    video_cur = (video_cur + 1) % video_lag;

    drawable = bugle_glwin_get_current_drawable();
    dpy = bugle_glwin_get_current_display();
    bugle_glwin_get_drawable_dimensions(dpy, drawable, &width, &height);
    if (test_width != -1 || test_height != -1)
        if (width != test_width || height != test_height)
        {
            bugle_log_printf("screenshot", "video", BUGLE_LOG_WARNING,
                             "size changed from %dx%d to %dx%d, stopping recording",
                             test_width, test_height, width, height);
            return BUGLE_FALSE;
        }

    prepare_screenshot_data(cur, width, height, 4, BUGLE_TRUE);

    if (!bugle_gl_begin_internal_render()) return BUGLE_FALSE;
#ifdef GL_EXT_pixel_buffer_object
    if (cur->pbo)
        CALL(glBindBufferARB)(GL_PIXEL_PACK_BUFFER_EXT, cur->pbo);
#endif
    CALL(glReadPixels)(0, 0, width, height, format,
                      GL_UNSIGNED_BYTE, cur->pbo ? NULL : cur->pixels);
#ifdef GL_EXT_pixel_buffer_object
    if (cur->pbo)
        CALL(glBindBufferARB)(GL_PIXEL_PACK_BUFFER_EXT, 0);
#endif
    bugle_gl_end_internal_render("do_screenshot", BUGLE_TRUE);

    return BUGLE_TRUE;
}

static bugle_bool screenshot_stream(FILE *out, bugle_bool check_size)
{
    screenshot_data *fetch;
    GLubyte *cur;
    size_t size, count;
    bugle_bool ret = BUGLE_TRUE;
    int i;

    if (check_size && !video_first)
        video_done = !do_screenshot(GL_RGB, video_data[0].width, video_data[0].height, &fetch);
    else
        do_screenshot(GL_RGB, -1, -1, &fetch);
    video_first = BUGLE_FALSE;

    if (fetch->width > 0)
    {
        if (!map_screenshot(fetch)) return BUGLE_FALSE;
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
                ret = BUGLE_FALSE;
                break;
            }
            cur -= fetch->stride;
        }
        unmap_screenshot(fetch);
    }
    return ret;
}

#if HAVE_LAVC
static void screenshot_video(void)
{
    screenshot_data *fetch;
    AVCodecContext *c;
    size_t out_size;
    int i, ret;
    bugle_timespec tv;
    double t = 0.0;
    screenshot_context ssctx;

    if (!video_sample_all)
    {
        bugle_gettime(&tv);
        t = tv.tv_sec + 1e-9 * tv.tv_nsec;
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
    video_first = BUGLE_FALSE;

    if (fetch->width > 0)
    {
        if (!video_context)
            lavc_initialise(fetch->width, fetch->height);
        c = video_stream->codec;
        if (!map_screenshot(fetch))
        {
            screenshot_stop(&ssctx);
            return;
        }
        video_raw->data[0] = fetch->pixels + fetch->stride * (fetch->height - 1);
        video_raw->linesize[0] = -fetch->stride;
#if HAVE_LIBSWSCALE
        sws_context = sws_getCachedContext(sws_context,
                                           fetch->width, fetch->height, CAPTURE_AV_FMT,
                                           fetch->width, fetch->height, c->pix_fmt,
                                           SWS_BILINEAR, NULL, NULL, NULL);
        sws_scale(sws_context, (const uint8_t * const *) video_raw->data, video_raw->linesize,
                  0, fetch->height, video_yuv->data, video_yuv->linesize);
#else

        img_convert((AVPicture *) video_yuv, c->pix_fmt,
                    (AVPicture *) video_raw, CAPTURE_AV_FMT,
                    fetch->width, fetch->height);
#endif
        for (i = 0; i < fetch->multiplicity; i++)
        {
            out_size = avcodec_encode_video(video_stream->codec,
                                            video_buffer, video_buffer_size,
                                            video_yuv);
            if (out_size != 0)
            {
                AVPacket pkt;

                av_init_packet(&pkt);
                pkt.pts = c->coded_frame->pts;
                if (c->coded_frame->key_frame)
                {
#if LIBAVFORMAT_BUILD < 4621
                    pkt.flags |= PKT_FLAG_KEY;
#else
                    pkt.flags |= AV_PKT_FLAG_KEY;
#endif
                }
                pkt.stream_index = video_stream->index;
                pkt.data = video_buffer;
                pkt.size = out_size;
                ret = av_write_frame(video_context, &pkt);
                if (ret != 0)
                {
                    bugle_log("screenshot", "video", BUGLE_LOG_ERROR, "encoding failed");
                    exit(1);
                }
            }
        }
        unmap_screenshot(fetch);
    }
    screenshot_stop(&ssctx);
}

#else /* !HAVE_LAVC */

static void screenshot_video(void)
{
    screenshot_context ssctx;
    if (!screenshot_start(&ssctx)) return;

    if (!screenshot_stream(video_pipe, BUGLE_TRUE))
    {
        /* TODO: abstract this code away somewhere */
#if defined(BUGLE_PLATFORM_POSIX)
        pclose(video_pipe);
#elif defined(BUGLE_PLATFORM_MSVCRT)
        _pclose(video_pipe);
#endif
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
    bugle_free(fname);
    if (!out)
    {
        perror("failed to open screenshot file");
        screenshot_stop(&ssctx);
        return;
    }
    screenshot_stream(out, BUGLE_FALSE);
    if (fclose(out) != 0)
        perror("write error");
    screenshot_stop(&ssctx);
}

bugle_bool screenshot_callback(function_call *call, const callback_data *data)
{
    /* FIXME: track the frameno in the context?
     */
    static int frameno = 0;

    if (video)
    {
        if (!video_done)
            screenshot_video();
    }
    else if (keypress_screenshot)
    {
        screenshot_file(frameno);
        keypress_screenshot = BUGLE_FALSE;
    }
    frameno++;
    return BUGLE_TRUE;
}

static bugle_bool screenshot_initialise(filter_set *handle)
{
    filter *f;
#if !HAVE_LAVC
    char *cmdline;
#endif

    f = bugle_filter_new(handle, "screenshot");
    bugle_glwin_filter_catches_swap_buffers(f, BUGLE_FALSE, screenshot_callback);
    bugle_filter_order("screenshot", "invoke");

    video_data = BUGLE_CALLOC(video_lag, screenshot_data);
    video_cur = 0;
    video_first = BUGLE_TRUE;
    if (video)
    {
        video_done = BUGLE_FALSE; /* becomes BUGLE_TRUE if we resize */
        if (!video_filename)
            video_filename = bugle_strdup("bugle.avi");
#if !HAVE_LAVC
        cmdline = bugle_asprintf("ppmtoy4m | ffmpeg -f yuv4mpegpipe -i - -vcodec %s -strict -1 -y %s",
                                 video_codec, video_filename);
#if defined(BUGLE_PLATFORM_POSIX)
        video_pipe = popen(cmdline, "w");
#elif defined(BUGLE_PLATFORM_MSVCRT)
        video_pipe = _popen(cmdline, "w");
#else
        bugle_log("screenshot", "init", BUGLE_LOG_ERROR,
                  "Video capture by pipe not supported on this platform");
#endif

        bugle_free(cmdline);
        if (!video_pipe) return BUGLE_FALSE;
#endif /* !HAVE_LAVC */
        /* Note: we only initialise libavcodec on the first frame, because
         * we need the frame size.
         */
    }
    else
    {
        if (!video_filename)
            video_filename = bugle_strdup("bugle.ppm");
        video_lag = 1;
        /* FIXME: should only intercept the key when enabled */
        bugle_input_key_callback(&key_screenshot, NULL, bugle_input_key_callback_flag, &keypress_screenshot);
    }
    return BUGLE_TRUE;
}

static void screenshot_shutdown(filter_set *handle)
{
#if HAVE_LAVC
    if (video_context)
        lavc_shutdown();
#endif
    if (video_pipe)
    {
#if defined(BUGLE_PLATFORM_POSIX)
        pclose(video_pipe);
#elif defined(BUGLE_PLATFORM_MSVCRT)
        _pclose(video_pipe);
#endif
    }
    if (video_codec) bugle_free(video_codec);
}

void bugle_initialise_filter_library(void)
{
    static const filter_set_variable_info screenshot_variables[] =
    {
        { "video", "record a video", FILTER_SET_VARIABLE_BOOL, &video, NULL },
        { "filename", "file to write video/screenshot to [bugle.avi | bugle.ppm]", FILTER_SET_VARIABLE_STRING, &video_filename, NULL },
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
        screenshot_initialise,
        screenshot_shutdown,
        NULL,
        NULL,
        screenshot_variables,
        "captures screenshots or a video clip"
    };

    video_codec = bugle_strdup("mpeg4");
    bugle_input_key_lookup("C-A-S-S", &key_screenshot);

    bugle_filter_set_new(&screenshot_info);

    bugle_gl_filter_set_renders("screenshot");
    bugle_filter_set_depends("screenshot", "trackcontext");
    bugle_filter_set_depends("screenshot", "glextensions");
}
