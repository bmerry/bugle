# Checks whether pthread_t can be converted to unsigned long
AC_DEFUN([BUGLE_TYPE_PTHREAD_T_INTEGRAL],
         [AC_REQUIRE([AC_PROG_CC])[]dnl
          AC_CHECK_HEADERS([pthread.h])
          AC_CACHE_CHECK([whether pthread_t exists and is integral], bugle_cv_type_pthread_t_integral,
                         [bugle_cv_type_pthread_t_integral=no
                          AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
[
#if HAVE_PTHREAD_H
# include <pthread.h>
#else
# error "pthread.h not available"
#endif
],[
pthread_t testa;
unsigned long testb;
testb = testa;
])], [bugle_cv_type_pthread_t_integral=yes])])
          if test $bugle_cv_type_pthread_t_integral = yes; then
            AC_DEFINE([BUGLE_PTHREAD_T_INTEGRAL], 1, [Define if pthread_t is type_compatible with unsigned long])
          fi
])
