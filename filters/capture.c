/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004-2006  Bruce Merry
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
# include <ffmpeg/avcodec.h>
# include <ffmpeg/avformat.h>
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
    int multiplicity;      /* number of times to write to video stream */
} screenshot_data;

/* General settings */
static bool video = false;
static char *video_filename = NULL;
/* Still settings */
static xevent_key key_screenshot = { XK_S, ControlMask | ShiftMask | Mod1Mask };
/* Video settings */
static char *video_codec = NULL;
static bool video_sample_all = false;
static long video_bitrate = 7500000;
static long video_lag = 1;     /* latency between readpixels and encoding */
static xevent_key key_video_start = { NoSymbol, 0 };
static xevent_key key_video_stop = { XK_E, ControlMask | ShiftMask | Mod1Mask };

/* General data */
static int video_cur;  /* index of the next circular queue index to capture into */
static bool video_first;
static screenshot_data *video_data;
/* Still data */
static bool keypress_screenshot = false;
/* Video data */
static FILE *video_pipe = NULL;  /* Used for ppmtoy4m */
static bool video_start = false, video_done = false;
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
#if LIBAVFORMAT_VERSION_INT >= 0x00320000  /* Version 50? */
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
#if LIBAVFORMAT_VERSION_INT >= 0x00320000 /* Version 50? */
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

#if LIBAVFORMAT_VERSION_INT >= 0x00320000
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
#if LIBAVFORMAT_VERSION_INT >= 0x00320000
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

static bool map_screenshot(screenshot_data *data)
{
#ifdef GL_EXT_pixel_buffer_object
    if (data->pbo)
    {
        /* Mapping is done from the real context, so we must save the
         * old binding.
         */
        GLint old_id;

        if (!bugle_begin_internal_render())
        {
            fputs("warning: glXSwapBuffers called inside begin/end. Dropping frame\n", stderr);
            return false;
        }

        CALL_glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING_EXT, &old_id);
        CALL_glBindBufferARB(GL_PIXEL_PACK_BUFFER_EXT, data->pbo);
        data->pixels = CALL_glMapBuffer(GL_PIXEL_PACK_BUFFER_EXT, GL_READ_ONLY_ARB);
        if (!data->pixels)
        {
            CALL_glGetError(); /* hide the error from end_internal_render() */
            CALL_glBindBufferARB(GL_PIXEL_PACK_BUFFER_EXT, old_id);
            bugle_end_internal_render("map_screenshot", true);
            return false;
        }
        else
        {
            CALL_glBindBufferARB(GL_PIXEL_PACK_BUFFER_EXT, old_id);
            bugle_end_internal_render("map_screenshot", true);
        }
    }
#endif
    return true;
}

static bool unmap_screenshot(screenshot_data *data)
{
#ifdef GL_EXT_pixel_buffer_object
    if (data->pbo)
    {
        /* Mapping is done from the real context, so we must save the
         * old binding.
         */
        GLint old_id;
        bool ret;

        if (!bugle_begin_internal_render())
        {
            fputs("warning: glXSwapBuffers called inside begin/end. Dropping frame\n", stderr);
            return false;
        }

        CALL_glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING_EXT, &old_id);
        CALL_glBindBufferARB(GL_PIXEL_PACK_BUFFER_EXT, data->pbo);
        ret = CALL_glUnmapBufferARB(GL_PIXEL_PACK_BUFFER_EXT);
        CALL_glBindBufferARB(GL_PIXEL_PACK_BUFFER_EXT, old_id);
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
    GLXContext aux, real;
    GLXDrawable old_read, old_write;
    Display *dpy;
    screenshot_data *cur;
    unsigned int w, h;
    int width, height;

    *data = &video_data[(video_cur + video_lag - 1) % video_lag];
    cur = &video_data[video_cur];
    video_cur = (video_cur + 1) % video_lag;

    real = CALL_glXGetCurrentContext();
    old_write = CALL_glXGetCurrentDrawable();
    old_read = CALL_glXGetCurrentReadDrawable();
    dpy = CALL_glXGetCurrentDisplay();
    CALL_glXQueryDrawable(dpy, old_write, GLX_WIDTH, &w);
    CALL_glXQueryDrawable(dpy, old_write, GLX_HEIGHT, &h);
    width = w;
    height = h;
    if (test_width != -1 || test_height != -1)
        if (width != test_width || height != test_height)
        {
            fprintf(stderr, "size changed from %dx%d to %dx%d, stopping recording\n",
                    test_width, test_height, width, height);
            return false;
        }

    aux = bugle_get_aux_context();
    if (!aux) return false;
    if (!bugle_begin_internal_render())
    {
        fputs("warning: glXSwapBuffers called inside begin/end - corrupting frame\n", stderr);
        return true;
    }
    CALL_glXMakeContextCurrent(dpy, old_write, old_write, aux);

    prepare_screenshot_data(cur, width, height, 4, true);

    /* FIXME: if we set up all the glReadPixels state, it may be faster
     * due to not having to context switch. It's probably not worth
     * the risk due to unknown extensions though
     */
#ifdef GL_EXT_pixel_buffer_object
    if (cur->pbo)
        CALL_glBindBufferARB(GL_PIXEL_PACK_BUFFER_EXT, cur->pbo);
#endif
    CALL_glReadPixels(0, 0, width, height, format,
                      GL_UNSIGNED_BYTE, cur->pixels);
#ifdef GL_EXT_pixel_buffer_object
    if (cur->pbo)
        CALL_glBindBufferARB(GL_PIXEL_PACK_BUFFER_EXT, 0);
#endif
    CALL_glXMakeContextCurrent(dpy, old_write, old_read, real);
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
        map_screenshot(fetch);
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

    if (!video_first)
        video_done = !do_screenshot(CAPTURE_GL_FMT, video_data[0].width, video_data[0].height, &fetch);
    else
        do_screenshot(CAPTURE_GL_FMT, -1, -1, &fetch);
    video_first = false;

    if (fetch->width > 0)
    {
        if (!video_context)
            initialise_lavc(fetch->width, fetch->height);
#if LIBAVFORMAT_VERSION_INT >= 0x00320000
        c = video_stream->codec;
#else
        c = &video_stream->codec;
#endif
        map_screenshot(fetch);
        video_raw->data[0] = fetch->pixels + fetch->stride * (fetch->height - 1);
        video_raw->linesize[0] = -fetch->stride;
        img_convert((AVPicture *) video_yuv, c->pix_fmt,
                    (AVPicture *) video_raw, CAPTURE_AV_FMT,
                    fetch->width, fetch->height);
        for (i = 0; i < fetch->multiplicity; i++)
        {
#if LIBAVFORMAT_VERSION_INT >= 0x00320000
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

    fname = interpolate_filename(video_filename, frameno);
    out = fopen(fname, "wb");
    free(fname);
    if (!out)
    {
        perror("failed to open screenshot file");
        return;
    }
    if (!screenshot_stream(out, false)) return;
    if (fclose(out) != 0)
        perror("write error");
}

bool screenshot_callback(function_call *call, const callback_data *data)
{
    /* FIXME: track the frameno in the context?
     */
    static int frameno = 0;

    if (video)
    {
        if (video_start && !video_done)
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
        video_start = false;
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
        if (key_video_start.keysym != NoSymbol)
            bugle_register_xevent_key(&key_video_start, NULL, bugle_xevent_key_callback_flag, &video_start);
        else
            video_start = true;
        bugle_register_xevent_key(&key_video_stop, NULL, bugle_xevent_key_callback_flag, &video_done);
    }
    else
    {
        if (!video_filename)
            video_filename = bugle_strdup("/tmp/bugle.ppm");
        video_lag = 1;
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

    info = &budgie_group_table[call->generic.id];
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
    GLfloat *feedback;
    GLsizei feedback_size;   /* number of GLfloats, not bytes */
} epswire_struct;

static bugle_object_view epswire_view;
static bool keypress_epswire = false;
static xevent_key key_epswire = { XK_W, ControlMask | ShiftMask | Mod1Mask };
static char *epswire_filename = "/tmp/bugle.eps";

static void initialise_epswire_context(const void *key, void *data)
{
    epswire_struct *d = (epswire_struct *) data;
    d->frame = 0;
    d->feedback_size = 1 << 20; /* 1 MB */
    d->feedback = bugle_malloc(d->feedback_size * sizeof(GLfloat));
}

static void epswire_adjustboundingbox(GLfloat *p,
                                      GLfloat *x1, GLfloat *y1,
                                      GLfloat *x2, GLfloat *y2)
{
    if (p[0] < *x1) *x1 = p[0];
    if (p[0] > *x2) *x2 = p[0];
    if (p[1] < *y1) *y1 = p[1];
    if (p[1] > *y2) *y2 = p[1];
}

static void epswire_boundingbox(GLfloat *buffer, GLint entries,
                                GLfloat *x1, GLfloat *y1,
                                GLfloat *x2, GLfloat *y2)
{
    GLint pos, i, n;
    GLfloat coord[2];
    GLenum token;
    *x1 = 1e20;
    *y1 = 1e20;
    *x2 = 0.0f;
    *y2 = 0.0f;

    pos = 0;
    while (pos < entries)
    {
        token = (GLenum) buffer[pos++];
        switch (token)
        {
        case GL_PASS_THROUGH_TOKEN:
            pos++;
            break;
        case GL_POINT_TOKEN:
        case GL_BITMAP_TOKEN:
        case GL_DRAW_PIXEL_TOKEN:
        case GL_COPY_PIXEL_TOKEN:
            pos += 2;
            break;
        case GL_LINE_TOKEN:
        case GL_LINE_RESET_TOKEN:
            for (i = 0; i < 2; i++)
            {
                coord[0] = buffer[pos++];
                coord[1] = buffer[pos++];
                epswire_adjustboundingbox(coord, x1, y1, x2, y2);
            }
            break;
        case GL_POLYGON_TOKEN:
            n = (GLint) buffer[pos++];
            for (i = 0; i < n; i++)
            {
                coord[0] = buffer[pos++];
                coord[1] = buffer[pos++];
                epswire_adjustboundingbox(coord, x1, y1, x2, y2);
            }
            break;
        }
    }
}

static void epswire_dumpfeedback(FILE *f, GLfloat *buffer, GLint entries)
{
    GLint pos, i, n;
    GLfloat coord[4];
    GLenum token;

    pos = 0;
    while (pos < entries)
    {
        token = (GLenum) buffer[pos++];
        switch (token)
        {
        case GL_PASS_THROUGH_TOKEN:
            pos++;
            break;
        case GL_POINT_TOKEN:
        case GL_BITMAP_TOKEN:
        case GL_DRAW_PIXEL_TOKEN:
        case GL_COPY_PIXEL_TOKEN:
            pos += 2;
            break;
        case GL_LINE_TOKEN:
        case GL_LINE_RESET_TOKEN:
            for (i = 0; i < 4; i++)
                coord[i] = buffer[pos++];
            fprintf(f, "newpath\n"
                    "%.6f %.6f moveto\n"
                    "%.6f %.6f lineto\n"
                    "stroke\n",
                    coord[0], coord[1], coord[2], coord[3]);
            break;
        case GL_POLYGON_TOKEN:
            n = (GLint) buffer[pos++];
            fputs("newpath\n", f);
            for (i = 0; i < n; i++)
            {
                coord[0] = buffer[pos++];
                coord[1] = buffer[pos++];
                fprintf(f, "%.6f %.6f %s\n",
                        coord[0], coord[1],
                        i ? "lineto" : "moveto");
            }
            fputs("closepath\nstroke\n", f);
        }
    }
}

static bool epswire_glXSwapBuffers(function_call *call, const callback_data *data)
{
    GLint entries;
    size_t frame;

    epswire_struct *d = (epswire_struct *) bugle_object_get_current_data(&bugle_context_class, epswire_view);
    if (!d) return true;
    frame = d->frame++;
    if (d->capture)
    {
        /* Captured frame; dump the EPS file and return to render mode */
        entries = glRenderMode(GL_RENDER);
        if (entries < 0)
        {
            /* Overflow - reallocate and better luck next time */
            d->feedback_size *= 2;
            d->feedback = bugle_realloc(d->feedback, d->feedback_size * sizeof(GLfloat));
            fputs("Feedback buffer overflowed. Will try again on next frame.\n", stderr);
        }
        else
        {
            FILE *f;
            char *fname;
            GLfloat x1, y1, x2, y2;

            fname = interpolate_filename(epswire_filename, frame);
            f = fopen(epswire_filename, "w");
            free(fname);
            if (f)
            {
                epswire_boundingbox(d->feedback, entries, &x1, &y1, &x2, &y2);
                fputs("%%!PS-Adobe-2.0 EPSF-1.2\n", f);
                fprintf(f, "%%%%BoundingBox: %.3f %.3f %.3f %.3f\n",
                        x1 - 1.0, y1 - 1.0, x2 + 1.0, y2 + 1.0);
                fputs("%%%%EndComments\ngsave\n1 setlinecap\n1 setlinejoin\n", f);
                epswire_dumpfeedback(f, d->feedback, entries);
                fputs("grestore\n", f);
                fclose(f);
            }
            d->capture = false;
        }
        return false; /* Don't swap, since it isn't a real frame */
    }
    else if (keypress_epswire)
    {
        keypress_epswire = false;
        glFeedbackBuffer(d->feedback_size, GL_2D, d->feedback);
        glRenderMode(GL_FEEDBACK);
        d->capture = true;
    }
    return true;
}

static bool epswire_glEnable(function_call *call, const callback_data *data)
{
    /* FIXME: dirty hack. This should be a post-call, should track the
     * value during EPS frames and restore the value for real frames.*/
    switch (*call->typed.glEnable.arg0)
    {
    case GL_CULL_FACE:
        return false;
    }
    return true;
}

static bool initialise_epswire(filter_set *handle)
{
    filter *f;

    f = bugle_register_filter(handle, "epswire");
    bugle_register_filter_catches(f, GROUP_glXSwapBuffers, false, epswire_glXSwapBuffers);
    bugle_register_filter_catches(f, GROUP_glEnable, false, epswire_glEnable);
    bugle_register_filter_depends("invoke", "epswire");
    epswire_view = bugle_object_class_register(&bugle_context_class, initialise_epswire_context,
                                               NULL, sizeof(epswire_struct));
    bugle_register_xevent_key(&key_epswire, NULL, bugle_xevent_key_callback_flag, &keypress_epswire);
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
        { "key_start", "key to start video encoding [autostart]", FILTER_SET_VARIABLE_KEY, &key_video_start, NULL },
        { "key_stop", "key to end video recording [C-A-S-E]", FILTER_SET_VARIABLE_KEY, &key_video_stop, NULL },
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

    static const filter_set_variable_info epswire_variables[] =
    {
        { "filename", "file to write to [/tmp/bugle.eps]", FILTER_SET_VARIABLE_STRING, &epswire_filename, NULL },
        { "key_epswire", "key to trigger EPS snapshot [C-A-S-W]", FILTER_SET_VARIABLE_KEY, &key_epswire },
        { NULL, NULL, 0, NULL, NULL }
    };

    static const filter_set_info epswire_info =
    {
        "epswire",
        initialise_epswire,
        NULL,
        NULL,
        NULL,
        epswire_variables,
        0,
        "dumps wireframes to postscript format"
    };

    video_codec = bugle_strdup("mpeg4");

    bugle_register_filter_set(&screenshot_info);
    bugle_register_filter_set(&showextensions_info);
    bugle_register_filter_set(&epswire_info);

    bugle_register_filter_set_renders("screenshot");
    bugle_register_filter_set_depends("screenshot", "trackextensions");
    bugle_register_filter_set_depends("epswire", "trackcontext");
}
