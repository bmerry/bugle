AC_INIT(bugle, 0.0.20090411, bmerry@users.sourceforge.net)
AC_CONFIG_SRCDIR([src/interceptor.c])
AM_INIT_AUTOMAKE([1.7 dist-bzip2 subdir-objects])
AM_CONFIG_HEADER(config.h)

dnl Checks for programs
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CPP
AM_PROG_LEX
AC_PROG_YACC
AC_PROG_LN_S
AC_PROG_INSTALL
AM_PROG_MKDIR_P
AC_PROG_MAKE_SET

AC_CANONICAL_HOST
AC_LANG(C)
AC_LIBLTDL_INSTALLABLE(libltdl)
AC_LIBTOOL_DLOPEN
AC_LIBTOOL_WIN32_DLL
AC_PROG_LIBTOOL
AC_CONFIG_SUBDIRS(libltdl lib)
AC_SUBST(LTDLINCL)
AC_SUBST(LIBLTDL)
AC_SUBST(LIBTOOL_DEPS)

AC_PATH_PROG([PERL], [perl], [perl], [$srcdir/missing])

dnl Checks for libraries
AC_PATH_XTRA

AM_PATH_GTK_2_0([2.4.0], [HAVE_GTK=1], [HAVE_GTK=0])
AM_PATH_GTKGLEXT_1_0([1.0.0], [HAVE_GTKGLEXT=1], [HAVE_GTKGLEXT=0])

READLINE_LIBS=""
HAVE_READLINE=1
AC_CHECK_LIB([ncurses], [initscr], [
  READLINE_LIBS="-lncurses $READLINE_LIBS"], [], [])
AC_CHECK_LIB([readline], [readline], [
  READLINE_LIBS="-lreadline $READLINE_LIBS"], [HAVE_READLINE=0], [$READLINE_LIBS])
AC_CHECK_LIB([history], [add_history], [
  READLINE_LIBS="-lhistory $READLINE_LIBS"], [HAVE_READLINE=0], [$READLINE_LIBS])
AC_SUBST(READLINE_LIBS)

HAVE_LAVC=1
AVCODEC_CFLAGS=""
AVCODEC_LIBS=""
PKG_CHECK_MODULES([avcodec], [libavcodec libavformat], [], [HAVE_LAVC=0; AC_MSG_RESULT([no])])
PKG_CHECK_MODULES([swscale], [libswscale], [HAVE_LIBSWSCALE=1], [HAVE_LIBSWSCALE=0])

GLEW_LIBS=""
HAVE_GLEW=0
AC_CHECK_LIB([GLEW], [glewInit], [
  GLEW_LIBS="-lGLEW" && HAVE_GLEW=1], [], [])
AC_CHECK_LIB([GLEW32], [glewInit], [
  GLEW_LIBS="-lGLEW32" && HAVE_GLEW=1], [], [])
AC_SUBST(GLEW_LIBS)

dnl Checks for header files
AC_HEADER_STDC
AC_CHECK_HEADERS([sys/types.h stdlib.h stddef.h NVPerfSDK.h dlfcn.h sys/select.h])
AC_CHECK_HEADERS([readline/readline.h readline/history.h], [], [HAVE_READLINE=0])
# ffmpeg moves the headers around
AC_CHECK_HEADERS([libavcodec/avcodec.h libavformat/avformat.h libswscale/swscale.h])

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

dnl Checks for library functions
AC_CHECK_FUNCS([strrchr vsnprintf])
AC_CHECK_FUNCS([cosf sinf finite isfinite nan isnan siglongjmp])
# Readline used to call this completion_matches
real_libs="$LIBS"
LIBS="$READLINE_LIBS $LIBS"
AC_CHECK_FUNCS([rl_completion_matches], [], [])
LIBS="$real_libs"

dnl Checks for system services
AC_PATH_XTRA
AC_ARG_VAR([XSLTPROC], [Path to xsltproc])
AC_ARG_VAR([XMLLINT], [Path to xmllint])
AC_PATH_PROG([XSLTPROC], [xsltproc], [true])
AC_PATH_PROG([XMLLINT], [xmllint], [true])

dnl Set up some extra stuff

ENABLE_INPUT=1
AC_ARG_ENABLE([input], AC_HELP_STRING([--disable-input], [disable key and mouse interception]),
              [test $enableval = no && ENABLE_INPUT=0 && BUGLE_WINSYS=BUGLE_WINSYS_NONE], [])
AC_DEFINE_UNQUOTED(ENABLE_INPUT, $ENABLE_INPUT, [non-zero when key and mouse events should be intercepted])

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
GLDB=gldb/gldb$EXEEXT
GLDB_GUI=gldb/gldb-gui$EXEEXT
test $HAVE_GTK == 1 || GLDB_GUI=""
test $BUGLE_OSAPI == "BUGLE_OSAPI_POSIX" || GLDB=""
AC_SUBST(GLDB)
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

dnl Write out files
AC_CONFIG_FILES([SConstruct Makefile bugle.pc])
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
case $ENABLE_INPUT in
  0) echo "        event interception: no" ;;
  1) echo "        event interception: yes" ;;
esac
