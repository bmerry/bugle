/* Makes GL calls from several threads. Generates errors and checks
 * that each is placed in the right thread. Uses GLX directly so that
 * multiple contexts and drawables may easily be used.
 */

#include <GL/glx.h>
#include <GL/gl.h>
#include <X11/Xlib.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define THREADS 5

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static void show_errors(void)
{
    GLenum error;
    while ((error = glGetError()) != GL_NO_ERROR)
        fprintf(stderr, "Got error 0x%.4x\n", error);
}

static void invalid_enum(void)
{
    glBegin(GL_MATRIX_MODE);
}

static void invalid_operation(void)
{
    glBegin(GL_TRIANGLES);
    glGetError();
    glEnd();
}

static void invalid_value(void)
{
    /* border of 2 is illegal */
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 1, 1, 2, GL_RGB, GL_UNSIGNED_BYTE, NULL);
}

static void stack_underflow(void)
{
    glPopAttrib();
}

static void stack_overflow(void)
{
    GLint depth, i;

    glGetIntegerv(GL_MAX_ATTRIB_STACK_DEPTH, &depth);
    for (i = 0; i <= depth; i++)
        glPushAttrib(GL_ALL_ATTRIB_BITS);
    for (i = 0; i < depth; i++)
        glPopAttrib();
}

void *thread(void *arg)
{
    Display *dpy;
    Window window;
    GLXContext ctx;
    GLXFBConfig cfg, *cfgs;
    GLXDrawable draw;
    int elements;
    XVisualInfo *vi;
    XSetWindowAttributes x_attribs;
    const int cfg_attribs[] = {
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        None
    };

    pthread_mutex_lock(&lock);
    dpy = XOpenDisplay(NULL);
    if (!dpy) return arg; /* indicates failure */

    cfgs = glXChooseFBConfig(dpy, DefaultScreen(dpy), cfg_attribs, &elements);
    if (elements == 0)
    {
        fputs("No suitable FBConfig was found.\n", stderr);
        pthread_exit(arg); /* indicates failure */
    }
    cfg = *cfgs;
    ctx = glXCreateNewContext(dpy, cfg, GLX_RGBA_TYPE, NULL, True);
    vi = glXGetVisualFromFBConfig(dpy, cfg);
    window = XCreateWindow(dpy,
                           RootWindow(dpy, vi->screen),
                           0, 0, 300, 300, 0,  /* x, y, w, h, border */
                           vi->depth,
                           InputOutput,
                           vi->visual,
                           0,                  /* valuemask */
                           &x_attribs);        /* attributes */
    XMapWindow(dpy, window);
    draw = glXCreateWindow(dpy, cfg, window, NULL);
    glXMakeContextCurrent(dpy, draw, draw, ctx);
    pthread_mutex_unlock(&lock);

    /* The fun begins */
    invalid_enum();
    show_errors();
    invalid_operation();
    show_errors();
    invalid_value();
    show_errors();
    stack_underflow();
    show_errors();
    stack_overflow();
    show_errors();
    /* Cleanup */

    pthread_mutex_lock(&lock);
    glXDestroyWindow(dpy, draw);
    glXDestroyContext(dpy, ctx);
    XDestroyWindow(dpy, window);
    XFree(cfgs);
    XFree(vi);
    XCloseDisplay(dpy);
    pthread_mutex_unlock(&lock);
    return NULL;
}

int main(void)
{
    pthread_t threads[THREADS];
    int i;

    for (i = 0; i < THREADS; i++)
        pthread_create(&threads[i], NULL, thread, NULL);
    for (i = 0; i < THREADS; i++)
        pthread_join(threads[i], NULL);

    return 0;
}
