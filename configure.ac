AC_INIT(bugle, 0.0.20041123, bmerry@users.sourceforge.net)
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
AC_PROG_MAKE_SET
AM_PROG_LIBTOOL
AC_PATH_PROG([PERL], [perl], [perl], [$srcdir/missing])
AC_LANG(C)

dnl Checks for libraries
AC_PATH_XTRA

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

AVCODEC_LIBS=""
HAVE_LAVC=1
AC_CHECK_LIB([avcodec], [avcodec_open], [
  AVCODEC_LIBS="-lavcodec $AVCODEC_LIBS"], [HAVE_LAVC=0], [])
AC_CHECK_LIB([avformat], [av_write_frame], [
  AVCODEC_LIBS="-lavformat $AVCODEC_LIBS"], [HAVE_LAVC=0], [$AVCODEC_LIBS])

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
AC_CHECK_HEADERS([sys/types.h stdlib.h stddef.h regex.h])
AC_CHECK_HEADERS([ffmpeg/avformat.h ffmpeg/avcodec.h], [], [HAVE_LAVC=0])
AC_CHECK_HEADERS([readline/readline.h readline/history.h], [], [HAVE_READLINE=0])
AC_CHECK_HEADERS([pthread.h], [], [THREADS=BUGLE_THREADS_SINGLE])

GLINCLUDES="`CPP=\"$CPP\" $PERL -w $srcdir/gentokens/find_header.perl GL/gl.h` \
`CPP=\"$CPP\" $PERL -w $srcdir/gentokens/find_header.perl GL/glx.h` \
`CPP=\"$CPP\" $PERL -w $srcdir/gentokens/find_header.perl GL/glext.h` \
`CPP=\"$CPP\" $PERL -w $srcdir/gentokens/find_header.perl GL/glxext.h`"
AC_SUBST(GLINCLUDES)

dnl Checks for types
AC_CHECK_TYPE(ptrdiff_t, long int)
AC_CHECK_TYPE([long long])
AC_TYPE_SIZE_T
AC_C_LONG_DOUBLE

dnl Checks for structures

dnl Checks for compiler characteristics
AC_C_VOLATILE
AC_C_INLINE
AC_C_CHAR_UNSIGNED

dnl Checks for library functions
AC_CHECK_FUNCS([regcomp strrchr vasprintf vsnprintf open_memstream])
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

AC_ARG_WITH([lavc], AC_HELP_STRING([--without-lavc], [do not use libavcodec]),
            [test $withval = no && HAVE_LAVC=0], [])
test $HAVE_LAVC = 1 || AVCODEC_LIBS=""
AC_SUBST(AVCODEC_LIBS)
AC_DEFINE_UNQUOTED(HAVE_LAVC, $HAVE_LAVC, [non-zero when libavcodec is being used])

AC_ARG_WITH([readline], AC_HELP_STRING([--without-readline], [do not use libreadline]),
            [test $withval = no && HAVE_READLINE=0], [])
test $HAVE_READLINE = 1 || READLINE_LIBS=""
AC_SUBST(HAVE_READLINE)
AC_DEFINE_UNQUOTED(HAVE_READLINE, $HAVE_READLINE, [non-zero when all the requirements for the readline library are met])

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
case $THREADS in
  BUGLE_THREADS_PTHREADS) echo "        threads: pthreads" ;;
  BUGLE_THREADS_SINGLE) echo "        threads: single-threaded" ;;
esac
