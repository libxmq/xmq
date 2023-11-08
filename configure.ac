# Copyright (C) 2023 Fredrik Öhrström
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

AC_PREREQ([2.69])
AC_INIT(LIBXMQ, libxmq, oehrstroem@gmail.com,,https://libxmq.org)
AC_CONFIG_AUX_DIR([autoconf])

AC_CANONICAL_BUILD
AC_CANONICAL_HOST

SRC_ROOT="$(pwd -L)"

CONF_NAME="${host_cpu}-${host_vendor}-${host_os}"
BUILD_ROOT="$SRC_ROOT/build"
OUTPUT_ROOT="$SRC_ROOT/build/${CONF_NAME}"

AC_CONFIG_HEADERS(${OUTPUT_ROOT}/config.h:autoconf/config.h.in)

AC_SUBST(PLATFORM)
AC_SUBST(PLATFORM_CFLAGS)
AC_SUBST(PATH_SEP)
AC_SUBST(CONF_MNEMONIC)

if test "${host_vendor}" = "w64"; then
   PLATFORM=WINAPI
   PATH_SEP=';'
   PLATFORM_CFLAGS=""
   FUSE_DEFAULT="no"
   MEDIA_DEFAULT="no"
   ASAN_DEFALUT="no"
   CONF_MNEMONIC="winapi64"
   MNEMONIC="WINAPI64"
   CFLAGS_EXTRAS=""
   CXXFLAGS_EXTRAS=""
   STATIC_XMQ_DEFAULT="yes"
else
   PLATFORM=POSIX
   PATH_SEP=':'
   PLATFORM_CFLAGS="-DUSE_SYSLOG"
   FUSE_DEFAULT="yes"
   MEDIA_DEFAULT="yes"
   ASAN_DEFAULT="yes"
   STATIC_XMQ_DEFAULT="no"
   if test "${host_cpu}" = "arm"; then
       CONF_MNEMONIC="arm32"
       MNEMONIC="ARM32"
       CFLAGS_EXTRAS=""
       CXXFLAGS_EXTRAS=""
   fi
   if test "${host_vendor}" = "apple"; then
       CONF_MNEMONIC="osx64"
       MNEMONIC="OSX64"
       CFLAGS_EXTRAS=-D_DARWIN_FEATURE_64_BIT_INODE
       CXXFLAGS_EXTRAS=-D_DARWIN_FEATURE_64_BIT_INODE
   fi
   if test "${host_vendor}" = "pc"; then
       CONF_MNEMONIC="linux64"
       MNEMONIC="LINUX64"
       CFLAGS_EXTRAS=""
       CXXFLAGS_EXTRAS=""
   fi
fi

PKG_CONFIG="$(which pkg-config)"
if test "$PKG_CONFIG" = ""; then
    PKG_CONFIG=/bin/false
    AC_MSG_ERROR([You need pkg-config to build. Please install.])
fi

mkdir -p "$OUTPUT_ROOT"
if test ! -d "$OUTPUT_ROOT"; then
    AC_MSG_ERROR([Could not create build directory $OUTPUT_ROOT])
fi

AC_SUBST(CONF_NAME)
AC_SUBST(SRC_ROOT)
AC_SUBST(OUTPUT_ROOT)
AC_SUBST(BUILD_ROOT)

AC_HEADER_STAT

if test "x$CC" = "x"; then
    AC_PATH_TOOL([CC], [gcc], [:])
fi
if test "x$CXX" = "x"; then
    AC_PATH_TOOL([CXX], [g++], [:])
fi
if test "x$LD" = "x"; then
    AC_PATH_TOOL([LD], [ld], [:])
fi
if test "x$STRIP" = "x"; then
    AC_PATH_TOOL([STRIP], [strip], [:])
fi

AC_MSG_CHECKING([for address/leak sanitizer and code coverage])

AC_ARG_ENABLE([asan],
  [AS_HELP_STRING([--disable-asan-gcov],
      [disable address sanitizer and code coverage for debug builds @<:@enabled@:>@])],
      [ENABLE_ASAN=${enable_asan}], [ENABLE_ASAN=${ASAN_DEFAULT}])

if test "x$ENABLE_ASAN" = xyes; then
    ASAN_CFLAGS="-O0 -ggdb -fsanitize=address -fno-omit-frame-pointer -fprofile-arcs -ftest-coverage"
    ASAN_LDFLAGS=""
    ASAN_LIBS="-lasan -lgcov --coverage"
    AC_MSG_RESULT([using ${with_asan}])
else
    AC_MSG_RESULT([no])
fi

AC_SUBST(ENABLE_ASAN)
AC_SUBST(ASAN_CFLAGS)
AC_SUBST(ASAN_LDFLAGS)
AC_SUBST(ASAN_LIBS)

AC_MSG_CHECKING([build static xmq binary])

AC_ARG_ENABLE([static-xmq],
  [AS_HELP_STRING([--enable-static-xmq],
      [build a statically linked xmq binary @<:@enabled@:>@])],
      [ENABLE_STATIC_XMQ=${enable_static_xmq}], [ENABLE_STATIC_XMQ=${STATIC_XMQ_DEFAULT}])

if test "x$ENABLE_STATIC_XMQ" = xyes; then
    AC_MSG_RESULT([yes])
else
    AC_MSG_RESULT([no])
fi

AC_SUBST(ENABLE_STATIC_XMQ)

AC_MSG_CHECKING([for zlib])
AC_ARG_WITH(zlib, [AS_HELP_STRING([--with-zlib],
      [specify prefix directory for the zlib package
      (expecting the libraries under PATH and the headers under PATH)])])

if test "x${with_zlib}" = "x"; then
    if ! pkg-config zlib ; then
        AC_MSG_ERROR([not found using pkg-config])
        exit 0
    fi
    ZLIB_CFLAGS="$($PKG_CONFIG zlib --cflags)"
    ZLIB_LIBS="$($PKG_CONFIG zlib --libs)"
    AC_MSG_RESULT([found using pkg-config])
    if test "x$ENABLE_STATIC_XMQ" = xyes; then
        AC_MSG_ERROR([You specified --enable-static-xmq so you must use --with-zlib=... to find a static libz.a])
    fi
else
    with_zlib=$(realpath ${with_zlib})
    ZLIB_CFLAGS="-I${with_zlib}"
    LIBNAME="libz.a"
    LIBF=$(find ${with_zlib} -name "${LIBNAME}")
    if test -z "$LIBF"; then
        AC_MSG_ERROR([Could not find ${LIBNAME} below ${with_zlib}])
    fi
    ZLIB_LIBS="${LIBF}"
    AC_MSG_RESULT([using ${with_zlib}])
fi

AC_SUBST(ZLIB_CFLAGS)
AC_SUBST(ZLIB_LIBS)

AC_MSG_CHECKING([for libxml2])
AC_ARG_WITH(libxml2, [AS_HELP_STRING([--with-libxml2],
      [specify prefix directory for the libxml2 package
      (expecting the libraries under PATH and the headers under PATH)])])

if test "x${with_libxml2}" = "x"; then
    LIBXML2_CFLAGS="-I /usr/include/libxml2"
    LIBXML2_LIBS="-lxml2"
    AC_MSG_RESULT([using -lxml2 and /usr/include/libxml2])
    AC_CHECK_LIB(xml2, xmlSAXParseDoc, [], [
        echo "Error! You need to have libxml2 available."
        exit -1
        ])
    if test "x$ENABLE_STATIC_XMQ" = xyes; then
        AC_MSG_ERROR([You specified --enable-static-xmq  so you must use --with-libxml2=... to find a static libxml2.a])
    fi
else
    with_libxml2=$(realpath ${with_libxml2})
    IDIR=$(find ${with_libxml2} -path "*/libxml/tree.h" | sed 's|/libxml/tree.h||')
    LIBXML2_CFLAGS="-I${IDIR}"
    if test -z "$IDIR"; then
        AC_MSG_ERROR([Could not find libxml/tree.h below ${with_libxml2}])
    fi
    LIBNAME="libxml2.a"
    LIBF=$(find ${with_libxml2} -name "${LIBNAME}")
    if test -z "$LIBF"; then
        AC_MSG_ERROR([Could not find ${LIBNAME} below ${with_libxml2}])
    fi
    LIBXML2_LIBS="${LIBF}"
    AC_MSG_RESULT([using ${with_libxml2}])
fi

AC_SUBST(LIBXML2_CFLAGS)
AC_SUBST(LIBXML2_LIBS)

AC_LANG_PUSH([C])
AC_COMPILE_IFELSE(
[AC_LANG_PROGRAM([], [[
#ifndef __clang__
       not clang
#endif
]])],
[CLANG=yes], [CLANG=no])
AC_LANG_POP([C])

AC_SUBST([CLANG])

GCC_EXTRA_CXXFLAGS=""
if test "x$CLANG" = xyes; then
    AC_MSG_RESULT([checking type of compiler: using clang])
    GCC_EXTRA_CXXFLAGS=""
    GCC_EXTRA_CFLAGS=""
else
    AC_MSG_RESULT([checking type of compiler: using g++])
    GCC_EXTRA_CXXFLAGS="-Wno-maybe-uninitialized"
    GCC_EXTRA_CFLAGS=""
fi

CXXFLAGS="-DPLATFORM_${PLATFORM} ${CXXFLAGS_EXTRAS} -D${MNEMONIC} -Wall -Wmissing-declarations -Wimplicit -Wno-error=deprecated-declarations -fmessage-length=0 -std=c++11 $GCC_EXTRA_CXXFLAGS -Wno-unused-function"

CXXFLAGS_release="-Os"
CXXFLAGS_debug="-O0 -g"
CXXFLAGS_asan="-O0 -g ${ASAN_CFLAGS}"

CFLAGS="-DPLATFORM_${PLATFORM} ${CFLAGS_EXTRAS} -D${MNEMONIC} $GCC_EXTRA_CFLAGS"

CFLAGS_release="-Os"
CFLAGS_debug="-O0 -g"
CFLAGS_asan="-O0 -g ${ASAN_CFLAGS}"

LDFLAGS=""
LDFLAGS_release="-Os"

LDFLAGS_debug="-g ${MEDIA_LIBS}"
LDFLAGSBEGIN_debug="${MEDIA_LIBS}"
LDFLAGSEND_debug=""

LDFLAGS_asan="-g ${ASAN_LDFLAGS} ${MEDIA_LIBS}"
LDFLAGSBEGIN_asan="${ASAN_LIBS} ${MEDIA_LIBS}"
LDFLAGSEND_asan=""

AC_SUBST(CC)
AC_SUBST(CXX)
AC_SUBST(LD)
AC_SUBST(CFLAGS)
AC_SUBST(CFLAGS_release)
AC_SUBST(CFLAGS_debug)
AC_SUBST(CFLAGS_asan)
AC_SUBST(CXXFLAGS)
AC_SUBST(CXXFLAGS_release)
AC_SUBST(CXXFLAGS_debug)
AC_SUBST(CXXCFLAGS_asan)
AC_SUBST(LDFLAGS)
AC_SUBST(LDFLAGS_release)
AC_SUBST(LDFLAGSBEGIN_release)
AC_SUBST(LDFLAGSEND_release)
AC_SUBST(LDFLAGS_debug)
AC_SUBST(LDFLAGSBEGIN_debug)
AC_SUBST(LDFLAGSEND_debug)
AC_SUBST(LDFLAGS_asan)
AC_SUBST(LDFLAGSBEGIN_asan)
AC_SUBST(LDFLAGSEND_asan)

AC_CONFIG_FILES([$OUTPUT_ROOT/spec.mk:$SRC_ROOT/autoconf/spec.mk.in])
AC_CONFIG_FILES([$OUTPUT_ROOT/Makefile:$SRC_ROOT/make/Makefile.in])
# Make sure config.status ends up in the build directory instead of the src root.
CONFIG_STATUS="$OUTPUT_ROOT/config.status"
# Write out spec.gmk and build/Makefile
AC_OUTPUT
