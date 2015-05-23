# ax_prog_ccache.m4
#
# SYNOPSIS
#
#   AX_PROG_CCACHE_CC
#   AX_PROG_CCACHE_CXX
#
# DESCRIPTION
#
#   These macro check for the 'ccache' program and prefix C and C++ compilers
#   with "ccache" if this later is present or provided
#
# DESCRIPTION
#
#   Check for ccache compiler cache and enables it if it is found.
#
# LICENSE
#
#   Copyright (c) 2015 German M. Bravo <german.mb@deipi.com>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.

#serial 1

AC_DEFUN([_AX_PROG_CCACHE], [
	AC_ARG_WITH(ccache,
		AS_HELP_STRING([--with-ccache[[=PREFIX]]|no|auto], [use the ccache compiler cache]),,
		with_ccache=auto)

	AS_VAR_PUSHDEF([ax_ccache_compiler], [$1])
	AS_VAR_PUSHDEF([ax_cv_ccache_enabled], [ax_cv_ccache_enabled_$1])

	AC_CACHE_CHECK(whether ccache is enabled by default, ax_cv_ccache_enabled, [
		AC_REQUIRE([AC_PROG_$1])

		AC_LANG_PUSH([$2])

		save_CCACHE_LOGFILE="$CCACHE_LOGFILE"
		CCACHE_LOGFILE="$(pwd)conftest.ccache.log"
		rm -f "$CCACHE_LOGFILE"
		AC_COMPILE_IFELSE([AC_LANG_SOURCE([return 0;])])
		AS_IF([test -f "$CCACHE_LOGFILE"],
			[rm -f $CCACHE_LOGFILE; ax_cv_ccache_enabled=yes],
			[ax_cv_ccache_enabled=no])
		CCACHE_LOGFILE="$save_CCACHE_LOGFILE"

		AC_LANG_POP
	])

	if test "$ax_cv_ccache_enabled" = "yes"; then
		AC_MSG_NOTICE([Using ccache, already enabled.])
	elif test "$with_ccache" != "no"; then
		AC_PATH_PROGS(CCACHE, [ccache],, [$with_ccache/bin:$PATH])
		AC_ARG_VAR([CCACHE],[ccache command to use])
		if test -n "$CCACHE"; then
			AC_MSG_NOTICE([Using ccache, prefixing '$$1' with '$CCACHE'])
			ax_ccache_compiler="$CCACHE $$1"
		elif test "with_ccache" = "yes"; then
			AC_MSG_ERROR([Cannot use ccache, not present.])
		else
			AC_MSG_NOTICE([Not using ccache, not present.])
		fi
	else
		AC_MSG_NOTICE([Not using ccache.])
	fi

	AS_VAR_POPDEF([ax_cv_ccache_enabled])
	AS_VAR_POPDEF([ax_ccache_compiler])
])

AC_DEFUN([AX_PROG_CCACHE_CC], [
	_AX_PROG_CCACHE([CC], [C])
])

AC_DEFUN([AX_PROG_CCACHE_CXX], [
	_AX_PROG_CCACHE([CXX], [C++])
])
