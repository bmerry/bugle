AC_INIT(bugle, 0.0.20040609, bmerry@users.sourceforge.net)
AC_CONFIG_SRCDIR([src/interceptor.c])
AM_INIT_AUTOMAKE(bugle, 0.0.20040609)
AM_CONFIG_HEADER(config.h)

dnl Checks for programs
AC_PROG_CC_STDC
AC_PROG_CXX
AC_PROG_AWK
AM_PROG_LEX
AC_PROG_YACC
AC_PROG_LN_S
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL
AC_LANG(C)

dnl Checks for libraries
AC_CHECK_LIB([readline], [readline], [], [], [])
AC_CHECK_LIB([history], [add_history], [], [], [])

dnl Checks for header files
AC_HEADER_STDC
AC_HEADER_DIRENT
AC_HEADER_STDBOOL
AC_CHECK_HEADERS([sys/types.h stdlib.h stddef.h regex.h])
AC_CHECK_HEADERS([readline/readline.h])

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
BUGLE_CPP_DEFINES

dnl Checks for library functions
AC_CHECK_FUNCS([regcomp strrchr vasprintf vsnprintf])
AC_REPLACE_FUNCS([bsearch])
AC_FUNC_MEMCMP

dnl Checks for system services
AC_PATH_XTRA
BUGLE_PATH_GLINCLUDES

dnl Set up some extra stuff
AM_CONDITIONAL(PREPROCESS_GL, test x$CPP_DEFINES = xyes)

dnl Write out files
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
