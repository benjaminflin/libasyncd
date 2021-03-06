################################################################################
## libasyncd
##
## Copyright (c) 2014 Seungyoung Kim.
## All rights reserved.
##
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions are met:
##
## 1. Redistributions of source code must retain the above copyright notice,
##    this list of conditions and the following disclaimer.
## 2. Redistributions in binary form must reproduce the above copyright notice,
##    this list of conditions and the following disclaimer in the documentation
##    and/or other materials provided with the distribution.
##
## THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
## AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
## IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
## ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
## LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
## CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
## SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
## INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
## CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
## ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
## POSSIBILITY OF SUCH DAMAGE.
################################################################################

AC_COPYRIGHT([
==============================================================================
 libasyncd

 Copyright (c) 2014 Seungyoung Kim.
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
==============================================================================
])

## Internal functions
AC_DEFUN([Q_ARG_ENABLE], [
	AC_ARG_ENABLE([$1],[AS_HELP_STRING([--enable-$1], [$2])],,[enableval=no])
	if test "$enableval" = yes; then
		AC_MSG_NOTICE(['$1' feature is enabled])
		CPPFLAGS="$CPPFLAGS $3"
	fi
])

AC_DEFUN([Q_ARG_DISABLE], [
	AC_ARG_ENABLE([$1],[AS_HELP_STRING([--disable-$1], [$2])],,[enableval=yes])
	if test "$enableval" = no; then
		AC_MSG_NOTICE(['$1' feature is disabled])
		CPPFLAGS="$CPPFLAGS $3"
	fi
])

## Initialize
AC_INIT([libasyncd], [1], [https://github.com/wolkykim/libasyncd])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_FILES([Makefile src/Makefile examples/Makefile])
AC_SUBST(DEPLIBS, [""])

## Set path
PATH="$PATH:/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin:/usr/local/opt/openssl/bin"
CPPFLAGS="$CPPFLAGS -I/usr/include -I/usr/local/include -I/usr/local/opt/openssl/include"
CPPFLAGS="$CPPFLAGS -D_GNU_SOURCE"
CPPFLAGS="$CPPFLAGS -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64"
LDFLAGS="$LDFLAGS -L/usr/lib -L/usr/local/lib -L/usr/local/opt/openssl/lib"

## Set autoconf setting
#AC_CANONICAL_TARGET
AC_PREFIX_DEFAULT([/usr/local])
#AC_PRESERVE_HELP_ORDER

## Checks for programs.
AC_PROG_CC
if test $ac_compiler_gnu = yes; then
	CFLAGS="-Wall -Wstrict-prototypes -fPIC"
#else
#	AC_MSG_FAILURE([GCC is required.])
fi
AC_PROG_CC_C99
if test $ac_cv_prog_cc_c99 = no; then
	AC_MSG_FAILURE([Compiler does not support C99 mode.])
fi

AC_PROG_INSTALL
AC_PROG_MKDIR_P
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB

AC_PATH_PROG(AR, ar, [$PATH])
AC_PATH_PROG(CHMOD, chmod, [$PATH])
AC_PATH_PROG(LD, ld, [$PATH])
AC_PATH_PROG(RM, rm, [$PATH])

## Checks for header files.
AC_HEADER_STDC
AC_HEADER_STDBOOL
AC_HEADER_DIRENT
AC_CHECK_HEADER([inttypes.h])
# AC_CHECK_HEADER([openssl/ssl.h])
AC_CHECK_HEADER([event2/event.h])

## Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_INT8_T
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_OFF_T

## Checks for libraries.

## Checks for library functions.
AC_CHECK_LIB([pthread], [main], [], AC_MSG_ERROR([Cannot find pthread library.]))
AC_SUBST(DEPLIBS, ["$DEPLIBS -lpthread"])

AC_CHECK_LIB([event], [main], [], AC_MSG_ERROR([Cannot find libevent library.]))
AC_SUBST(DEPLIBS, ["$DEPLIBS -levent"])

AC_CHECK_LIB([event_pthreads], [main], [], AC_MSG_ERROR([Cannot find libevent_pthreads library.]))
AC_SUBST(DEPLIBS, ["$DEPLIBS -levent_pthreads"])

AC_CHECK_LIB([event_openssl], [main], [], AC_MSG_ERROR([Cannot find libevent_openssl library.]))
AC_SUBST(DEPLIBS, ["$DEPLIBS -levent_openssl"])

AC_CHECK_LIB([ssl], [main], [], AC_MSG_ERROR([Cannot find openssl library.]))
AC_SUBST(DEPLIBS, ["$DEPLIBS -lssl"])

AC_CHECK_LIB([crypto], [main], [], AC_MSG_ERROR([Cannot find crypto library.]))
AC_SUBST(DEPLIBS, ["$DEPLIBS -lcrypto"])

##
## --enable section
##

Q_ARG_ENABLE([debug], [Enable debugging output. This will print out internal debugging messages to stdout.], [-DBUILD_DEBUG])
if test "$enableval" = yes; then
	CFLAGS="$CFLAGS -g -O2"
else
	CFLAGS="$CFLAGS -g -O2"
fi

AC_MSG_NOTICE([CFLAGS $CFLAGS])
AC_MSG_NOTICE([CPPFLAGS $CPPFLAGS])
AC_MSG_NOTICE([DEPLIBS $DEPLIBS])

## Create Makefile
AC_OUTPUT
