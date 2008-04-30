# Process with automake 1.7 or newer
SUBDIRS = lib libltdl
ACLOCAL_AMFLAGS = -I m4

SYMBOLFLAGS = -export-symbols-regex '^(gl[A-Z]|budgie_|bugle_|gldb_|X|dlopen)'
MODULEFLAGS = -no-undefined -thread-safe -export-dynamic -module -avoid-version
MODULELIBS = src/libbugleutils.la lib/libgnu.la

# Utility programs, used for generating other files
noinst_PROGRAMS = budgie/budgie
# Misc extra files to include
noinst_HEADERS = budgie/tree.def budgie/c-common.def
dist_noinst_SCRIPTS = gengl/gengl.perl gengl/find_header.perl \
		      mvtu.sh
# The real things
lib_LTLIBRARIES = src/libbugleutils.la
bin_PROGRAMS = gldb/gldb @GLDB_GUI@
EXTRA_PROGRAMS = gldb/gldb-gui

includebugledir = $(includedir)/bugle/bugle
includebuglegldir = $(includebugledir)/gl
includebudgiedir = $(includedir)/bugle/budgie
includebugle_HEADERS = \
	include/bugle/hashtable.h \
	include/bugle/linkedlist.h \
	include/bugle/misc.h \
	include/bugle/filters.h \
	include/bugle/glsl.h \
	include/bugle/glstate.h \
	include/bugle/glutils.h \
	include/bugle/glreflect.h \
	include/bugle/log.h \
	include/bugle/objects.h \
	include/bugle/stats.h \
	include/bugle/tracker.h \
	include/bugle/xevent.h \
	include/bugle/porting.h \
	include/bugle/glwintypes.h \
	include/bugle/glwin.h
includebuglegl_HEADERS = \
	include/bugle/gl/overrides.h \
	include/bugle/gl/gltypes.h \
	include/bugle/gl/gldump.h
includebudgie_HEADERS = \
	include/budgie/addresses.h \
	include/budgie/basictypes.h \
	include/budgie/types.h \
	include/budgie/reflect.h \
	include/budgie/macros.h
nodist_includebudgie_HEADERS = \
	include/budgie/call.h \
	include/budgie/types2.h
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = bugle.pc

pkglib_LTLIBRARIES = \
	filters/common.la \
	filters/validate.la \
	filters/capture.la \
	filters/trace.la \
	filters/debugger.la \
	filters/modify.la \
	filters/stats_basic.la \
	filters/stats_calls.la \
	filters/stats_fragments.la \
	filters/stats_nv.la \
	filters/stats_primitives.la \
	filters/logstats.la

AM_CPPFLAGS = -I$(srcdir)/lib -Ilib -I$(srcdir)/include -Iinclude $(LTDLINCL) -DLIBDIR=\"$(libdir)\" -DPKGLIBDIR=\"$(pkglibdir)\" $(X_CFLAGS)
AM_YFLAGS = -d

gldb_gldb_SOURCES = gldb/gldb.c gldb/gldb-common.c gldb/gldb-common.h
gldb_gldb_LDADD = src/libbugleutils.la $(READLINE_LIBS) $(LTLIBINTL) lib/libgnu.la
gldb_gldb_LDFLAGS = -thread-safe

gldb_gldb_gui_SOURCES = gldb/gldb-gui.c gldb/gldb-gui.h \
        gldb/gldb-gui-target.c gldb/gldb-gui-target.h \
	gldb/gldb-gui-state.c gldb/gldb-gui-state.h \
	gldb/gldb-gui-image.c gldb/gldb-gui-image.h \
	gldb/gldb-gui-texture.c gldb/gldb-gui-texture.h \
	gldb/gldb-gui-framebuffer.c gldb/gldb-gui-framebuffer.h \
	gldb/gldb-gui-shader.c gldb/gldb-gui-shader.h \
	gldb/gldb-gui-backtrace.c gldb/gldb-gui-backtrace.h \
	gldb/gldb-gui-breakpoint.c gldb/gldb-gui-breakpoint.h \
	gldb/gldb-common.c gldb/gldb-common.h \
	gldb/gldb-channels.c gldb/gldb-channels.h
gldb_gldb_gui_LDADD = src/libbugleutils.la -lm $(GTKGLEXT_LIBS) $(GLEW_LIBS) $(GTK_LIBS) $(LTLIBINTL) lib/libgnu.la
gldb_gldb_gui_LDFLAGS = -thread-safe
gldb_gldb_gui_CFLAGS = $(GTKGLEXT_CFLAGS) $(GTK_CFLAGS)

budgie_budgie_SOURCES = \
	budgie/budgie.cpp budgie/budgie.h \
	budgie/tree.cpp budgie/tree.h \
	budgie/treeutils.cpp budgie/treeutils.h \
	budgie/tulexer.ll \
	budgie/bclexer.ll budgie/bcparser.yy
budgie_budgie_LDADD = $(REGEX_LIBS)

# Unfortunately this introduces a dependence on flex
budgie/bclexer.cc: budgie/bclexer.ll
	$(SHELL) $(YLWRAP) `test -f budgie/bclexer.ll || echo '$(srcdir)/'`budgie/bclexer.ll lex.bc_yy.c budgie/bclexer.cc -- $(LEXCOMPILE) -Pbc_yy

BUILT_SOURCES = budgie/bcparser.h

src_libbugleutils_la_SOURCES = \
	src/gl/gltypes.c src/glreflect.c \
	src/glwintypes.c \
	common/misc.c \
	common/protocol.h common/protocol.c \
	common/hashtable.c include/bugle/hashtable.h \
	common/linkedlist.c include/bugle/linkedlist.h \
	budgielib/internal.c budgielib/internal.h budgielib/reflect.c
nodist_src_libbugleutils_la_SOURCES = \
	budgielib/defines.h budgielib/tables.c \
	src/gltables.c src/gltables.h
src_libbugleutils_la_LIBADD = $(LTLIBOBJS) $(LTLIBTHREAD) $(LTLIBINTL) lib/libgnu.la
src_libbugleutils_la_LDFLAGS = -no-undefined -thread-safe -export-dynamic $(SYMBOLFLAGS) -version-info 5:1:1

# The name of the target is OS-dependent, because on Win32 we want to call the
# library opengl32.dll. Here we just define the inputs
bugle_sources = \
	src/confparse.y src/conflex.l src/conffile.h \
	src/glutils.c src/glstate.c src/glsl.c src/interceptor.c \
	src/dlopen.c src/dlopen.h src/filters.c \
	src/trackcontext.c src/trackdisplaylist.c src/trackobjects.c \
	src/trackbeginend.c src/trackextensions.c \
	src/stats.c src/statslex.l src/statsparse.y \
	src/log.c src/objects.c \
	src/gl/gldump.c src/xevent.c \
	budgielib/addresses.c budgielib/lib.h \
	common/threads.h
bugle_sources_nodist = \
	budgielib/lib.c
bugle_libadd = $(LIBLTDL) src/libbugleutils.la $(GL_LIBS) $(LTLIBTHREAD) $(LTLIBINTL) lib/libgnu.la
bugle_ldflags = -no-undefined -thread-safe -export-dynamic $(SYMBOLFLAGS) -version-info 6:2:1

BUDGIE_BUILT_SRCS = \
        budgielib/lib.c budgielib/tables.c \
        include/budgie/types2.h include/budgie/call.h budgielib/defines.h
BUILT_SOURCES += $(BUDGIE_BUILT_SRCS)

# Generated files that generate other files

# ccache doesn't play nice with -fdump-translation-unit, so we disable it
src/data/gl.tu: src/data/gl.c include/bugle/gl/overrides.h include/bugle/glx/overrides.h
	$(mkdir_p) src/data
	CCACHE_DISABLE=1 $(COMPILE) -fdump-translation-unit -c $(srcdir)/src/data/gl.c -o src/data/gl.o
	rm -f src/data/gl.o
	$(srcdir)/mvtu.sh gl.c src/data/gl.tu

GLBCFILES = gl/gl.bc gl/bc/core1_1.bc gl/bc/glsl1_20.bc \
	gl/bc/GL_ARB_multitexture.bc gl/bc/GL_ARB_imaging.bc \
	gl/bc/GL_ARB_shader_objects.bc gl/bc/GL_ARB_occlusion_query.bc \
	gl/bc/GL_ARB_texture_compression.bc gl/bc/GL_ARB_vertex_buffer_object.bc \
	gl/bc/GL_ARB_vertex_program.bc gl/bc/GL_ATI_draw_buffers.bc \
	gl/bc/GL_EXT_blend_func_separate.bc gl/bc/GL_EXT_draw_buffers2.bc \
	gl/bc/GL_EXT_draw_instanced.bc gl/bc/GL_EXT_draw_range_elements.bc \
	gl/bc/GL_EXT_geometry_shader4.bc gl/bc/GL_EXT_multi_draw_arrays.bc \
	gl/bc/GL_EXT_secondary_color.bc gl/bc/GL_EXT_texture3D.bc \
	gl/bc/GL_EXT_framebuffer_object.bc gl/bc/GL_EXT_gpu_program_parameters.bc \
	gl/bc/GL_NV_transform_feedback.bc \
	gl/bc/alias.bc
GLXBCFILES = glx/glx.bc glx/bc/glx.bc \
	glx/bc/GLX_ARB_get_proc_address.bc glx/bc/GLX_SGI_make_current_read.bc \
	glx/bc/GLX_SGIX_fbconfig.bc glx/bc/GLX_SGIX_pbuffer.bc
WGLBCFILES = wgl/wgl.bc
BCFILES = $(GLBCFILES) $(GLXBCFILES) $(WGLBCFILES) gl-glx.bc gl-wgl.bc

BUILT_SOURCES += src/gltables.h

src/gltables.c: gengl/gengl.perl $(BUDGIE_BUILT_SRCS)
	$(PERL) $(srcdir)/gengl/gengl.perl --mode=c --header=budgielib/defines.h $(GLINCLUDES) > src/gltables.c
src/gltables.h: gengl/gengl.perl $(BUDGIE_BUILT_SRCS)
	$(PERL) $(srcdir)/gengl/gengl.perl --mode=header --header=budgielib/defines.h $(GLINCLUDES) > src/gltables.h
gl/bc/alias.bc: gengl/gengl.perl
	$(mkdir_p) gl/bc
	$(PERL) $(srcdir)/gengl/gengl.perl --mode=alias $(GLINCLUDES) > gl/bc/alias.bc

# Filters
filters_common_la_SOURCES = filters/common.c
filters_common_la_LDFLAGS = $(MODULEFLAGS)
filters_common_la_LIBADD = $(MODULELIBS)

filters_capture_la_SOURCES = filters/capture.c gl2ps/gl2ps.c gl2ps/gl2ps.h
filters_capture_la_CFLAGS = $(AVCODEC_CFLAGS)
filters_capture_la_LIBADD = $(MODULELIBS) $(AVCODEC_LIBS)
filters_capture_la_LDFLAGS = $(MODULEFLAGS)

filters_validate_la_SOURCES = filters/validate.c
filters_validate_la_LDFLAGS = $(MODULEFLAGS)
filters_validate_la_LIBADD = $(MODULELIBS)

filters_trace_la_SOURCES = filters/trace.c
filters_trace_la_LDFLAGS = $(MODULEFLAGS)
filters_trace_la_LIBADD = $(MODULELIBS)

filters_debugger_la_SOURCES = filters/debugger.c
filters_debugger_la_LDFLAGS = $(MODULEFLAGS)
filters_debugger_la_LIBADD = $(MODULELIBS) $(WINSOCK_LIBS)

filters_modify_la_SOURCES = filters/modify.c
filters_modify_la_LDFLAGS = $(MODULEFLAGS)
filters_modify_la_LIBADD = $(MODULELIBS) -lm

filters_stats_basic_la_SOURCES = filters/stats_basic.c
filters_stats_basic_la_LDFLAGS = $(MODULEFLAGS)
filters_stats_basic_la_LIBADD = $(MODULELIBS)

filters_stats_calls_la_SOURCES = filters/stats_calls.c
filters_stats_calls_la_LDFLAGS = $(MODULEFLAGS)
filters_stats_calls_la_LIBADD = $(MODULELIBS)

filters_stats_fragments_la_SOURCES = filters/stats_fragments.c
filters_stats_fragments_la_LDFLAGS = $(MODULEFLAGS)
filters_stats_fragments_la_LIBADD = $(MODULELIBS)

filters_stats_nv_la_SOURCES = filters/stats_nv.c
filters_stats_nv_la_LDFLAGS = $(MODULEFLAGS)
filters_stats_nv_la_LIBADD = $(MODULELIBS)

filters_stats_primitives_la_SOURCES = filters/stats_primitives.c
filters_stats_primitives_la_LDFLAGS = $(MODULEFLAGS)
filters_stats_primitives_la_LIBADD = $(MODULELIBS)

filters_logstats_la_SOURCES = filters/logstats.c
filters_logstats_la_LDFLAGS = $(MODULEFLAGS)
filters_logstats_la_LIBADD = $(MODULELIBS) -lm

BUILT_SOURCES += src/statsparse.h

# Demo/Test programs
check_PROGRAMS = \
	tests/errors tests/threads1 tests/threads2 tests/queries \
	tests/misc tests/setstate tests/showextensions tests/triangles \
	tests/pointers tests/objects tests/textest tests/shadertest \
	tests/pbo tests/interpose tests/dlopen tests/texcomplete
dist_check_SCRIPTS = \
	tests/errors.sh tests/misc.sh tests/queries.sh tests/setstate.sh \
	tests/showextensions.sh tests/pointers.sh tests/triangles.sh \
	tests/pbo.sh tests/interpose.sh tests/dlopen.sh tests/texcomplete.sh \
	tests/comparelog.perl tests/comparelog.sh
tests_errors_SOURCES = tests/errors.c
tests_errors_LDADD = -lglut -lGLU $(GL_LIBS) $(X_PRE_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS)
tests_pointers_SOURCES = tests/pointers.c
tests_pointers_LDADD = $(GLEW_LIBS) -lglut -lGLU $(GL_LIBS) $(X_PRE_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS)
tests_pbo_SOURCES = tests/pbo.c
tests_pbo_LDADD = $(GLEW_LIBS) -lglut -lGLU $(GL_LIBS) $(X_PRE_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS)
tests_threads1_SOURCES = tests/threads1.c
tests_threads1_LDADD = $(GL_LIBS) $(X_PRE_LIBS) $(X_LIBS) $(X_EXTRA_LIBS) -lX11 $(LIBMULTITHREAD)
tests_threads2_SOURCES = tests/threads2.c
tests_threads2_LDADD = $(GL_LIBS) $(X_PRE_LIBS) $(X_LIBS) $(X_EXTRA_LIBS) -lX11 -lpthread $(LIBMULTITHREAD)
tests_queries_SOURCES = tests/queries.c
tests_queries_LDADD = $(GLEW_LIBS) -lglut -lGLU $(GL_LIBS) $(X_PRE_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS)
tests_triangles_SOURCES = tests/triangles.c
tests_triangles_LDADD = $(GLEW_LIBS) -lglut -lGLU $(GL_LIBS) $(X_PRE_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS)
tests_misc_SOURCES = tests/misc.c
tests_misc_LDADD = -lglut -lGLU $(GL_LIBS) $(X_PRE_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS)
tests_interpose_SOURCES = tests/interpose.c
tests_interpose_LDADD = -lglut -lGLU $(GL_LIBS) $(X_PRE_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS)
tests_dlopen_SOURCES = tests/dlopen.c
tests_dlopen_LDADD = $(GL_LIBS) $(X_PRE_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS)
tests_setstate_SOURCES = tests/setstate.c
tests_setstate_LDADD = $(GLEW_LIBS) -lglut -lGLU $(GL_LIBS) $(X_PRE_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS)
tests_showextensions_SOURCES = tests/showextensions.c
tests_showextensions_LDADD = $(GLEW_LIBS) -lglut -lGLU $(GL_LIBS) $(X_PRE_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS)
tests_objects_SOURCES = tests/objects.c
tests_objects_LDADD = $(GLEW_LIBS) -lglut -lGLU $(GL_LIBS) $(X_PRE_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS)
tests_textest_SOURCES = tests/textest.c
tests_textest_LDADD = $(GLEW_LIBS) -lglut -lGLU $(GL_LIBS) $(X_PRE_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS)
tests_shadertest_SOURCES = tests/shadertest.c
tests_shadertest_LDADD = $(GLEW_LIBS) -lglut -lGLU $(GL_LIBS) $(X_PRE_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS)
tests_texcomplete_SOURCES = tests/texcomplete.c
tests_texcomplete_LDADD = $(GLEW_LIBS) -lglut -lGLU $(GL_LIBS) $(X_PRE_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS)
TESTS = tests/errors.sh tests/misc.sh tests/queries.sh tests/setstate.sh \
	tests/showextensions.sh tests/triangles.sh tests/pointers.sh \
	tests/pbo.sh tests/interpose.sh tests/dlopen.sh tests/texcomplete.sh
TESTS_ENVIRONMENT = BUGLE_FILTERS=$(srcdir)/tests/filters BUGLE_STATISTICS=$(srcdir)/tests/statistics BUGLE_FILTER_DIR=filters/.libs BUGLE_LIB=src/.libs/libbugle.so

# Extra files to distribute
EXTRA_DIST = bugle.pc.in $(BCFILES) src/data/gl.c \
	LICENSE \
	doc/filters.html doc/locks.txt \
	doc/protocol.txt doc/extensions.txt \
	doc/examples/filters doc/examples/statistics \
	gl2ps/TODO gl2ps/COPYING.GL2PS gl2ps/COPYING.LGPL gl2ps/gl2ps.pdf \
	gl2ps/gl2psTestSimple.c gl2ps/gl2psTest.c \
	tests/filters tests/statistics \
	m4/gnulib-cache.m4 \
	gnulib-local/lib/xalloc-die.c gnulib-local/modules/xalloc-die

# Note: automake ought to clean the various files in the .libs directories,
# but for some reason does not.
CLEANFILES = \
	$(BUDGIE_BUILT_SRCS) \
	src/gltables.c src/gltables.h \
	src/data/gl.tu gl/bc/alias.bc \
	doc/gldb.1.gz doc/bugle.3.gz \
	gldb/.libs/gldb gldb/.libs/gldb-gui
DISTCLEANFILES = tests/*.log
MAINTAINERCLEANFILES =

### Documentation starts here
XMLLINT_OPTIONS = --noent --noout --xinclude --postvalid
XSLTPROC_OPTIONS = --xinclude --stringparam bugle.version $(PACKAGE_VERSION)

# HTML shared files
htmldocdir = $(htmldir)/html
htmlimagesdir = $(htmldir)/html/images
dist_htmldoc_DATA = doc/DocBook/bugle.css
dist_htmlimages_DATA = \
	doc/DocBook/images/gtk-copy.png \
	doc/DocBook/images/gtk-zoom-in.png \
	doc/DocBook/images/gtk-zoom-out.png \
	doc/DocBook/images/gtk-zoom-fit.png \
	doc/DocBook/images/gtk-zoom-100.png

# Documentation sources
DOCBOOK_MAN_SRCS = \
	doc/DocBook/man/bugle.xml \
	doc/DocBook/man/gldb-gui.xml \
	doc/DocBook/man/gldb.xml \
	doc/DocBook/man/camera.xml \
	doc/DocBook/man/checks.xml \
	doc/DocBook/man/eps.xml \
	doc/DocBook/man/error.xml \
	doc/DocBook/man/extoverride.xml \
	doc/DocBook/man/frontbuffer.xml \
	doc/DocBook/man/log.xml \
	doc/DocBook/man/logstats.xml \
	doc/DocBook/man/screenshot.xml \
	doc/DocBook/man/showerror.xml \
	doc/DocBook/man/showextensions.xml \
	doc/DocBook/man/showstats.xml \
	doc/DocBook/man/statistics.xml \
	doc/DocBook/man/stats_basic.xml \
	doc/DocBook/man/stats_calls.xml \
	doc/DocBook/man/stats_fragments.xml \
	doc/DocBook/man/stats_nv.xml \
	doc/DocBook/man/stats_primitives.xml \
	doc/DocBook/man/trace.xml \
	doc/DocBook/man/unwindstack.xml \
	doc/DocBook/man/wireframe.xml \
	doc/DocBook/man/manpages.xml
DOCBOOK_SRCS = $(DOCBOOK_MAN_SRCS) \
	       doc/DocBook/bugle.xml \
	       doc/DocBook/introduction.xml \
	       doc/DocBook/install.xml \
	       doc/DocBook/extending.xml \
	       doc/DocBook/hacking.xml \
	       doc/DocBook/faq.xml \
	       doc/DocBook/releasenotes.xml \
	       doc/DocBook/changelog.xml \
	       doc/DocBook/bugle.ent
DOCBOOK_MAN_STYLES = doc/DocBook/manpages.xsl
DOCBOOK_STYLES = $(DOCBOOK_MAN_STYLES) \
		 doc/DocBook/xhtml-common.xsl \
		 doc/DocBook/xhtml-chunk.xsl \
		 doc/DocBook/xhtml-single.xsl \
		 doc/DocBook/xhtml-local.xsl \
		 doc/DocBook/xhtml-php.xsl \
		 doc/DocBook/xhtml-chunk-php.xsl
EXTRA_DIST += $(DOCBOOK_SRCS) $(DOCBOOK_STYLES)

# man pages, man format
DOCBOOK_MAN_MANPAGES = \
	doc/man1/gldb-gui.1 \
	doc/man1/gldb.1 \
	doc/man3/bugle.3 \
	doc/man7/bugle-statistics.5 \
	doc/man7/bugle-camera.7 \
	doc/man7/bugle-checks.7 \
	doc/man7/bugle-eps.7 \
	doc/man7/bugle-error.7 \
	doc/man7/bugle-extoverride.7 \
	doc/man7/bugle-frontbuffer.7 \
	doc/man7/bugle-log.7 \
	doc/man7/bugle-logstats.7 \
	doc/man7/bugle-screenshot.7 \
	doc/man7/bugle-showerror.7 \
	doc/man7/bugle-showextensions.7 \
	doc/man7/bugle-showstats.7 \
	doc/man7/bugle-stats_basic.7 \
	doc/man7/bugle-stats_calls.7 \
	doc/man7/bugle-stats_fragments.7 \
	doc/man7/bugle-stats_nv.7 \
	doc/man7/bugle-stats_primitives.7 \
	doc/man7/bugle-trace.7 \
	doc/man7/bugle-unwindstack.7 \
	doc/man7/bugle-wireframe.7
$(DOCBOOK_MAN_MANPAGES): doc/DocBook/man.stamp
	@touch "$@"
doc/DocBook/man.stamp: $(DOCBOOK_MAN_SRCS) doc/DocBook/bugle.xml doc/DocBook/manpages.xsl
	$(XMLLINT) $(XMLLINT_OPTIONS) $(srcdir)/doc/DocBook/bugle.xml
	$(XSLTPROC) $(XSLTPROC_OPTIONS) $(srcdir)/doc/DocBook/manpages.xsl $(srcdir)/doc/DocBook/bugle.xml
	$(mkdir_p) doc/DocBook
	$(mkdir_p) doc/man1
	$(mkdir_p) doc/man3
	$(mkdir_p) doc/man7
	echo timestamp > doc/DocBook/man.stamp
dist_man_MANS = $(DOCBOOK_MAN_MANPAGES)
EXTRA_DIST += doc/DocBook/man.stamp
MAINTAINERCLEANFILES += doc/DocBook/man.stamp $(DOCBOOK_MAN_MANPAGES)

# HTML documentation
DOCBOOK_HTML = doc/html/index.html \
	       doc/html/introduction.html \
	       doc/html/install.html \
	       doc/html/extending.html \
	       doc/html/hacking.html \
	       doc/html/faq.html \
	       doc/html/gldb-gui.1.html \
	       doc/html/gldb.1.html \
	       doc/html/bugle.3.html \
	       doc/html/statistics.5.html \
	       doc/html/camera.7.html \
	       doc/html/checks.7.html \
	       doc/html/eps.7.html \
	       doc/html/error.7.html \
	       doc/html/extoverride.7.html \
	       doc/html/frontbuffer.7.html \
	       doc/html/log.7.html \
	       doc/html/logstats.7.html \
	       doc/html/screenshot.7.html \
	       doc/html/showerror.7.html \
	       doc/html/showextensions.7.html \
	       doc/html/showstats.7.html \
	       doc/html/stats_basic.7.html \
	       doc/html/stats_calls.7.html \
	       doc/html/stats_fragments.7.html \
	       doc/html/stats_nv.7.html \
	       doc/html/stats_primitives.7.html \
	       doc/html/trace.7.html \
	       doc/html/unwindstack.7.html \
	       doc/html/wireframe.7.html
$(DOCBOOK_HTML): doc/DocBook/html.stamp
	@touch $@
doc/DocBook/html.stamp: $(DOCBOOK_SRCS) doc/DocBook/xhtml-chunk.xsl doc/DocBook/xhtml-common.xsl doc/DocBook/xhtml-local.xsl
	$(XMLLINT) $(XMLLINT_OPTIONS) $(srcdir)/doc/DocBook/bugle.xml
	$(XSLTPROC) $(XSLTPROC_OPTIONS) $(srcdir)/doc/DocBook/xhtml-chunk.xsl $(srcdir)/doc/DocBook/bugle.xml
	$(mkdir_p) doc/DocBook
	$(mkdir_p) doc/html
	echo timestamp > doc/DocBook/html.stamp
htmldoc_DATA = $(DOCBOOK_HTML)
EXTRA_DIST += doc/DocBook/html.stamp $(DOCBOOK_HTML)
MAINTAINERCLEANFILES += doc/DocBook/html.stamp $(DOCBOOK_HTML)

# HTML documentation, single document
doc/html/single.html: $(DOCBOOK_SRCS) doc/DocBook/xhtml-single.xsl doc/DocBook/xhtml-common.xsl doc/DocBook/xhtml-local.xsl
	$(XMLLINT) $(XMLLINT_OPTIONS) $(srcdir)/doc/DocBook/bugle.xml
	$(XSLTPROC) $(XSLTPROC_OPTIONS) -o doc/html/single.html $(srcdir)/doc/DocBook/xhtml-single.xsl $(srcdir)/doc/DocBook/bugle.xml
htmldoc_DATA += doc/html/single.html
EXTRA_DIST += doc/html/single.html
MAINTAINERCLEANFILES += doc/html/single.html

libtool: @LIBTOOL_DEPS@
	$(SHELL) ./config.status --recheck


# Window-system interface specific files
if BUGLE_GLWIN_GLX
include $(top_srcdir)/glx/Makefile.am
endif
if BUGLE_GLWIN_WGL
include $(top_srcdir)/wgl/Makefile.am
endif

if BUGLE_BINFMT_PE
MODULELIBS += src/opengl32.la
MODULEFLAGS += -Wl,--export-all-symbols
SYMBOLFLAGS += -Wl,--add-stdcall-alias
lib_LTLIBRARIES += src/opengl32.la

src_opengl32_la_SOURCES = $(bugle_sources)
nodist_src_opengl32_la_SOURCES = $(bugle_sources_nodist)
src_opengl32_la_LIBADD = $(bugle_libadd)
src_opengl32_la_LDFLAGS = $(bugle_ldflags) -module -avoid-version
else
MODULELIBS += src/libbugle.la
lib_LTLIBRARIES += src/libbugle.la

src_libbugle_la_SOURCES = $(bugle_sources)
nodist_src_libbugle_la_SOURCES = $(bugle_sources_nodist)
src_libbugle_la_LIBADD = $(bugle_libadd)
src_libbugle_la_LDFLAGS = $(bugle_ldflags)
endif
