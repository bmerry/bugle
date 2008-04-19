MODULEFLAGS += src/libbugle.la

AM_CPPFLAGS += -DBUGLE_WIN_X11

lib_LTLIBRARIES += src/libbugle.la
src_libbugle_la_SOURCES = $(bugle_sources) src/x11/xevent.c
nodist_src_libbugle_la_SOURCES = $(bugle_sources_nodist)
src_libbugle_la_LIBADD = $(bugle_libadd)
src_libbugle_la_LDFLAGS = $(bugle_ldflags)
