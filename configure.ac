AC_INIT(bugle, 0.0.20040903, bmerry@users.sourceforge.net)
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
AC_CHECK_LIB([readline], [readline], [
  READLINE_LIBS="-lreadline $READLINE_LIBS"], [], [])
AC_CHECK_LIB([history], [add_history], [
  READLINE_LIBS="-lhistory $READLINE_LIBS"], [], [-lreadline])
AC_SUBST(READLINE_LIBS)

THREAD_LIBS=""
AC_CHECK_LIB([pthread], [pthread_create], [
  THREAD_LIBS="-lpthread $THREAD_LIBS"], [], [])
AC_SUBST(THREAD_LIBS)

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
AC_CHECK_HEADERS([readline/readline.h pthread.h])

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
LIBS="-lreadline $LIBS"
AC_CHECK_FUNCS([rl_completion_matches], [], [])
LIBS="$real_libs"

dnl Checks for system services
AC_PATH_XTRA

dnl Set up some extra stuff
dnl FIXME: add a --without-lavc option

test $HAVE_LAVC = 1 || AVCODEC_LIBS=""
AC_SUBST(AVCODEC_LIBS)
AC_DEFINE_UNQUOTED(HAVE_LAVC, $HAVE_LAVC, [non-zero when libavcodec is being used])

dnl Write out files
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
