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

# Checks whether __attribute__((visibility("hidden"))) is supported
AC_DEFUN([BUGLE_C_GCC_VISIBILITY],
         [AC_REQUIRE([AC_PROG_CC])[]dnl
          AC_CACHE_CHECK([for GCC visibility attribute], bugle_cv_c_gcc_visibility,
                         [bugle_cv_c_gcc_visibility=no
                          AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
[
void test_function(void) __attribute__((visibility("hidden")));
], [])], [bugle_cv_c_gcc_visibility=yes])])
          if test $bugle_cv_c_gcc_visibility = yes; then
            AC_DEFINE([BUGLE_GCC_VISIBILITY(x)], [__attribute__((visibility(x)))], [Specify ELF linkage visibility])
          else
            AC_DEFINE([BUGLE_GCC_VISIBILITY(x)], [], [Specify ELF linkage visibility])
          fi
])


# Checks whether __attribute__((format(printf, 1, 2))) is supported
AC_DEFUN([BUGLE_C_GCC_FORMAT_PRINTF],
         [AC_REQUIRE([AC_PROG_CC])[]dnl
          AC_CACHE_CHECK([for GCC format attribute], bugle_cv_c_gcc_format_printf,
                         [bugle_cv_c_gcc_format_printf=no
                          AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
[
void myprintf(const char *format, ...) __attribute__((format(printf, 1, 2)));
], [])], [bugle_cv_c_gcc_format_printf=yes])])
          if test $bugle_cv_c_gcc_format_printf = yes; then
            AC_DEFINE([BUGLE_GCC_FORMAT_PRINTF(a, b)], [__attribute__((format(printf, a, b)))], [Specify printf semantics])
          else
            AC_DEFINE([BUGLE_GCC_FORMAT_PRINTF(a, b)], [], [Specify printf semantics])
          fi
])
