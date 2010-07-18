/* A multi-threaded application, that calls various GLX functions
 * simultaneously from two threads. It does not call simultaneous
 * GL functions (that is a separate test).
 */

#include <GL/glx.h>
#include <X11/Xlib.h>
#include <pthread.h>

void *thread1(void *arg)
{
    int i;
    Display *dpy;
    int major, minor;

    dpy = (Display *) arg;
    for (i = 0; i < 10000; i++)
        glXQueryVersion(dpy, &major, &minor);
    return NULL;
}

void *thread2(void *arg)
{
    int i;
    Display *dpy;

    dpy = (Display *) arg;
    for (i = 0; i < 10000; i++)
        glXQueryExtensionsString(dpy, DefaultScreen(dpy));
    return NULL;
}

int main(void)
{
    Display *dpy;
    pthread_t threads[2];

    dpy = XOpenDisplay(NULL);
    if (!dpy) return 0;

    pthread_create(&threads[0], NULL, thread1, (void *) dpy);
    pthread_create(&threads[1], NULL, thread2, (void *) dpy);
    pthread_join(threads[0], NULL);
    pthread_join(threads[1], NULL);

    XCloseDisplay(dpy);
    return 0;
}
