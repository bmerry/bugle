## Process with automake 1.7 or newer

SYMBOLFLAGS = -export-symbol-regex '^(gl[A-Z]|budgie_|bugle_|gldb_)'

# Utility programs, used for generating other files
noinst_PROGRAMS = budgie/budgie
# Misc extra files to include
noinst_HEADERS = budgie/tree.def budgie/c-common.def
dist_noinst_SCRIPTS = gentokens/gentokens.perl gentokens/genfunctions.perl \
	gentokens/find_header.perl gentokens/genexts.perl
dist_man_MANS = doc/gldb.1 doc/bugle.3
# The real things
lib_LTLIBRARIES = src/libbugleutils.la src/libbugle.la
bin_PROGRAMS = gldb/gldb
pkglib_LTLIBRARIES = \
	filters/common.la \
	filters/trace.la \
	filters/modify.la \
	filters/validate.la \
	filters/debugger.la \
	filters/capture.la \
        filters/stats.la

INCLUDES = -I$(srcdir)/budgielib
AM_CFLAGS = -DLIBDIR=\"$(libdir)\" -DPKGLIBDIR=\"$(pkglibdir)\" $(X_CFLAGS)
AM_YFLAGS = -d

gldb_gldb_SOURCES = gldb/gldb.c
gldb_gldb_LDADD = src/libbugleutils.la $(READLINE_LIBS)
gldb_gldb_LDFLAGS = -thread-safe

budgie_budgie_SOURCES = \
	budgie/budgie.cpp budgie/budgie.h \
	budgie/tree.cpp budgie/tree.h \
	budgie/treeutils.cpp budgie/treeutils.h \
	budgie/tulexer.ll \
	budgie/bclexer.ll budgie/bcparser.yy

# Unfortunately this introduces a dependence open flex
budgie/bclexer.cc: budgie/bclexer.ll
	$(SHELL) $(YLWRAP) `test -f budgie/bclexer.ll || echo '$(srcdir)/'`budgie/bclexer.ll lex.bc_yy.c budgie/bclexer.cc -- $(LEXCOMPILE) -Pbc_yy

BUILT_SOURCES = budgie/bcparser.h

src_libbugleutils_la_SOURCES = \
	src/confparse.y src/conflex.l src/conffile.h \
	src/gltokens.h \
	common/safemem.c common/safemem.h \
	common/bool.h common/protocol.h common/protocol.c \
	common/hashtable.c common/hashtable.h \
	common/linkedlist.c common/linkedlist.h \
        common/threads.h
nodist_src_libbugleutils_la_SOURCES = \
	src/names.c src/names.h \
	src/gltokens.c src/glfuncs.c src/glfuncs.h \
        src/glexts.c src/glexts.h
src_libbugleutils_la_LIBADD = $(LTLIBOBJS) $(THREAD_LIBS)
src_libbugleutils_la_LDFLAGS = -thread-safe -export-dynamic $(SYMBOLFLAGS) -version-info 0:0:0

src_libbugle_la_SOURCES = \
	src/gldump.c src/gldump.h \
	src/glutils.c src/glutils.h \
	src/glstate.c src/glstate.h \
	src/interceptor.c \
	src/filters.c src/filters.h \
	src/trackcontext.c src/trackdisplaylist.c src/trackobjects.c \
        src/trackbeginend.c src/trackextensions.c src/tracker.h \
	src/log.c src/log.h \
        src/objects.c src/objects.h \
	budgielib/budgieutils.c budgielib/budgieutils.h
nodist_src_libbugle_la_SOURCES = \
	src/utils.c src/utils.h src/lib.c src/lib.h
src_libbugle_la_LIBADD = src/libbugleutils.la -ldl -lGL $(X_PRE_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS) $(THREAD_LIBS)
src_libbugle_la_LDFLAGS = -thread-safe -export-dynamic $(SYMBOLFLAGS) -version-info 0:1:0

BUILT_SOURCES += src/lib.h src/utils.h src/names.h src/confparse.h

# Generated files that generate other files

src/data/gl.tu: src/data/gl.c src/data/overrides.h
	$(srcdir)/mkinstalldirs src/data
	$(COMPILE) -fdump-translation-unit -c $(srcdir)/src/data/gl.c -o src/data/gl.o
	rm -f src/data/gl.o
	mv gl.c.tu src/data/gl.tu

BCFILES = bc/core1_1.bc bc/glx.bc \
	bc/GL_ARB_multitexture.bc bc/GL_ARB_shader_objects.bc \
        bc/GL_ARB_texture_compression.bc bc/GL_ARB_vertex_buffer_object.bc \
        bc/GL_ARB_vertex_program.bc bc/GL_EXT_blend_func_separate.bc \
        bc/GL_EXT_draw_range_elements.bc bc/GL_EXT_multi_draw_arrays.bc \
        bc/GL_EXT_secondary_color.bc bc/GL_EXT_texture3D.bc \
        bc/GLX_ARB_get_proc_address.bc

src/names.c src/names.h src/utils.c src/utils.h src/lib.c src/lib.h: budgie/budgie src/data/gl.tu gl.bc bc/alias.bc $(BCFILES)
	$(srcdir)/mkinstalldirs src
	budgie/budgie -t `test -f src/data/gl.tu || echo '$(srcdir)/'`src/data/gl.tu -o src/utils -n src/names -l src/lib gl.bc

BUILT_SOURCES += src/glfuncs.h src/glexts.h

src/gltokens.c: gentokens/gentokens.perl
	$(PERL) -w $(srcdir)/gentokens/gentokens.perl $(GLINCLUDES) > $@
src/glfuncs.h: gentokens/genfunctions.perl src/utils.h
	$(PERL) -w $(srcdir)/gentokens/genfunctions.perl --out-header --header src/utils.h $(GLINCLUDES) > $@
src/glfuncs.c: gentokens/genfunctions.perl src/utils.h
	$(PERL) -w $(srcdir)/gentokens/genfunctions.perl --header src/utils.h $(GLINCLUDES) > $@
src/glexts.h: gentokens/genexts.perl
	$(PERL) -w $(srcdir)/gentokens/genexts.perl --out-header $(GLINCLUDES) > $@
src/glexts.c: gentokens/genexts.perl
	$(PERL) -w $(srcdir)/gentokens/genexts.perl $(GLINCLUDES) > $@
bc/alias.bc: gentokens/genfunctions.perl
	$(srcdir)/mkinstalldirs bc
	$(PERL) -w $(srcdir)/gentokens/genfunctions.perl --alias $(GLINCLUDES) > $@

# Filters
filters_common_la_SOURCES = filters/common.c
filters_common_la_LDFLAGS = -thread-safe -module -avoid-version -export-dynamic $(SYMBOLFLAGS)
filters_trace_la_SOURCES = filters/trace.c
filters_trace_la_LDFLAGS = -thread-safe -module -avoid-version -export-dynamic $(SYMBOLFLAGS)
filters_modify_la_SOURCES = filters/modify.c
filters_modify_la_LDFLAGS = -thread-safe -module -avoid-version -export-dynamic $(SYMBOLFLAGS)
filters_validate_la_SOURCES = filters/validate.c
filters_validate_la_LDFLAGS = -thread-safe -module -avoid-version -export-dynamic $(SYMBOLFLAGS)
filters_debugger_la_SOURCES = filters/debugger.c
filters_debugger_la_LDFLAGS = -thread-safe -module -avoid-version -export-dynamic $(SYMBOLFLAGS)
filters_stats_la_SOURCES = filters/stats.c
filters_stats_la_LDFLAGS = -thread-safe -module -avoid-version -export-dynamic $(SYMBOLFLAGS)
filters_capture_la_SOURCES = filters/capture.c
filters_capture_la_LIBADD = $(AVCODEC_LIBS)
filters_capture_la_LDFLAGS = -thread-safe -module -avoid-version -export-dynamic $(SYMBOLFLAGS)

# Demo/Test programs
check_PROGRAMS = \
	tests/errors tests/threads1 tests/threads2 tests/queries \
	tests/misc tests/setstate tests/triangles \
        tests/pointers
dist_check_SCRIPTS = tests/queries.sh tests/setstate.sh tests/pointers.sh tests/triangles.sh tests/comparelog.sh tests/comparelog.perl
tests_errors_SOURCES = tests/errors.c
tests_errors_LDADD = src/libbugle.la -lglut -lGLU $(GL_LIBS) $(X_PRE_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS)
tests_pointers_SOURCES = tests/pointers.c
tests_pointers_LDADD = src/libbugle.la -lglut -lGLU $(GL_LIBS) $(X_PRE_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS)
tests_threads1_SOURCES = tests/threads1.c
tests_threads1_LDADD = src/libbugle.la $(GL_LIBS) $(X_PRE_LIBS) $(X_LIBS) $(X_EXTRA_LIBS) -lX11 $(THREAD_LIBS)
tests_threads2_SOURCES = tests/threads2.c
tests_threads2_LDADD = src/libbugle.la $(GL_LIBS) $(X_PRE_LIBS) $(X_LIBS) $(X_EXTRA_LIBS) -lX11 $(THREAD_LIBS)
tests_queries_SOURCES = tests/queries.c
tests_queries_LDADD = src/libbugle.la -lglut -lGLU $(GL_LIBS) $(X_PRE_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS)
tests_triangles_SOURCES = tests/triangles.c
tests_triangles_LDADD = src/libbugle.la -lglut -lGLU $(GL_LIBS) $(X_PRE_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS)
tests_misc_SOURCES = tests/misc.c
tests_misc_LDADD = src/libbugle.la -lglut -lGLU $(GL_LIBS) $(X_PRE_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS)
tests_setstate_SOURCES = tests/setstate.c
tests_setstate_LDADD = src/libbugle.la -lglut -lGLU $(GL_LIBS) $(X_PRE_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS)
TESTS = tests/errors tests/misc tests/queries.sh tests/setstate.sh tests/triangles.sh tests/pointers.sh
TESTS_ENVIRONMENT = BUGLE_FILTERS=$(srcdir)/tests/filters BUGLE_FILTER_DIR=filters/.libs

# Extra files to distribute
EXTRA_DIST = gl.bc.in $(BCFILES) src/data/gl.c src/data/overrides.h \
	FAQ TROUBLESHOOTING \
	doc/filters.html doc/objects.html doc/locks.txt doc/protocol.txt doc/examples/filters \
	tests/filters

# Note: automake ought to clean the various files in the .libs directories,
# but for some reason does not.
CLEANFILES = \
	src/names.c src/names.h src/utils.c src/utils.h src/lib.c src/lib.h \
	src/gltokens.c src/glfuncs.c src/glfuncs.h src/glexts.c src/glexts.h \
        src/data/gl.tu bc/alias.bc \
	doc/gldb.1.gz doc/bugle.3.gz \
	gldb/.libs/gldb \
        tests/.libs/threads1 \
        tests/.libs/threads2 \
	tests/.libs/errors tests/.libs/lt-errors \
        tests/.libs/queries tests/.libs/lt-queries \
        tests/.libs/misc tests/.libs/lt-misc \
	tests/.libs/setstate tests/.libs/lt-setstate \
        tests/.libs/triangles tests/.libs/lt-triangles \
        tests/.libs/pointers tests/.libs/lt-pointers
DISTCLEANFILES = tests/*.log
