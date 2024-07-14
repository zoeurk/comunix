#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([comunix], [0.1~1], [zoeurk@gmail.com], , [vasytespointnet.totalementfake])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
# FIXME: Replace `main' with a function in `-lcomunix':
### NOT UNDERSTAND
#AC_CHECK_LIB([comunix] [main])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h sys/socket.h unistd.h stdlib.h string.h])
LT_INIT
AM_INIT_AUTOMAKE([foreign])
AM_MAINTAINER_MODE([enable])
AC_CONFIG_MACRO_DIRS([m4])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([socket strtoul])

AC_CONFIG_FILES([Makefile
                 lib/Makefile
                 main/Makefile])
AC_OUTPUT
