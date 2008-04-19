# FIXME: need to make -export-symbols-regex work with stdcall
# SYMBOLFLAGS = -export-symbols-regex '^(gl[A-Z]|budgie_|bugle_|gldb_|X|dlopen)

SYMBOLFLAGS += -Wl,--kill-at

AM_CPPFLAGS += -DBUGLE_WIN_WIN32

src_libbugle_la_SOURCES += src/win32/xevent.c
