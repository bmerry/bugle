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

# Checks whether __attribute__((format(printf, 1, 2))) is supported
AC_DEFUN([BUGLE_C_ATTRIBUTE_FORMAT_PRINTF],
         [AC_REQUIRE([AC_PROG_CC])[]dnl
          AC_CACHE_CHECK([for GCC format attribute], bugle_cv_c_attribute_format_printf,
                         [bugle_cv_c_attribute_format_printf=no
                          AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
[
void myprintf(const char *format, ...) __attribute__((format(printf, 1, 2)));
], [])], [bugle_cv_c_attribute_format_printf=yes])])
          if test $bugle_cv_c_attribute_format_printf = yes; then
            AC_DEFINE([BUGLE_HAVE_ATTRIBUTE_FORMAT_PRINTF], [1], [Define if attribute((format(printf,a,b))) is available])
          fi
])


# Checks whether we can define a hidden alias with GCC attribute magic
AC_DEFUN([BUGLE_C_ATTRIBUTE_HIDDEN_ALIAS],
         [AC_REQUIRE([AC_PROG_CC])[]dnl
          AC_CACHE_CHECK([for GCC alias and visibility attributes], bugle_cv_c_attribute_hidden_alias,
                         [bugle_cv_c_attribute_hidden_alias=no
                          AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
[
extern void mypublic(int x) {}
extern __typeof(mypublic) myalias __attribute__((alias("mypublic"), visibility("hidden")));
#if defined(_WIN32) || defined(__CYGWIN__)
# error "Cygwin accepts but ignores hidden visibility"
#endif
], [])], [bugle_cv_c_attribute_hidden_alias=yes])])
         if test $bugle_cv_c_attribute_hidden_alias = yes; then
           AC_DEFINE([BUGLE_HAVE_ATTRIBUTE_HIDDEN_ALIAS], [1], [Define if it is possible to define hidden aliases with GCC attributes])
         fi
])


# Checks whether we can define startup code with GCC attribute magic
AC_DEFUN([BUGLE_C_ATTRIBUTE_CONSTRUCTOR],
         [AC_REQUIRE([AC_PROG_CC])[]dnl
          AC_CACHE_CHECK([for GCC constructor attribute], bugle_cv_c_attribute_constructor_attribute,
                         [bugle_cv_c_attribute_constructor_attribute=no
                          AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
[
static void constructor(void) __attribute__((constructor));
static void constructor(void)
{
}
], [])], [bugle_cv_c_attribute_constructor_attribute=yes])])
         if test $bugle_cv_c_attribute_constructor_attribute = yes; then
           AC_DEFINE([BUGLE_ATTRIBUTE_CONSTRUCTOR_ATTRIBUTE], [__attribute__((constructor))], [function declaration suffix for constructors])
           AC_DEFINE([BUGLE_HAVE_ATTRIBUTE_CONSTRUCTOR], 1, [Define if __attribute__((constructor)) is supported])
         else
           AC_DEFINE([BUGLE_ATTRIBUTE_CONSTRUCTOR_ATTRIBUTE], [], [function declaration suffix for constructors])
         fi
])
