dnl  This file is part of par2cmdline (a PAR 2.0 compatible file verification and
dnl  repair tool). See http://parchive.sourceforge.net for details of PAR 2.0.
dnl
dnl  Copyright (c) 2003 Peter Brian Clements
dnl
dnl  Modifications for specific OS support (Mac, Linux, FreeBSD), and x86/x86_64
dnl  assembly code building (c) 2008 Vincent Tan.
dnl
dnl  par2cmdline is free software; you can redistribute it and/or modify
dnl  it under the terms of the GNU General Public License as published by
dnl  the Free Software Foundation; either version 2 of the License, or
dnl  (at your option) any later version.
dnl
dnl  par2cmdline is distributed in the hope that it will be useful,
dnl  but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl  GNU General Public License for more details.
dnl
dnl  You should have received a copy of the GNU General Public License
dnl  along with this program; if not, write to the Free Software
dnl  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
dnl                                               -*- Autoconf -*-
dnl Process this file with autoconf to produce a configure script.
dnl
dnl added AM_PROG_AS, added <case "$host_cpu" in>...<esac>, 

AC_PREREQ(2.58a)
AC_INIT([par2cmdline], [0.4], [peterbclements@users.sourceforge.net])
AC_CONFIG_SRCDIR([par2cmdline.cpp])

AC_CANONICAL_HOST

AM_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE


dnl Checks for programs.
AC_PROG_CXX
AC_PROG_INSTALL
AM_PROG_AS

dnl AC_MSG_RESULT("$host_cpu" "$host_os")
dnl
dnl because GCC 4.0 produces i386 code that is sub-optimal,
dnl an assembly file is used to bring performance on GCC
dnl compiled versions up to the same level as Visual C++ 8
dnl

AC_DEFINE([NDEBUG], [], [disable assert statements])

case "$host_cpu" in
	x86_64|amd64)
		AC_MSG_RESULT(SSE2_on_x86_64 is enabled)
		AM_CONDITIONAL(AMD64, true)
		AM_CONDITIONAL(X86MMX, false)
		AM_CONDITIONAL(X86CPU, false)
		AM_CONDITIONAL(PPC64, false)
		AM_CONDITIONAL(PPC32, false)
		;;
	i686)
		AC_MSG_RESULT(SSE2_on_i686 is enabled)
		AM_CONDITIONAL(AMD64, false)
		AM_CONDITIONAL(X86MMX, true)
		AM_CONDITIONAL(X86CPU, true)
		AM_CONDITIONAL(PPC64, false)
		AM_CONDITIONAL(PPC32, false)
		;;
	i586)
		AC_MSG_RESULT(MMX is enabled)
		AM_CONDITIONAL(AMD64, false)
		AM_CONDITIONAL(X86MMX, true)
		AM_CONDITIONAL(X86CPU, true)
		AM_CONDITIONAL(PPC64, false)
		AM_CONDITIONAL(PPC32, false)
		;;
	i386|i486)
		AC_MSG_RESULT(x86 ASM is enabled)
		AM_CONDITIONAL(AMD64, false)
		AM_CONDITIONAL(X86CPU, true)
		AM_CONDITIONAL(X86MMX, false)
		AM_CONDITIONAL(PPC64, false)
		AM_CONDITIONAL(PPC32, false)
		;;
	powerpc64)
		AC_MSG_RESULT(PPC64 is enabled)
		AM_CONDITIONAL(AMD64, false)
		AM_CONDITIONAL(X86CPU, false)
		AM_CONDITIONAL(X86MMX, false)
		AM_CONDITIONAL(PPC64, true)
		AM_CONDITIONAL(PPC32, false)
		AC_DEFINE([WORDS_BIGENDIAN])
		;;
	powerpc)
		AC_MSG_RESULT(PPC32 is enabled)
		AM_CONDITIONAL(AMD64, false)
		AM_CONDITIONAL(X86CPU, false)
		AM_CONDITIONAL(X86MMX, false)
		AM_CONDITIONAL(PPC64, false)
		AM_CONDITIONAL(PPC32, true)
		AC_DEFINE([WORDS_BIGENDIAN])
		;;
	*)
		AM_CONDITIONAL(AMD64, false)
		AM_CONDITIONAL(X86CPU, false)
		AM_CONDITIONAL(X86MMX, false)
		AM_CONDITIONAL(PPC64, false)
		AM_CONDITIONAL(PPC32, false)
		;;
esac

case "$host" in
	*-*-linux-gnu)
		sys_linux=true
		AM_CONDITIONAL(PLATFORM_LINUX, true)
		AM_CONDITIONAL(PLATFORM_DARWIN, false)
		AM_CONDITIONAL(PLATFORM_FREEBSD, false)
		AC_DEFINE(PLATFORM_LINUX, 1, [defined if building for Linux])
		;;
	*-*-darwin*)
		sys_darwin=true
		AM_CONDITIONAL(PLATFORM_LINUX, false)
		AM_CONDITIONAL(PLATFORM_DARWIN, true)
		AM_CONDITIONAL(PLATFORM_FREEBSD, false)
		AC_DEFINE(PLATFORM_DARWIN, 1, [defined if building for Darwin / MacOS X])
		;;
	*-*-freebsd*)
		sys_freebsd=true
		AM_CONDITIONAL(PLATFORM_LINUX, false)
		AM_CONDITIONAL(PLATFORM_DARWIN, false)
		AM_CONDITIONAL(PLATFORM_FREEBSD, true)
		AC_DEFINE(PLATFORM_FREEBSD, 1, [defined if building for FreeBSD])
		;;
esac

case "$gpgpu" in
	cuda)
		AM_CONDITIONAL(GPGPU_CUDA, true)
		AC_DEFINE(GPGPU_CUDA, 1, [defined if nVidia CUDA is supported])
		;;
	*)
		AM_CONDITIONAL(GPGPU_CUDA, false)
		AH_TEMPLATE(GPGPU_CUDA, [defined if nVidia CUDA is supported])
		;;
esac

AM_CCASFLAGS="-I."

dnl Checks for libraries.

dnl Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDBOOL
AC_HEADER_STDC
AC_CHECK_HEADERS([stdio.h] [endian.h] [aio.h] [errno.h])
AC_CHECK_HEADERS([getopt.h])

dnl Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_C_BIGENDIAN
AC_C_CONST
AC_C_INLINE
AC_SYS_LARGEFILE
AC_FUNC_FSEEKO

dnl Checks for library functions.
AC_FUNC_MEMCMP
AC_CHECK_FUNCS([stricmp] [strcasecmp])
AC_CHECK_FUNCS([strchr] [memcpy])

AC_CHECK_FUNCS([getopt] [getopt_long])

AC_CHECK_FUNCS([realpath])

AC_CONFIG_FILES([stamp-h], [echo timestamp > stamp-h])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT

