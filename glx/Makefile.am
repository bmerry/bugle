includebugleglxdir = $(includebugledir)/glx
includebugleglx_HEADERS = \
	include/bugle/glx/overrides.h \
	include/bugle/glx/glxdump.h

bugle_sources += \
	src/glx/glxdump.c src/glx/glwin.c

$(BUDGIE_BUILT_SRCS): budgie/budgie$(EXEEXT) src/data/gl.tu $(GLBCFILES) $(GLXBCFILES) $(srcdir)/gl-glx.bc
	$(mkdir_p) include/budgie
	budgie/budgie$(EXEEXT) -I$(srcdir) -T `test -f src/data/gl.tu || echo '$(srcdir)/'`src/data/gl.tu $(srcdir)/gl-glx.bc
