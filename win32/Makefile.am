# FIXME: need to make -export-symbols-regex work with stdcall
# SYMBOLFLAGS = -export-symbols-regex '^(gl[A-Z]|budgie_|bugle_|gldb_|X|dlopen)

SYMBOLFLAGS += -Wl,--kill-at
MODULEFLAGS += src/opengl32.la

AM_CPPFLAGS += -DBUGLE_WIN_WIN32

lib_LTLIBRARIES += src/opengl32.la
src_opengl32_la_SOURCES = $(bugle_sources) src/win32/xevent.c
nodist_src_opengl32_la_SOURCES = $(bugle_sources_nodist)
src_opengl32_la_LIBADD = $(bugle_libadd)
src_opengl32_la_LDFLAGS = $(bugle_ldflags) -module -avoid-version
