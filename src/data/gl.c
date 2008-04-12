#define GL_GLEXT_PROTOTYPES
#define GLX_GLXEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glx.h>
#include <bugle/gl/overrides.h>
#if BUGLE_GLWIN_GLX
# include <bugle/glx/overrides.h>
#endif
