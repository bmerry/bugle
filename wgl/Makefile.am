AM_CPPFLAGS += -DBUGLE_GLWIN_WGL

includebugle_HEADERS += \
	include/bugle/wgl/glwintypes.h

src_libbugle_la_SOURCES += \
	src/wgl/glwin.c

# libtool seems to mess up the wrapper, so call it directly
$(BUDGIE_BUILT_SRCS): budgie/budgie$(EXEEXT) src/data/gl.tu $(GLBCFILES) $(WGLBCFILES) $(srcdir)/gl-wgl.bc
	$(mkdir_p) include/budgie
	budgie/.libs/budgie$(EXEEXT) -I$(srcdir) -T `test -f src/data/gl.tu || echo '$(srcdir)/'`src/data/gl.tu $(srcdir)/gl-wgl.bc
