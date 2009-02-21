AC_INIT(bugle, 0.0.20090221, bmerry@users.sourceforge.net)
AC_CONFIG_SRCDIR([budgie.cpp])
AM_INIT_AUTOMAKE([1.7 subdir-objects])

dnl Checks for programs
AC_PROG_CXX
AM_PROG_LEX
AC_PROG_YACC

AC_LANG(C++)

dnl Checks for libraries
real_libs="$LIBS"
REGEX_LIBS=""
AC_SEARCH_LIBS([regexec], [regex], [REGEX_LIBS="$LIBS"], [AC_MSG_ERROR([regex is required])])
LIBS="$real_libs"
AC_SUBST(REGEX_LIBS)

dnl Checks for header files
AC_CHECK_HEADERS([regex.h])

dnl Checks for types
AC_TYPE_SIZE_T

dnl Checks for structures

dnl Checks for compiler characteristics
AC_C_INLINE

dnl Checks for library functions
AC_CHECK_FUNCS([regcomp strrchr])

dnl Checks for system services

dnl Write out files
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
