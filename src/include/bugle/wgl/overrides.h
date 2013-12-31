#ifndef BUGLE_WGL_OVERRIDES_H
#define BUGLE_WGL_OVERRIDES_H

/* These functions are undocumented, but called by GDI32
 * Not too sure about the signatures on all of these, but the number of bytes
 * on the stack is the important part for __stdcall. These are correct for
 * 32-bit, I don't know about 64-bit.
 */

WINGDIAPI int WINAPI wglChoosePixelFormat(HDC, const PIXELFORMATDESCRIPTOR *);
WINGDIAPI BOOL WINAPI wglSetPixelFormat(HDC, int, const PIXELFORMATDESCRIPTOR *);
WINGDIAPI int WINAPI wglDescribePixelFormat(HDC, int, UINT, LPPIXELFORMATDESCRIPTOR);
WINGDIAPI int WINAPI wglGetPixelFormat(HDC);
WINGDIAPI PROC WINAPI wglGetDefaultProcAddress(HDC);
WINGDIAPI BOOL WINAPI wglSwapBuffers(HDC);
WINGDIAPI DWORD WINAPI wglSwapMultipleBuffers(UINT a1, CONST WGLSWAP *);

WINGDIAPI int WINAPI GlmfPlayGlsRecord(DWORD a1, DWORD a2, DWORD a3, DWORD a4);
WINGDIAPI int WINAPI GlmfInitPlayback(DWORD a1, DWORD a2, DWORD a3);
WINGDIAPI int WINAPI GlmfEndPlayback(DWORD a1);
WINGDIAPI int WINAPI GlmfEndGlsBlock(DWORD a1);
WINGDIAPI int WINAPI GlmfCloseMetaFile(DWORD a1);
WINGDIAPI int WINAPI GlmfBeginGlsBlock(DWORD a1);
WINGDIAPI int WINAPI glDebugEntry(DWORD a1, DWORD a2);

#endif /* BUGLE_WGL_OVERRIDES_H */

