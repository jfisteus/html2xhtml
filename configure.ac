AC_PREREQ(2.61)

AC_INIT([html2xhtml], [1.4])
AC_CONFIG_SRCDIR([configure.ac])
AM_INIT_AUTOMAKE
AC_CONFIG_MACRO_DIR([m4])

AC_CONFIG_HEADERS([config.h])

AC_LANG([C])
AC_PROG_CC
AC_GNU_SOURCE
AM_PROG_LIBTOOL
AM_PROG_LEX
AC_PROG_YACC
AM_ICONV

# Checks for header files.
AC_FUNC_ALLOCA
AC_HEADER_STDC
AC_CHECK_HEADERS([libintl.h malloc.h stddef.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([strcasecmp strstr memmem])

# Avail. at http://www.gnu.org/software/autoconf-archive/ax_func_snprintf.html
AX_FUNC_SNPRINTF

AC_CONFIG_FILES([Makefile src/Makefile doc/Makefile dtdcoder/Makefile charset_coder/Makefile tests/Makefile])
AC_OUTPUT
