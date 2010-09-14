#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT
AC_LANG(C)
LZFS_AC_META
AC_CONFIG_AUX_DIR([config])
AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE([$LZFS_META_NAME], [$LZFS_META_VERSION])
AC_CONFIG_HEADERS([lzfs_config.h])
LZFS_AC_CONFIG_KERNEL
ZFS_AC_PACKAGE
AM_MAINTAINER_MODE

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([sys/mntent.h sys/vfs.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T

# Checks for library functions.
AC_CHECK_FUNCS([bzero memset strchr])

AC_CONFIG_FILES([
	Makefile
	module/Makefile
	etc/Makefile
	lzfs.spec
])
AC_OUTPUT
