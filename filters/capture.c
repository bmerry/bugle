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

/* I have no idea what I'm doing with the video encoding, since
 * libavformat has no documentation that I can find. It is probably full
 * of bugs.
 *
 * This file is also blatantly unsafe to use with multiple contexts.
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "src/filters.h"
#include "src/utils.h"
#include "src/canon.h"
#include "src/glutils.h"
#include "src/glfuncs.h"
#include "src/glexts.h"
#include "src/tracker.h"
#include "common/hashtable.h"
#include "common/safemem.h"
#include "common/bool.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
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

static char *file_base = "frame";
static const char *file_suffix = ".ppm";
static char *video_codec = NULL;
static bool video_sample_all = false;
static long video_bitrate = 7500000;
static long start_frameno = 0;
static long video_lag = 1;     /* greater lag may give better overall speed */

static char *video_file = NULL;
static FILE *video_pipe = NULL;
static screenshot_data *video_data;
/* video_cur is the index of the next circular queue index to capture into,
 * and also to read back from first if appropriate. video_leader starts at
 * video_lag, and is decremented on each frame.
 */
static int video_cur, video_leader;
static bool video_done;
static double video_frame_time = 0.0;
static double video_frame_step = 1.0 / 30.0; /* FIXME: depends on frame rate */

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
    fmt = guess_format(NULL, video_file, NULL);
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
    video_buffer = bugle_malloc(video_buffer_size);
    video_raw = allocate_video_frame(CAPTURE_AV_FMT, width, height, false);
    video_yuv = allocate_video_frame(c->pix_fmt, width, height, true);
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
        GLuint old_id;

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
        GLuint old_id;
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

static screenshot_data *start_screenshot(void)
{
    if (video_leader == 0
        && map_screenshot(&video_data[video_cur]))
        return &video_data[video_cur];
    else
        return NULL;
}

/* Returns false if width and height do not match test_width and test_height.
 * If these are both -1, then no test is performed (use -1,0 for a test
 * that will always fail).
 */
static bool end_screenshot(GLenum format, int test_width, int test_height)
{
    GLXContext aux, real;
    GLXDrawable old_read, old_write;
    Display *dpy;
    screenshot_data *cur;
    int width, height;

    cur = &video_data[video_cur];
    video_cur = (video_cur + 1) % video_lag;
    if (video_leader) video_leader--;
    if (cur->pbo && cur->pixels)
    {
        if (!unmap_screenshot(cur))
            fputs("warning: buffer data was lost - corrupting frame\n", stderr);
    }

    real = CALL_glXGetCurrentContext();
    old_write = CALL_glXGetCurrentDrawable();
    old_read = CALL_glXGetCurrentReadDrawable();
    dpy = CALL_glXGetCurrentDisplay();
    CALL_glXQueryDrawable(dpy, old_write, GLX_WIDTH, &width);
    CALL_glXQueryDrawable(dpy, old_write, GLX_HEIGHT, &height);
    if (test_width != -1 || test_height != -1)
        if (width != test_width || height != test_height)
        {
            fprintf(stderr, "size changed from %dx%d to %dx%d\n",
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
     * due to not having to context switch.
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
    bugle_end_internal_render("end_screenshot", true);
    return true;
}

static bool screenshot_stream(FILE *out, bool check_size)
{
    screenshot_data *fetch;
    GLubyte *cur;
    size_t size, count;
    bool ret = true;
    int i;

    if ((fetch = start_screenshot()) != NULL)
    {
        fprintf(out, "P6\n%d %d\n255\n",
                (int) fetch->width, (int) fetch->height);
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
    }
    if (check_size && video_leader < video_lag)
        video_done = !end_screenshot(GL_RGB, video_data[0].width, video_data[0].height);
    else
        end_screenshot(GL_RGB, -1, -1);
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
        if (video_leader == video_lag) /* first frame */
            video_frame_time = t;
        else if (t < video_frame_time)
            return; /* drop the frame because it is too soon */
    }
    if ((fetch = start_screenshot()) != NULL)
    {
        if (!video_context)
            initialise_lavc(fetch->width, fetch->height);
        c = &video_stream->codec;
        video_raw->data[0] = fetch->pixels + fetch->stride * (fetch->height - 1);
        video_raw->linesize[0] = -fetch->stride;
        img_convert((AVPicture *) video_yuv, c->pix_fmt,
                    (AVPicture *) video_raw, CAPTURE_AV_FMT,
                    fetch->width, fetch->height);
        for (i = 0; i < fetch->multiplicity; i++)
        {
            out_size = avcodec_encode_video(&video_stream->codec,
                                            video_buffer, video_buffer_size,
                                            video_yuv);
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
    }

    if (!video_sample_all)
    {
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
    if (video_leader < video_lag)
        video_done = !end_screenshot(CAPTURE_GL_FMT, video_data[0].width, video_data[0].height);
    else
        end_screenshot(CAPTURE_GL_FMT, -1, -1);
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

    bugle_asprintf(&fname, "%s%.4d%s", file_base, frameno, file_suffix);
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

bool screenshot_callback(function_call *call, const callback_data *data)
{
    /* FIXME: track the frameno in the context?
     */
    static int frameno = 0;

    if (frameno >= start_frameno)
    {
        if (video_file)
        {
                if (!video_done) screenshot_video(frameno);
        }
        else screenshot_file(frameno);
    }
    frameno++;
    return true;
}

static bool initialise_screenshot(filter_set *handle)
{
    filter *f;

    f = bugle_register_filter(handle, "screenshot");
    bugle_register_filter_catches(f, CFUNC_glXSwapBuffers, screenshot_callback);
    bugle_register_filter_depends("invoke", "screenshot");

    video_data = bugle_calloc(video_lag, sizeof(screenshot_data));
    video_cur = 0;
    video_leader = video_lag;
    if (video_file)
    {
        video_done = false; /* becomes true if we resize */
#if !HAVE_LAVC
        char *cmdline;

        bugle_asprintf(&cmdline, "ppmtoy4m | ffmpeg -f yuv4mpegpipe -i - -vcodec %s -strict -1 -y %s",
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
    if (video_codec) free(video_codec);
}

static bugle_hash_table seen_extensions;
static const char *gl_version = "GL_VERSION_1_1";
static const char *glx_version = "GLX_VERSION_1_2";

static bool showextensions_callback(function_call *call, const callback_data *data)
{
    size_t i;
    const function_data *info;
    const gl_function *glinfo;

    info = &budgie_function_table[call->generic.id];
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
    bugle_register_filter_catches_all(f, showextensions_callback);
    /* The order mainly doesn't matter, but making it a pre-filter
     * reduces the risk of another filter aborting the call.
     */
    bugle_register_filter_depends("invoke", "showextensions");
    bugle_hash_init(&seen_extensions, false);
    return true;
}

static void destroy_showextensions(filter_set *handle)
{
    size_t i;
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

void bugle_initialise_filter_library(void)
{
    static const filter_set_variable_info screenshot_variables[] =
    {
        { "video", "file to write video to [none]", FILTER_SET_VARIABLE_STRING, &video_file, NULL },
        { "codec", "video codec to use [mpeg4]", FILTER_SET_VARIABLE_STRING, &video_codec, NULL },
        { "start", "frame from which to capture [0]", FILTER_SET_VARIABLE_UINT, &start_frameno, NULL },
        { "bitrate", "video bitrate (bytes/s) [7.5MB/s]", FILTER_SET_VARIABLE_POSITIVE_INT, &video_bitrate, NULL },
        { "allframes", "capture every frame, ignoring framerate [no]", FILTER_SET_VARIABLE_BOOL, &video_sample_all, NULL },
        { "lag", "length of capture pipeline (set higher for better throughput) [1]", FILTER_SET_VARIABLE_POSITIVE_INT, &video_lag, NULL },
        { NULL, NULL, 0, NULL, NULL }
    };

    static const filter_set_info screenshot_info =
    {
        "screenshot",
        initialise_screenshot,
        destroy_screenshot,
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
        0,
        "reports extensions used at program termination"
    };

    video_codec = bugle_strdup("mpeg4");

    bugle_register_filter_set(&screenshot_info);
    bugle_register_filter_set(&showextensions_info);

    bugle_register_filter_set_renders("screenshot");
    bugle_register_filter_set_depends("screenshot", "trackextensions");
}
