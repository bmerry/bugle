## Process with automake 1.7 or newer
SUBDIRS = libltdl

SYMBOLFLAGS = -export-symbols-regex '^(gl[A-Z]|budgie_|bugle_|gldb_|X|dlopen)'

# Utility programs, used for generating other files
noinst_PROGRAMS = budgie/budgie
# Misc extra files to include
noinst_HEADERS = budgie/tree.def budgie/c-common.def
dist_noinst_SCRIPTS = gentokens/gentokens.perl gentokens/genfunctions.perl \
	gentokens/find_header.perl gentokens/genexts.perl
dist_man_MANS = doc/man1/gldb.1 doc/man3/bugle.3 \
	doc/man7/bugle-camera.7 \
	doc/man7/bugle-checks.7 \
	doc/man7/bugle-eps.7 \
	doc/man7/bugle-error.7 \
	doc/man7/bugle-frontbuffer.7 \
	doc/man7/bugle-log.7 \
	doc/man7/bugle-screenshot.7 \
	doc/man7/bugle-showerror.7 \
	doc/man7/bugle-showextensions.7 \
	doc/man7/bugle-stats_basic.7 \
	doc/man7/bugle-stats_primitives.7 \
	doc/man7/bugle-stats_fragments.7 \
	doc/man7/bugle-stats_nv.7 \
	doc/man7/bugle-showstats.7 \
	doc/man7/bugle-logstats.7 \
	doc/man7/bugle-trace.7 \
	doc/man7/bugle-unwindstack.7 \
	doc/man7/bugle-wireframe.7 \
	doc/man7/bugle-extoverride.7
# The real things
lib_LTLIBRARIES = src/libbugleutils.la src/libbugle.la
bin_PROGRAMS = gldb/gldb @GLDB_GUI@
EXTRA_PROGRAMS = gldb/gldb-gui

pkglib_LTLIBRARIES = \
	filters/common.la \
	filters/trace.la \
	filters/modify.la \
	filters/validate.la \
	filters/debugger.la \
	filters/capture.la \
	filters/stats.la

AM_CPPFLAGS = -I$(srcdir)/budgielib $(LTDLINCL) -DLIBDIR=\"$(libdir)\" -DPKGLIBDIR=\"$(pkglibdir)\" $(X_CFLAGS)
AM_YFLAGS = -d

gldb_gldb_SOURCES = gldb/gldb.c gldb/gldb-common.c gldb/gldb-common.h
gldb_gldb_LDADD = src/libbugleutils.la $(READLINE_LIBS)
gldb_gldb_LDFLAGS = -thread-safe

gldb_gldb_gui_SOURCES = gldb/gldb-gui.c gldb/gldb-gui.h \
	gldb/gldb-gui-state.c gldb/gldb-gui-state.h \
	gldb/gldb-gui-image.c gldb/gldb-gui-image.h \
	gldb/gldb-gui-texture.c gldb/gldb-gui-texture.h \
	gldb/gldb-gui-framebuffer.c gldb/gldb-gui-framebuffer.h \
	gldb/gldb-gui-shader.c gldb/gldb-gui-shader.h \
	gldb/gldb-gui-backtrace.c gldb/gldb-gui-backtrace.h \
	gldb/gldb-gui-breakpoint.c gldb/gldb-gui-breakpoint.h \
	gldb/gldb-common.c gldb/gldb-common.h \
	gldb/gldb-channels.c gldb/gldb-channels.h
if HAVE_GTKGLEXT
  gldb_gldb_gui_SOURCES += glee/GLee.c glee/GLee.h
endif
EXTRA_gldb_gldb_gui_SOURCES = glee/GLee.c glee/GLee.h
gldb_gldb_gui_LDADD = src/libbugleutils.la $(GTKGLEXT_LIBS) $(GTK_LIBS) -lm
gldb_gldb_gui_LDFLAGS = -thread-safe
gldb_gldb_gui_CFLAGS = $(GTKGLEXT_CFLAGS) $(GTK_CFLAGS)

budgie_budgie_SOURCES = \
	budgie/budgie.cpp budgie/budgie.h \
	budgie/tree.cpp budgie/tree.h \
	budgie/treeutils.cpp budgie/treeutils.h \
	budgie/tulexer.ll \
	budgie/bclexer.ll budgie/bcparser.yy

# Unfortunately this introduces a dependence on flex
budgie/bclexer.cc: budgie/bclexer.ll
	$(SHELL) $(YLWRAP) `test -f budgie/bclexer.ll || echo '$(srcdir)/'`budgie/bclexer.ll lex.bc_yy.c budgie/bclexer.cc -- $(LEXCOMPILE) -Pbc_yy

BUILT_SOURCES = budgie/bcparser.h

src_libbugleutils_la_SOURCES = \
	src/gltokens.h src/gltypes.c src/gltypes.h \
	common/safemem.c common/safemem.h \
	common/bool.h common/protocol.h common/protocol.c \
	common/hashtable.c common/hashtable.h \
	common/linkedlist.c common/linkedlist.h \
	common/radixtree.c common/radixtree.h \
	common/threads.h \
	budgielib/typeutils.c budgielib/typeutils.h \
	budgielib/ioutils.c budgielib/ioutils.h
nodist_src_libbugleutils_la_SOURCES = \
	src/names.c src/names.h src/types.c src/types.h \
	src/gltokens.c src/glfuncs.c src/glfuncs.h \
	src/glexts.c src/glexts.h
src_libbugleutils_la_LIBADD = $(LTLIBOBJS) $(THREAD_LIBS)
src_libbugleutils_la_LDFLAGS = -thread-safe -export-dynamic $(SYMBOLFLAGS) -version-info 2:0:0

src_libbugle_la_SOURCES = \
	src/confparse.y src/conflex.l src/conffile.h \
	src/gldump.c src/gldump.h \
	src/glutils.c src/glutils.h \
	src/glstate.c src/glstate.h \
	src/glsl.c src/glsl.h \
	src/interceptor.c \
	src/dlopen.c src/dlopen.h \
	src/filters.c src/filters.h \
	src/trackcontext.c src/trackdisplaylist.c src/trackobjects.c \
	src/trackbeginend.c src/trackextensions.c src/tracker.h \
	src/log.c src/log.h \
	src/objects.c src/objects.h \
	src/xevent.c src/xevent.h \
	budgielib/budgieutils.c budgielib/budgieutils.h
nodist_src_libbugle_la_SOURCES = \
	src/utils.c src/utils.h src/lib.c src/lib.h
src_libbugle_la_LIBADD = $(LIBLTDL) src/libbugleutils.la -lGL $(X_PRE_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS) $(THREAD_LIBS)
src_libbugle_la_LDFLAGS = -thread-safe -export-dynamic $(SYMBOLFLAGS) -version-info 2:0:0

BUILT_SOURCES += src/lib.h src/types.h src/utils.h src/names.h src/confparse.h

# Generated files that generate other files

# ccache doesn't play nice with -fdump-translation-unit, so we disable it
src/data/gl.tu: src/data/gl.c src/data/overrides.h
	$(mkdir_p) src/data
	CCACHE_DISABLE=1 $(COMPILE) -fdump-translation-unit -c $(srcdir)/src/data/gl.c -o src/data/gl.o
	rm -f src/data/gl.o
	mv gl.c.`test -f gl.c.t00.tu && echo t00.`tu src/data/gl.tu

BCFILES = bc/core1_1.bc bc/glsl1_20.bc bc/glx.bc \
	bc/GL_ARB_multitexture.bc bc/GL_ARB_imaging.bc \
	bc/GL_ARB_shader_objects.bc bc/GL_ARB_occlusion_query.bc \
	bc/GL_ARB_texture_compression.bc bc/GL_ARB_vertex_buffer_object.bc \
	bc/GL_ARB_vertex_program.bc bc/GL_ATI_draw_buffers.bc \
	bc/GL_EXT_blend_func_separate.bc bc/GL_EXT_draw_buffers2.bc \
	bc/GL_EXT_draw_instanced.bc bc/GL_EXT_draw_range_elements.bc \
	bc/GL_EXT_geometry_shader4.bc bc/GL_EXT_multi_draw_arrays.bc \
	bc/GL_EXT_secondary_color.bc bc/GL_EXT_texture3D.bc \
	bc/GL_EXT_framebuffer_object.bc bc/GL_EXT_gpu_program_parameters.bc \
	bc/GL_NV_transform_feedback.bc \
	bc/GLX_ARB_get_proc_address.bc bc/GLX_SGI_make_current_read.bc \
	bc/GLX_SGIX_fbconfig.bc bc/GLX_SGIX_pbuffer.bc

src/names.c src/names.h src/types.c src/types.h src/utils.c src/utils.h src/lib.c src/lib.h: budgie/budgie src/data/gl.tu gl.bc bc/alias.bc $(BCFILES)
	$(mkdir_p) src
	budgie/budgie -t `test -f src/data/gl.tu || echo '$(srcdir)/'`src/data/gl.tu -T src/types -o src/utils -n src/names -l src/lib gl.bc

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
	$(mkdir_p) bc
	$(PERL) -w $(srcdir)/gentokens/genfunctions.perl --alias $(GLINCLUDES) > $@

# Filters
filters_common_la_SOURCES = filters/common.c
filters_common_la_LDFLAGS = -thread-safe -module -avoid-version -export-dynamic $(SYMBOLFLAGS)
filters_trace_la_SOURCES = filters/trace.c
filters_trace_la_LDFLAGS = -thread-safe -module -avoid-version -export-dynamic $(SYMBOLFLAGS)
filters_modify_la_SOURCES = filters/modify.c
filters_modify_la_LIBADD = -lm
filters_modify_la_LDFLAGS = -thread-safe -module -avoid-version -export-dynamic $(SYMBOLFLAGS)
filters_validate_la_SOURCES = filters/validate.c
filters_validate_la_LDFLAGS = -thread-safe -module -avoid-version -export-dynamic $(SYMBOLFLAGS)
filters_debugger_la_SOURCES = filters/debugger.c
filters_debugger_la_LDFLAGS = -thread-safe -module -avoid-version -export-dynamic $(SYMBOLFLAGS)
filters_stats_la_SOURCES = filters/statslex.l filters/statsparse.y filters/stats.c filters/stats.h
filters_stats_la_LIBADD = -lm
filters_stats_la_LDFLAGS = -thread-safe -module -avoid-version -export-dynamic $(SYMBOLFLAGS)
filters_capture_la_SOURCES = filters/capture.c gl2ps/gl2ps.c gl2ps/gl2ps.h
filters_capture_la_CFLAGS = $(AVCODEC_CFLAGS)
filters_capture_la_LIBADD = $(AVCODEC_LIBS)
filters_capture_la_LDFLAGS = -thread-safe -module -avoid-version -export-dynamic $(SYMBOLFLAGS)

BUILT_SOURCES += filters/statsparse.h

# Demo/Test programs
check_PROGRAMS = \
	tests/errors tests/threads1 tests/threads2 tests/queries \
	tests/misc tests/setstate tests/triangles \
	tests/pointers tests/objects tests/textest tests/shadertest \
	tests/pbo tests/interpose tests/dlopen
dist_check_SCRIPTS = tests/errors.sh tests/misc.sh tests/queries.sh tests/setstate.sh tests/pointers.sh tests/triangles.sh tests/pbo.sh tests/comparelog.sh tests/comparelog.perl
tests_errors_SOURCES = tests/errors.c
tests_errors_LDADD = -lglut -lGLU $(GL_LIBS) $(X_PRE_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS)
tests_pointers_SOURCES = tests/pointers.c glee/GLee.c glee/GLee.h
tests_pointers_LDADD = -lglut -lGLU $(GL_LIBS) $(X_PRE_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS)
tests_pbo_SOURCES = tests/pbo.c glee/GLee.c glee/GLee.h
tests_pbo_LDADD = -lglut -lGLU $(GL_LIBS) $(X_PRE_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS)
tests_threads1_SOURCES = tests/threads1.c
tests_threads1_LDADD = $(GL_LIBS) $(X_PRE_LIBS) $(X_LIBS) $(X_EXTRA_LIBS) -lX11 $(THREAD_LIBS)
tests_threads2_SOURCES = tests/threads2.c
tests_threads2_LDADD = $(GL_LIBS) $(X_PRE_LIBS) $(X_LIBS) $(X_EXTRA_LIBS) -lX11 $(THREAD_LIBS)
tests_queries_SOURCES = tests/queries.c glee/GLee.c glee/GLee.h
tests_queries_LDADD = -lglut -lGLU $(GL_LIBS) $(X_PRE_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS)
tests_triangles_SOURCES = tests/triangles.c glee/GLee.c glee/GLee.h
tests_triangles_LDADD = -lglut -lGLU $(GL_LIBS) $(X_PRE_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS)
tests_misc_SOURCES = tests/misc.c
tests_misc_LDADD = -lglut -lGLU $(GL_LIBS) $(X_PRE_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS)
tests_interpose_SOURCES = tests/interpose.c
tests_interpose_LDADD = -lglut -lGLU $(GL_LIBS) $(X_PRE_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS)
tests_dlopen_SOURCES = tests/dlopen.c
tests_dlopen_LDADD = $(GL_LIBS) $(X_PRE_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS)
tests_setstate_SOURCES = tests/setstate.c glee/GLee.c glee/GLee.h
tests_setstate_LDADD = src/libbugle.la -lglut -lGLU $(GL_LIBS) $(X_PRE_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS)
tests_objects_SOURCES = tests/objects.c glee/GLee.c glee/GLee.h
tests_objects_LDADD = -lglut -lGLU $(GL_LIBS) $(X_PRE_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS)
tests_textest_SOURCES = tests/textest.c glee/GLee.c glee/GLee.h
tests_textest_LDADD = -lglut -lGLU $(GL_LIBS) $(X_PRE_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS)
tests_shadertest_SOURCES = tests/shadertest.c glee/GLee.c glee/GLee.h
tests_shadertest_LDADD = -lglut -lGLU $(GL_LIBS) $(X_PRE_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS)
TESTS = tests/errors.sh tests/misc.sh tests/queries.sh tests/setstate.sh tests/triangles.sh tests/pointers.sh tests/pbo.sh tests/interpose.sh tests/dlopen.sh
TESTS_ENVIRONMENT = BUGLE_FILTERS=$(srcdir)/tests/filters BUGLE_STATISTICS=$(srcdir)/tests/statistics BUGLE_FILTER_DIR=filters/.libs BUGLE_LIB=src/.libs/libbugle.so

# Extra files to distribute
EXTRA_DIST = gl.bc.in $(BCFILES) src/data/gl.c src/data/overrides.h \
	FAQ TROUBLESHOOTING LICENSE \
	doc/filters.html doc/objects.html doc/locks.txt \
	doc/protocol.txt doc/extensions.txt \
	doc/examples/filters doc/examples/statistics \
	glee/readme.txt \
	gl2ps/TODO gl2ps/COPYING.GL2PS gl2ps/COPYING.LGPL gl2ps/gl2ps.pdf \
	gl2ps/gl2psTestSimple.c gl2ps/gl2psTest.c \
	tests/filters tests/statistics

# Note: automake ought to clean the various files in the .libs directories,
# but for some reason does not.
CLEANFILES = \
	src/names.c src/names.h src/utils.c src/utils.h src/lib.c src/lib.h \
	src/types.c src/types.h \
	src/gltokens.c src/glfuncs.c src/glfuncs.h src/glexts.c src/glexts.h \
	src/data/gl.tu bc/alias.bc \
	doc/gldb.1.gz doc/bugle.3.gz \
	gldb/.libs/gldb gldb/.libs/gldb-gui \
	tests/.libs/threads1 \
	tests/.libs/threads2 \
	tests/.libs/errors tests/.libs/lt-errors \
	tests/.libs/queries tests/.libs/lt-queries \
	tests/.libs/misc tests/.libs/lt-misc \
	tests/.libs/setstate tests/.libs/lt-setstate \
	tests/.libs/triangles tests/.libs/lt-triangles \
	tests/.libs/pointers tests/.libs/lt-pointers \
	tests/.libs/pbo tests/.libs/lt-pbo \
	tests/.libs/objects \
DISTCLEANFILES = tests/*.log

libtool: @LIBTOOL_DEPS@
	$(SHELL) ./config.status --recheck
