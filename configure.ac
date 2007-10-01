AC_INIT(bugle, 0.0.20070718, bmerry@users.sourceforge.net)
AC_CONFIG_SRCDIR([src/interceptor.c])
AM_INIT_AUTOMAKE([1.7 dist-bzip2 subdir-objects])
AM_CONFIG_HEADER(config.h)

dnl Checks for programs
AC_PROG_CC_STDC
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

AM_PATH_GTK_2_0([2.6.0], [HAVE_GTK2_6=1], [HAVE_GTK2_6=0])
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

THREAD_LIBS=""
THREADS=BUGLE_THREADS_PTHREADS
AC_CHECK_LIB([pthread], [pthread_create], [
  THREAD_LIBS="-lpthread $THREAD_LIBS"], [THREADS=BUGLE_THREADS_SINGLE], [])

HAVE_LAVC=1
AVCODEC_CFLAGS=""
AVCODEC_LIBS=""
PKG_CHECK_MODULES([avcodec], [libavcodec libavformat], [], [HAVE_LAVC=0; AC_MSG_RESULT([no])])

# Sadly, FreeBSD does not implement libraries depending on libraries, and
# OpenGL uses X and maths functions. Hence the -lm and all the X rubbish.
GL_LIBS=""
AC_CHECK_LIB([GL], [glBegin], [GL_LIBS="-lGL -lm $GL_LIBS"], [], [$X_PRE_LIBS $X_LIBS $X_XTRA_LIBS -lm])
AC_SUBST(GL_LIBS)
if test "x$GL_LIBS" = "x"; then 
  AC_MSG_ERROR([An OpenGL library is required])
fi

dnl Checks for header files
AC_HEADER_STDC
AC_HEADER_DIRENT
AC_HEADER_STDBOOL
AC_CHECK_HEADERS([sys/types.h stdlib.h stddef.h regex.h sys/select.h NVPerfSDK.h dlfcn.h])
AC_CHECK_HEADERS([readline/readline.h readline/history.h], [], [HAVE_READLINE=0])
AC_CHECK_HEADERS([pthread.h], [], [THREADS=BUGLE_THREADS_SINGLE])

GLEXTINCLUDE="`CPP=\"$CPP\" srcdir=\"$srcdir\" $PERL -w $srcdir/gentokens/find_header.perl GL/glext.h`"
AC_SUBST(GLEXTINCLUDE)
GLINCLUDES="`CPP=\"$CPP\" srcdir=\"$srcdir\" $PERL -w $srcdir/gentokens/find_header.perl GL/gl.h` \
`CPP=\"$CPP\" srcdir=\"$srcdir\" $PERL -w $srcdir/gentokens/find_header.perl GL/glx.h` \
`CPP=\"$CPP\" srcdir=\"$srcdir\" $PERL -w $srcdir/gentokens/find_header.perl GL/glext.h` \
`CPP=\"$CPP\" srcdir=\"$srcdir\" $PERL -w $srcdir/gentokens/find_header.perl GL/glxext.h`"
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
BUGLE_C_GCC_VISIBILITY
BUGLE_C_GCC_HIDDEN_ALIAS
BUGLE_C_GCC_FORMAT_PRINTF
BUGLE_C_GCC_CONSTRUCTOR_ATTRIBUTE

dnl Checks for library functions
AC_CHECK_FUNCS([regcomp strrchr vasprintf vsnprintf open_memstream pthread_kill])
AC_CHECK_FUNCS([cosf sinf finite isfinite nan isnan])
AC_REPLACE_FUNCS([bsearch])
AC_FUNC_MEMCMP
# Readline used to call this completion_matches
real_libs="$LIBS"
LIBS="$READLINE_LIBS $LIBS"
AC_CHECK_FUNCS([rl_completion_matches], [], [])
LIBS="$real_libs"

dnl Checks for system services
AC_PATH_XTRA

dnl Set up some extra stuff

ENABLE_XEVENT=1
AC_ARG_ENABLE([xevent], AC_HELP_STRING([--disable-xevent], [disable key and mouse interception]),
              [test $enableval = no && ENABLE_XEVENT=0], [])
AC_DEFINE_UNQUOTED(ENABLE_XEVENT, $ENABLE_XEVENT, [non-zero when X key and mouse events should be intercepted])

AC_ARG_WITH([lavc], AC_HELP_STRING([--without-lavc], [do not use libavcodec]),
            [test $withval = no && HAVE_LAVC=0], [])
AVCODEC_LIBS="$avcodec_LIBS"
AVCODEC_CFLAGS="$avcodec_CFLAGS"
test $HAVE_LAVC = 1 || AVCODEC_LIBS=""
test $HAVE_LAVC = 1 || AVCODEC_CFLAGS=""
AC_SUBST(AVCODEC_LIBS)
AC_SUBST(AVCODEC_CFLAGS)
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
AC_DEFINE_UNQUOTED(HAVE_GTK2_6, $HAVE_GTK2_6, [non-zero when GTK+ 2.6 or later is available])

AC_ARG_WITH([gtkglext], AC_HELP_STRING([--without-gtkglext], [do not use OpenGL within the GUI debugger]),
            [ test $withval = no && HAVE_GTKGLEXT=0], [])
AM_CONDITIONAL(HAVE_GTKGLEXT, test x$HAVE_GTKGLEXT = x1)
AC_DEFINE_UNQUOTED(HAVE_GTKGLEXT, $HAVE_GTKGLEXT, [non-zero when all the requirements for GtkGLExt are met])

AC_ARG_WITH([threads], AC_HELP_STRING([--with-threads=pthreads|single], [choose threading model [[auto]]]), [
case $withval in
  pthreads) THREADS=BUGLE_THREADS_PTHREADS ;;
  single) THREADS=BUGLE_THREADS_SINGLE ;;
  *) AC_MSG_ERROR([--with-threads must be either pthreads or single]) ;;
esac], [])
test $THREADS != BUGLE_THREADS_SINGLE || THREAD_LIBS=""
AC_SUBST(THREAD_LIBS)
AC_DEFINE_UNQUOTED(THREADS, $THREADS, [threading model])

dnl Write out files
AC_CONFIG_FILES([Makefile gl.bc])
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
     0) echo "        GUI: yes (without OpenGL - install GtkGLExt for OpenGL support)" ;;
     1) echo "        GUI: yes (with OpenGL)" ;;
     esac ;;
esac
case $THREADS in
  BUGLE_THREADS_PTHREADS) echo "        threads: pthreads" ;;
  BUGLE_THREADS_SINGLE) echo "        threads: single-threaded" ;;
esac
case $ENABLE_XEVENT in
  0) echo "        X event interception: no" ;;
  1) echo "        X event interception: yes" ;;
esac
if [[ $HAVE_GTK = 1 ]] && [[ $HAVE_GTK2_6 = 0 ]]
then
  echo "        WARNING: You have a pre-2.6 GTK+. Some features will be disabled."
fi
