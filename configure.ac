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

dnl Checks for types
AC_CHECK_TYPES([ptrdiff_t],,[AC_MSG_ERROR([you are missing the ptrdiff_t type])])

dnl Checks for compiler characteristics
AC_C_CONST
AC_C_VOLATILE

dnl Checks for system services

dnl Write out files
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
