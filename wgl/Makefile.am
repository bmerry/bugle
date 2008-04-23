AM_CPPFLAGS += -DBUGLE_GLWIN_WGL

includebugle_HEADERS += \
	include/bugle/wgl/overrides.h

bugle_sources += \
	src/wgl/glwin.c

$(BUDGIE_BUILT_SRCS): budgie/budgie$(EXEEXT) src/data/gl.tu $(GLBCFILES) $(WGLBCFILES) $(srcdir)/gl-wgl.bc
	$(mkdir_p) include/budgie
	budgie/budgie$(EXEEXT) -I$(srcdir) -T `test -f src/data/gl.tu || echo '$(srcdir)/'`src/data/gl.tu $(srcdir)/gl-wgl.bc
