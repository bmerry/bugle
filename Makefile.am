noinst_PROGRAMS = budgie/budgie gentokens/gentokens
noinst_LTLIBRARIES = budgie/libbudgie.la
noinst_HEADERS = budgie/tree.def
lib_LTLIBRARIES = src/libbugle.la
pkglib_LTLIBRARIES = \
	filters/common.la \
	filters/modify.la \
	filters/validate.la \
	filters/tracker.la \
	filters/debugger.la
INCLUDES = -I$(srcdir)/budgielib
AM_CFLAGS = -DPKGLIBDIR=\"$(pkglibdir)\"
AM_YFLAGS = -d

budgie_budgie_SOURCES = \
	budgie/budgie.cpp \
	budgie/generator.cpp budgie/generator.h \
	budgie/tree.cpp budgie/tree.h \
	budgie/treeutils.cpp budgie/treeutils.h \
	budgie/lexer.ll

budgie_libbudgie_la_SOURCES = \
	budgielib/budgieutils.c budgielib/budgieutils.h \
	budgielib/state.c budgielib/state.h

src_libbugle_la_SOURCES = \
	src/confparse.y src/conflex.l src/conffile.h \
	src/interceptor.c src/filters.c src/filters.h \
	src/gldump.c src/gldump.h \
	src/gltokens.h \
	src/linkedlist.c src/linkedlist.h \
	src/glstate.c src/glstate.h \
	src/canon.c src/canon.h \
	src/safemem.c src/safemem.h \
	common/bool.h
nodist_src_libbugle_la_SOURCES = \
	src/lib.c src/lib.h src/utils.c \
	src/utils.h src/types.c src/types.h \
	src/gltokens.c
BUILT_SOURCES = src/lib.h src/utils.h src/types.h confparse.h
src_libbugle_la_LIBADD = budgie/libbudgie.la
src_libbugle_la_LDFLAGS = -export-dynamic

# This is not the intended use of EXTRA..., but it forces the files into
# the distribution. It would also work to use EXTRA_DIST
EXTRA_src_libbugle_la_SOURCES = src/gl.bc src/data/gl.c src/data/gl.tu src/data/overrides.h

# Generated files that generate other files

src/data/gl.tu: src/data/gl.c src/data/overrides.h
	$(srcdir)/mkinstalldirs src/data
	$(CC) -fdump-translation-unit -c $(srcdir)/src/data/gl.c -o src/data/gl.o
	rm -f src/data/gl.o
	mv gl.c.tu src/data/gl.tu

src/types.c src/types.h src/utils.c src/utils.h src/lib.c src/lib.h: budgie/budgie src/data/gl.tu src/gl.bc Makefile
	$(srcdir)/mkinstalldirs src
	budgie/budgie -c $(srcdir)/src/gl.bc -t `test -f src/data/gl.tu || echo '$(srcdir)/'`src/data/gl.tu -o src/utils -T src/types -l src/lib

gentokens_gentokens_SOURCES = gentokens/gentokens.c
BUILT_SOURCES += src/gltokens.c
src/gltokens.c: gentokens/gentokens Makefile
	$(AWK) '/^#define GL_[_A-Z0-9]+[ \t]+0x[0-9A-F]+/ && !/(GL_COLOR_TABLE_.*_EXT)|(GL_.*BIT(_[A-Z]+.*)?[ \t])/{printf "%s 0x%x\n", $$2, $$3;}' /usr/include/GL/glext.h /usr/include/GL/gl.h | gentokens/gentokens > $@

# Filters

filters_common_la_SOURCES = filters/common.c
filters_common_la_LDFLAGS = -module
filters_modify_la_SOURCES = filters/modify.c
filters_modify_la_LDFLAGS = -module
filters_validate_la_SOURCES = filters/validate.c
filters_validate_la_LDFLAGS = -module -avoid-version
filters_tracker_la_SOURCES = filters/tracker.c
filters_tracker_la_LDFLAGS = -module -avoid-version
filters_debugger_la_SOURCES = filters/debugger.c
filters_debugger_la_LDFLAGS = -module -avoid-version

CLEANFILES = src/types.c src/types.h src/utils.c src/utils.h src/lib.c src/lib.h src/gltokens.c
MAINTAINERCLEANFILES = src/data/gl.tu
