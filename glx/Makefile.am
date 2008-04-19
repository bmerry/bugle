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

pkglib_LTLIBRARIES += filters/modify.la
filters_modify_la_SOURCES = filters/modify.c
filters_modify_la_LIBADD = lib/libgnu.la -lm
filters_modify_la_LDFLAGS = $(MODULEFLAGS)

pkglib_LTLIBRARIES += filters/capture.la
filters_capture_la_SOURCES = filters/capture.c gl2ps/gl2ps.c gl2ps/gl2ps.h
filters_capture_la_CFLAGS = $(AVCODEC_CFLAGS)
filters_capture_la_LIBADD = lib/libgnu.la $(AVCODEC_LIBS)
filters_capture_la_LDFLAGS = $(MODULEFLAGS)

pkglib_LTLIBRARIES += filters/showstats.la
filters_showstats_la_SOURCES = filters/showstats.c
filters_showstats_la_LDFLAGS = $(MODULEFLAGS)
filters_showstats_la_LIBADD = lib/libgnu.la -lm

pkglib_LTLIBRARIES += filters/validate.la
filters_validate_la_SOURCES = filters/validate.c
filters_validate_la_LDFLAGS = $(MODULEFLAGS)
filters_validate_la_LIBADD = lib/libgnu.la

$(BUDGIE_BUILT_SRCS): budgie/budgie$(EXEEXT) src/data/gl.tu gl.bc gl/bc/alias.bc $(GLBCFILES) $(GLXBCFILES) $(srcdir)/gl-glx.bc
	$(mkdir_p) include/budgie
	budgie/budgie$(EXEEXT) -I$(srcdir) -T `test -f src/data/gl.tu || echo '$(srcdir)/'`src/data/gl.tu $(srcdir)/gl-glx.bc
