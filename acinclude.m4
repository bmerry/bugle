dnl Check whether we can use gcc -E -g3 to extract #defines from header files

AC_DEFUN([BUGLE_CPP_DEFINES],
[
AC_PROG_CPP
bugle_old_CPPFLAGS="$CPPFLAGS"
AC_CACHE_CHECK([how to extract defines from header files],
               [bugle_cv_cpp_defines_flags],
    [CPPFLAGS="$CPPFLAGS -g3"
    AC_EGREP_CPP([^#define +TEST_DEFINE +1], [
    #define TEST_DEFINE 1
    ], [bugle_cv_cpp_defines_flags="-g3"], [bugle_cv_cpp_defines_flags="no"])
    CPPFLAGS="$bugle_old_CPPFLAGS"])
if test "x$bugle_cv_cpp_defines_flags" != "xno"; then
    CPP_DEFINES_FLAGS="$bugle_cv_cpp_defines_flags"
    CPP_DEFINES=yes
else
    CPP_DEFINES_FLAGS=""
    CPP_DEFINES=no
fi
AC_SUBST(CPP_DEFINES)
AC_SUBST(CPP_DEFINES_FLAGS)
])

AC_DEFUN([BUGLE_PATH_GLINCLUDES],
[
GLINCLUDE_PATH=/usr/include/GL
AC_CHECK_FILE([/usr/X11R6/include/GL/gl.h], [GLINCLUDE_PATH=/usr/X11R6/include/GL], [])
AC_ARG_WITH([gl],
            AC_HELP_STRING([--with-gl=DIR],
                           [GL headers are in DIR]),
            [bugle_cv_path_gl="$withval"],
            [])
if test "x$CPP_DEFINES" != "xyes"; then
    AC_CACHE_CHECK([for path to OpenGL headers],
                   [bugle_cv_path_gl],
                   [bugle_cv_path_gl=$GLINCLUDE_PATH])
    GLINCLUDE_PATH="$bugle_cv_path_gl"
fi
AC_SUBST(GLINCLUDE_PATH)
])
