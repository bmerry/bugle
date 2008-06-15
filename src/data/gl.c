#define GL_GLEXT_PROTOTYPES
#define GLX_GLXEXT_PROTOTYPES
#define WGL_WGLEXT_PROTOTYPES
#include <bugle/porting.h>
#include <bugle/gl/glheaders.h>
#include <bugle/gl/overrides.h>
#if BUGLE_GLWIN_GLX
# include <GL/glx.h>
# include <bugle/glx/overrides.h>
#endif
#if BUGLE_GLWIN_WGL
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <bugle/wgl/overrides.h>
#endif
#if BUGLE_GLWIN_EGL
# include <EGL/egl.h>
# include <bugle/egl/overrides.h>
#endif
