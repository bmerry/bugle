AC_INIT(bugle, 0.0.20080303, bmerry@users.sourceforge.net)
AC_CONFIG_SRCDIR([src/interceptor.c])
AM_INIT_AUTOMAKE([1.7 dist-bzip2 subdir-objects])
AM_CONFIG_HEADER(config.h)

bugle_target=gl-wgl

dnl Checks for programs
AC_PROG_CC
gl_EARLY
AM_PROG_CC_C_O
AC_PROG_CPP
AC_PROG_CXX
AM_PROG_LEX
AC_PROG_YACC
AC_PROG_LN_S
AC_PROG_INSTALL
AM_PROG_MKDIR_P
AC_PROG_MAKE_SET

AC_LANG(C)
AC_LIBLTDL_INSTALLABLE
AC_LIBTOOL_DLOPEN
AC_PROG_LIBTOOL
AC_CONFIG_SUBDIRS(libltdl)
AC_SUBST(LTDLINCL)
AC_SUBST(LIBLTDL)
AC_SUBST(LIBTOOL_DEPS)

AC_PATH_PROG([PERL], [perl], [perl], [$srcdir/missing])

dnl Checks for libraries
AC_PATH_XTRA

if test "$bugle_target" = "gl-wgl"; then
  # Disable for now since it causes problems for me
  HAVE_GTK=0
else
  AM_PATH_GTK_2_0([2.4.0], [HAVE_GTK=1], [HAVE_GTK=0])
  AM_PATH_GTKGLEXT_1_0([1.0.0], [HAVE_GTKGLEXT=1], [HAVE_GTKGLEXT=0])
fi

READLINE_LIBS=""
HAVE_READLINE=1
AC_CHECK_LIB([ncurses], [initscr], [
  READLINE_LIBS="-lncurses $READLINE_LIBS"], [], [])
AC_CHECK_LIB([readline], [readline], [
  READLINE_LIBS="-lreadline $READLINE_LIBS"], [HAVE_READLINE=0], [$READLINE_LIBS])
AC_CHECK_LIB([history], [add_history], [
  READLINE_LIBS="-lhistory $READLINE_LIBS"], [HAVE_READLINE=0], [$READLINE_LIBS])
AC_SUBST(READLINE_LIBS)

AC_SEARCH_LIBS([regexec], [regex], [], [AC_MSG_ERROR([regex is required])])

HAVE_LAVC=1
AVCODEC_CFLAGS=""
AVCODEC_LIBS=""
PKG_CHECK_MODULES([avcodec], [libavcodec libavformat], [], [HAVE_LAVC=0; AC_MSG_RESULT([no])])
PKG_CHECK_MODULES([swscale], [libswscale], [HAVE_LIBSWSCALE=1], [HAVE_LIBSWSCALE=0])

# Sadly, FreeBSD does not implement libraries depending on libraries, and
# OpenGL uses X and maths functions. Hence the -lm and all the X rubbish.
GL_LIBS=""
if test "$bugle_target" = "gl-wgl"
then
  # AC_CHECK_LIB fails because the autoconf test does not specify __stdcall
  # AC_CHECK_LIB([opengl32], [glBegin], [GL_LIBS="-lopengl32 $GL_LIBS"], [], [])
  GL_LIBS="-lopengl32"
else
  AC_CHECK_LIB([GL], [glBegin], [GL_LIBS="-lGL -lm $GL_LIBS"], [], [$X_PRE_LIBS $X_LIBS $X_XTRA_LIBS -lm])
fi
AC_SUBST(GL_LIBS)
if test "x$GL_LIBS" = "x"; then 
  AC_MSG_ERROR([An OpenGL library is required])
fi

GLEW_LIBS=""
HAVE_GLEW=1
AC_CHECK_LIB([GLEW], [glewInit], [
  GLEW_LIBS="-lGLEW"], [HAVE_GLEW=0], [])
AC_SUBST(GLEW_LIBS)

dnl Checks for header files
gl_INIT
AC_HEADER_STDC
AC_HEADER_DIRENT
AC_HEADER_STDBOOL
AC_CHECK_HEADERS([sys/types.h stdlib.h stddef.h regex.h sys/select.h arpa/inet.h netinet/in.h NVPerfSDK.h dlfcn.h])
AC_CHECK_HEADERS([readline/readline.h readline/history.h], [], [HAVE_READLINE=0])

GLEXTINCLUDE="`CPP=\"$CPP\" srcdir=\"$srcdir\" $PERL -w $srcdir/gengl/find_header.perl GL/glext.h`"
AC_SUBST(GLEXTINCLUDE)

if test "$bugle_target" = "gl-wgl"
then
    GLINCLUDES="`CPP=\"$CPP\" srcdir=\"$srcdir\" $PERL -w $srcdir/gengl/find_header.perl GL/gl.h` \
    `CPP=\"$CPP\" srcdir=\"$srcdir\" $PERL -w $srcdir/gengl/find_header.perl wingdi.h` \
    `CPP=\"$CPP\" srcdir=\"$srcdir\" $PERL -w $srcdir/gengl/find_header.perl GL/glext.h`"
else
    GLINCLUDES="`CPP=\"$CPP\" srcdir=\"$srcdir\" $PERL -w $srcdir/gengl/find_header.perl GL/gl.h` \
    `CPP=\"$CPP\" srcdir=\"$srcdir\" $PERL -w $srcdir/gengl/find_header.perl GL/glx.h` \
    `CPP=\"$CPP\" srcdir=\"$srcdir\" $PERL -w $srcdir/gengl/find_header.perl GL/glext.h` \
    `CPP=\"$CPP\" srcdir=\"$srcdir\" $PERL -w $srcdir/gengl/find_header.perl GL/glxext.h`"
fi
AC_SUBST(GLINCLUDES)

dnl Checks for types
AC_CHECK_TYPE(ptrdiff_t, long int)
AC_CHECK_TYPE([long long])
AC_TYPE_SIZE_T
AC_C_LONG_DOUBLE
BUGLE_TYPE_PTHREAD_T_INTEGRAL

dnl Checks for structures

dnl Checks for compiler characteristics
AC_C_VOLATILE
AC_C_INLINE
AC_C_CHAR_UNSIGNED
BUGLE_C_ATTRIBUTE_HIDDEN_ALIAS
BUGLE_C_ATTRIBUTE_FORMAT_PRINTF
BUGLE_C_ATTRIBUTE_CONSTRUCTOR
BUGLE_C_PRAGMA_WEAK

dnl Checks for library functions
AC_CHECK_FUNCS([regcomp strrchr vsnprintf open_memstream])
AC_CHECK_FUNCS([cosf sinf finite isfinite nan isnan])
# Readline used to call this completion_matches
real_libs="$LIBS"
LIBS="$READLINE_LIBS $LIBS"
AC_CHECK_FUNCS([rl_completion_matches], [], [])
LIBS="$real_libs"

dnl Checks for system services
AC_PATH_XTRA
AC_ARG_VAR([XSLTPROC], [Path to xsltproc])
AC_ARG_VAR([XMLLINT], [Path to xmllint])
AC_PATH_PROG([XSLTPROC], [xsltproc], [xsltproc_is_required_to_update_documentation])
AC_PATH_PROG([XMLLINT], [xmllint], [true])

dnl Set up some extra stuff

ENABLE_XEVENT=1
AC_ARG_ENABLE([xevent], AC_HELP_STRING([--disable-xevent], [disable key and mouse interception]),
              [test $enableval = no && ENABLE_XEVENT=0], [])
AC_DEFINE_UNQUOTED(ENABLE_XEVENT, $ENABLE_XEVENT, [non-zero when X key and mouse events should be intercepted])

AC_ARG_WITH([lavc], AC_HELP_STRING([--without-lavc], [do not use libavcodec]),
            [test $withval = no && HAVE_LAVC=0], [])
AVCODEC_LIBS="$avcodec_LIBS $swscale_LIBS"
AVCODEC_CFLAGS="$avcodec_CFLAGS $swscale_CFLAGS"
test $HAVE_LAVC = 1 || AVCODEC_LIBS=""
test $HAVE_LAVC = 1 || AVCODEC_CFLAGS=""
AC_SUBST(AVCODEC_LIBS)
AC_SUBST(AVCODEC_CFLAGS)
AC_DEFINE_UNQUOTED(HAVE_LIBSWSCALE, $HAVE_LIBSWSCALE, [non-zero when libswscale is available])
AC_DEFINE_UNQUOTED(HAVE_LAVC, $HAVE_LAVC, [non-zero when libavcodec is being used])

AC_ARG_WITH([readline], AC_HELP_STRING([--without-readline], [do not use libreadline]),
            [test $withval = no && HAVE_READLINE=0], [])
test $HAVE_READLINE = 1 || READLINE_LIBS=""
AC_SUBST(HAVE_READLINE)
AC_DEFINE_UNQUOTED(HAVE_READLINE, $HAVE_READLINE, [non-zero when all the requirements for the readline library are met])

AC_ARG_WITH([gtk], AC_HELP_STRING([--without-gtk], [do not build the GUI debugger]),
            [test $withval = no && HAVE_GTK=0], [])
GLDB_GUI=gldb/gldb-gui
test $HAVE_GTK == 1 || GLDB_GUI=""
AC_SUBST(GLDB_GUI)

AC_ARG_WITH([gtkglext], AC_HELP_STRING([--without-gtkglext], [do not use OpenGL within the GUI debugger]),
            [ test $withval = no && HAVE_GTKGLEXT=0], [])
AM_CONDITIONAL(HAVE_GTKGLEXT, test x$HAVE_GTKGLEXT = x1)
if test x$HAVE_GLEW = x0
then
    HAVE_GTKGLEXT=0
fi
if test x$HAVE_GTKGLEXT = x0
then
    HAVE_GLEW=0
    GLEW_LIBS=""
    GTKGLEXT_LIBS=""
fi
AC_DEFINE_UNQUOTED(HAVE_GTKGLEXT, $HAVE_GTKGLEXT, [non-zero when all the requirements for GtkGLExt are met])

AM_CONDITIONAL(USING_GLX, test "$bugle_target" = "gl-glx")
AM_CONDITIONAL(USING_X11, test "$bugle_target" = "gl-glx")
AM_CONDITIONAL(USING_WGL, test "$bugle_target" = "gl-wgl")
AM_CONDITIONAL(USING_WIN32, test "$bugle_target" = "gl-wgl")

dnl Write out files
AC_CONFIG_FILES([Makefile lib/Makefile bugle.pc])
AC_OUTPUT

dnl Display configuration
echo
echo "Configuration:"
case $HAVE_LAVC in
  0) echo "        libavcodec: no" ;;
  *) echo "        libavcodec: yes" ;;
esac
case $HAVE_READLINE in
  0) echo "        readline: no" ;;
  1) echo "        readline: yes" ;;
esac
case $HAVE_GTK in
  0) echo "        GUI: no" ;;
  1) case $HAVE_GTKGLEXT in
     0) echo "        GUI: yes (without OpenGL - install GtkGLExt and GLEW for OpenGL support)" ;;
     1) echo "        GUI: yes (with OpenGL)" ;;
     esac ;;
esac
case $ENABLE_XEVENT in
  0) echo "        X event interception: no" ;;
  1) echo "        X event interception: yes" ;;
esac
