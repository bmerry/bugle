AC_INIT(bugle, 0.1, bmerry@cs.uct.ac.za)
AC_CONFIG_SRCDIR([budgie/budgie.cpp])
AM_INIT_AUTOMAKE(bugle, 0.1)
AM_CONFIG_HEADER(config.h)

dnl Checks for programs
AC_PROG_CC_STDC
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_LEX
AC_PROG_LN_S
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL

dnl Checks for libraries

dnl Checks for header files
AC_HEADER_STDC
AC_HEADER_DIRENT
AC_HEADER_STDBOOL

dnl Checks for types
AC_CHECK_TYPE(ptrdiff_t, long int)
AC_CHECK_TYPE([long long])
AC_TYPE_SIZE_T
AC_C_LONG_DOUBLE

dnl Checks for structures

dnl Checks for compiler characteristics
# AC_C_CONST
AC_C_VOLATILE
AC_C_INLINE
AC_C_CHAR_UNSIGNED

dnl Checks for library functions
AC_CHECK_FUNCS([regcomp strrchr])
AC_FUNC_MEMCMP

dnl Checks for system services

dnl Write out files
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
