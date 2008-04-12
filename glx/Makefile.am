AM_CPPFLAGS += -DBUGLE_GLWIN_GLX

includebugle_HEADERS += \
	include/bugle/glx/overrides.h \
	include/bugle/glx/glxtypes.h \
	include/bugle/glx/glxdump.h \
	include/bugle/glx/glwintypes.h

src_libbugleutils_la_SOURCES += \
	src/glx/glxtypes.c

src_libbugle_la_SOURCES += \
	src/glx/glxdump.c src/glx/glwin.c

$(BUDGIE_BUILT_SRCS): budgie/budgie$(EXEEXT) src/data/gl.tu gl.bc gl/bc/alias.bc $(GLBCFILES) $(GLXBCFILES) $(srcdir)/gl-glx.bc
	$(mkdir_p) include/budgie
	budgie/.libs/budgie$(EXEEXT) -I$(srcdir) -T `test -f src/data/gl.tu || echo '$(srcdir)/'`src/data/gl.tu $(srcdir)/gl-glx.bc
