noinst_PROGRAMS = budgie/budgie gentokens/gentokens
noinst_LTLIBRARIES = budgie/libbudgie.la common/libcommon.la
noinst_HEADERS = budgie/tree.def budgie/c-common.def
noinst_SCRIPTS = gentokens/gettokens.awk
lib_LTLIBRARIES = src/libbugle.la
bin_PROGRAMS = gldb/gldb
pkglib_LTLIBRARIES = \
	filters/common.la \
	filters/modify.la \
	filters/validate.la \
	filters/tracker.la \
	filters/debugger.la \
	filters/capture.la
INCLUDES = -I$(srcdir)/budgielib
AM_CFLAGS = -DLIBDIR=\"$(libdir)\" -DPKGLIBDIR=\"$(pkglibdir)\" $(X_CFLAGS)
AM_YFLAGS = -d

common_libcommon_la_SOURCES = \
	common/safemem.c common/safemem.h \
	common/bool.h common/protocol.h common/protocol.c \
	common/hashtable.c common/hashtable.h \
common_libcommon_la_LDFLAGS = -static

gldb_gldb_SOURCES = gldb/gldb.c
gldb_gldb_LDADD = common/libcommon.la

budgie_budgie_SOURCES = \
	budgie/budgie.cpp \
	budgie/generator.cpp budgie/generator.h \
	budgie/tree.cpp budgie/tree.h \
	budgie/treeutils.cpp budgie/treeutils.h \
	budgie/lexer.ll

budgie_libbudgie_la_SOURCES = \
	budgielib/budgieutils.c budgielib/budgieutils.h \
	budgielib/state.c budgielib/state.h
budgie_libbudgie_la_LDFLAGS = -static

src_libbugle_la_SOURCES = \
	src/confparse.y src/conflex.l src/conffile.h \
	src/interceptor.c src/filters.c src/filters.h \
	src/gldump.c src/gldump.h \
	src/glutils.c src/glutils.h \
	src/gltokens.h \
	src/linkedlist.c src/linkedlist.h \
	src/glstate.c src/glstate.h \
	src/canon.c src/canon.h \
nodist_src_libbugle_la_SOURCES = \
	src/lib.c src/lib.h src/utils.c \
	src/utils.h src/types.c src/types.h \
	src/gltokens.c
BUILT_SOURCES = src/lib.h src/utils.h src/types.h confparse.h
src_libbugle_la_LIBADD = budgie/libbudgie.la common/libcommon.la $(LTLIBOBJS) -lGL $(X_PRE_LIBS) $(X_LIBS) $(X_EXTRA_LIBS)
src_libbugle_la_LDFLAGS = -export-dynamic -version-info 0:0:0

# Extra files to distribute
EXTRA_DIST = src/gl.bc src/data/gl.c src/data/overrides.h \
	gentokens/gettokens.awk \
	FAQ TROUBLESHOOTING \
	doc/filters.txt doc/protocol.txt \
	doc/examples/filters

# Generated files that generate other files

src/data/gl.tu: src/data/gl.c src/data/overrides.h
	$(srcdir)/mkinstalldirs src/data
	$(COMPILE) -fdump-translation-unit -c $(srcdir)/src/data/gl.c -o src/data/gl.o
	rm -f src/data/gl.o
	mv gl.c.tu src/data/gl.tu

src/types.c src/types.h src/utils.c src/utils.h src/lib.c src/lib.h: budgie/budgie src/data/gl.tu src/gl.bc Makefile
	$(srcdir)/mkinstalldirs src
	budgie/budgie -c $(srcdir)/src/gl.bc -t `test -f src/data/gl.tu || echo '$(srcdir)/'`src/data/gl.tu -o src/utils -T src/types -l src/lib

gentokens_gentokens_SOURCES = gentokens/gentokens.c
BUILT_SOURCES += src/gltokens.c

src/gltokens.c: gentokens/gentokens gentokens/gettokens.awk Makefile
if PREPROCESS_GL
	$(CPP) $(CPP_DEFINES_FLAGS) $(DEFAULT_INCLUDES) $(INCLUDES) $(X_CFLAGS) $(srcdir)/src/data/gl.c | $(AWK) -f $(srcdir)/gentokens/gettokens.awk | gentokens/gentokens > $@
else
	$(AWK) -f $(srcdir)/gentokens/gettokens.awk $(GLINCLUDE_PATH)/glext.h $(GLINCLUDE_PATH)/gl.h | gentokens/gentokens > $@
endif

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
filters_capture_la_SOURCES = filters/capture.c
filters_capture_la_LDFLAGS = -module -avoid-version

CLEANFILES = src/types.c src/types.h src/utils.c src/utils.h src/lib.c src/lib.h \
	src/gltokens.c src/data/gl.tu

