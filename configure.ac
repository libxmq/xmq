# Copyright (C) 2023-2024 Fredrik Öhrström
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

# host_cpu   host_vendor   host_os
# armv7l     unknown       linux-gnueabihf
# x86_64     pc            linux-gnu
# x86_64     w64           mingw32
# wasm32     unknown       emscripten

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
   if test "${host_cpu}" = "aarch64"; then
       CONF_MNEMONIC="aarch64"
       MNEMONIC="LINUX64"
       CFLAGS_EXTRAS=""
       CXXFLAGS_EXTRAS=""
   elif test "${host_cpu}" = "armv7l"; then
       CONF_MNEMONIC="arm32"
       MNEMONIC="LINUX32"
       CFLAGS_EXTRAS=""
       CXXFLAGS_EXTRAS=""
   elif test "${host_vendor}" = "apple"; then
       CONF_MNEMONIC="osx64"
       MNEMONIC="OSX64"
       CFLAGS_EXTRAS=-D_DARWIN_FEATURE_64_BIT_INODE
       CXXFLAGS_EXTRAS=-D_DARWIN_FEATURE_64_BIT_INODE
   elif test "${host_vendor}" = "pc"; then
       CONF_MNEMONIC="linux64"
       MNEMONIC="LINUX64"
       CFLAGS_EXTRAS=""
       CXXFLAGS_EXTRAS=""
   elif test "${host_cpu}" = "wasm32"; then
       CONF_MNEMONIC="wasm32"
       MNEMONIC="LINUX32"
       CFLAGS_EXTRAS="-Wno-unknown-warning-option"
       CXXFLAGS_EXTRAS=""
   else
       CONF_MNEMONIC="posix64"
       MNEMONIC="POSIX64"
       CFLAGS_EXTRAS=""
       CXXFLAGS_EXTRAS=""
   fi
fi

PKG_CONFIG="$(which pkg-config)"
if test "$PKG_CONFIG" = ""; then
    PKG_CONFIG=/bin/false
    AC_MSG_ERROR([You need pkg-config to build. Try: sudo apt install pkg-config])
fi

mkdir -p "$OUTPUT_ROOT"
if test ! -d "$OUTPUT_ROOT"; then
    AC_MSG_ERROR([Could not create build directory $OUTPUT_ROOT])
fi

rm -f "$SRC_ROOT/build/default"
# Create the link build/default which points to the output root.
# This reduces the need to know the exact output root which might include OS versions etc.
# Useful for CI builds.
ln -s "$OUTPUT_ROOT" "$SRC_ROOT/build/default"

AC_SUBST(CONF_NAME)
AC_SUBST(SRC_ROOT)
AC_SUBST(OUTPUT_ROOT)
AC_SUBST(BUILD_ROOT)
AC_SUBST(PREFIX)
if test "x$prefix" = "xNONE"; then
    PREFIX="$ac_default_prefix"
else
    PREFIX="$prefix"
fi

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
    if test "$CC" = "clang"; then
        ASAN_CFLAGS="-O0 -ggdb -fsanitize=address,undefined -fno-omit-frame-pointer"
        ASAN_LDFLAGS="-fsanitize=address,undefined -static-libsan"
        ASAN_LIBS=""
    else
        ASAN_CFLAGS="-O0 -ggdb -fsanitize=address,undefined -fno-omit-frame-pointer"
        ASAN_LDFLAGS="-fsanitize=address,undefined -static-libasan"
        ASAN_LIBS="-lgcov --coverage"
    fi
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
        AC_MSG_ERROR([not found using pkg-config. Try: sudo apt install zlib1g-dev])
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
    if ! pkg-config libxml-2.0 ; then
        AC_MSG_ERROR([not found using pkg-config. Try: sudo apt install libxml2-dev])
        exit 0
    fi
    LIBXML2_CFLAGS="$($PKG_CONFIG libxml-2.0 --cflags)"
    LIBXML2_LIBS="$($PKG_CONFIG libxml-2.0 --libs)"
    AC_MSG_RESULT([found using pkg-config])
    if test "x$ENABLE_STATIC_XMQ" = xyes; then
        AC_MSG_ERROR([You specified --enable-static-xmq so you must use --with-libxml2=... to find a static libxml2.a])
    fi
else
    with_libxml2=$(realpath ${with_libxml2})
    IDIR=$(find ${with_libxml2} -path "*/libxml/tree.h" | sed 's|/libxml/tree.h||')
    LIBXML2_CFLAGS="-I${IDIR}"
    if test -z "$IDIR"; then
        AC_MSG_ERROR([Could not find libxml/tree.h below ${with_libxml2}])
    fi
    LIBNAME="libxml2-*.dll"
    LIBF=$(find ${with_libxml2} -name "${LIBNAME}")
    LIBXML2_LIBS="-Wl,-Bstatic ${LIBF}"

    if test -z "$LIBF"; then
       LIBNAME="libxml2_la-tree.o"
       LIBF=$(find ${with_libxml2} -name "${LIBNAME}" | head -n 1)
       LIBXML2_LIBS=$(find ${with_libxml2} -name "libxml2*.o" | grep "\\.libs/" | tr '\n' ' ')
    fi
    if test -z "$LIBF"; then
        AC_MSG_ERROR([Could not find ${LIBNAME} below ${with_libxml2}])
    fi
    LIBXML2_LIBNAME=$(basename ${LIBF})
    AC_MSG_RESULT([using ${with_libxml2} ${LIBXML2_LIBNAME}])
fi

AC_SUBST(LIBXML2_CFLAGS)
AC_SUBST(LIBXML2_LIBS)
AC_SUBST(LIBXML2_LIBNAME)

AC_MSG_CHECKING([for libxslt])
AC_ARG_WITH(libxslt, [AS_HELP_STRING([--with-libxslt],
      [specify prefix directory for the libxslt package
      (expecting the libraries under PATH and the headers under PATH)])])

if test "x${with_libxslt}" = "x"; then
    if ! pkg-config libxslt ; then
        AC_MSG_ERROR([not found using pkg-config. Try: sudo apt install libxslt1-dev])
        exit 0
    fi
    LIBXSLT_CFLAGS="$($PKG_CONFIG libxslt --cflags)"
    LIBXSLT_LIBS="$($PKG_CONFIG libxslt --libs)"
    AC_MSG_RESULT([found using pkg-config])
    if test "x$ENABLE_STATIC_XMQ" = xyes; then
        AC_MSG_ERROR([You specified --enable-static-xmq so you must use --with-libxslt=... to find a static libxslt.a])
    fi
else
    with_libxslt=$(realpath ${with_libxslt})
    IDIR=$(find ${with_libxslt} -path "*/libxslt/xslt.h" | sed 's|/libxslt/xslt.h||')
    LIBXSLT_CFLAGS="-I${IDIR}"
    if test -z "$IDIR"; then
        AC_MSG_ERROR([Could not find libxslt/xslt.h below ${with_libxslt}])
    fi
    LIBNAME="libxslt-1.dll"
    LIBF=$(find ${with_libxslt} -name "${LIBNAME}")
    LIBXSLT_LIBS="-Wl,-Bstatic ${LIBF}"

    if test -z "$LIBF"; then
       LIBNAME="xsltutils.o"
       LIBF=$(find ${with_libxslt}/libxslt -name "${LIBNAME}" | head -n 1)
       LIBXSLT_LIBS=$(find ${with_libxslt}/libxslt -name "*.o" | grep "\\.libs/" | tr '\n' ' ')
    fi
    if test -z "$LIBF"; then
        AC_MSG_ERROR([Could not find ${LIBNAME} below ${with_libxslt}])
    fi

    AC_MSG_RESULT([using ${with_libxslt}])
fi

AC_SUBST(LIBXSLT_CFLAGS)
AC_SUBST(LIBXSLT_LIBS)

BCRYPT_DLL=""

if test "${host_vendor}" = "w64"; then
    AC_MSG_CHECKING([for bcrypt.dll])
    AC_ARG_WITH(bcrypt, [AS_HELP_STRING([--with-bcrypt], [specify location of bcrypt.dll])])

    if test "x${with_bcrypt}" = "x"; then
        # There is a bcrypt.dll in wine /usr/lib/x86_64-linux-gnu/wine/bcrypt.dll
        # Search for it and we link against it.
        # We do not package it, since it is available on Windows.
        BCRYPT_DLL=$(find /usr/lib -name bcrypt.dll | grep $(uname -i))
        if ! test -f "$BCRYPT_DLL"; then
            AC_MSG_ERROR([Could not find bcrypt.dll below /usr/lib])
        fi
        AC_MSG_RESULT([found ${BCRYPT_DLL}])
    else
        BCRYPT_DLL=$(realpath ${with_bcrypt})
        if ! test -f "$BCRYPT_DLL"; then
            AC_MSG_ERROR([The specified bcrypt.dll does not exist. ${with_bcrypt}])
        fi
        AC_MSG_RESULT([using ${with_bcrypt}])
    fi
fi

AC_SUBST(BCRYPT_DLL)

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

CXXFLAGS="-DPLATFORM_${PLATFORM} ${CXXFLAGS_EXTRAS} -D${MNEMONIC} -Wall -Wunused-variable -Wextra -Wno-clobbered -Wno-unused-parameter -Wno-attributes -Wparentheses -Wmissing-declarations -Werror=implicit-function-declaration -Wredundant-decls -Wimplicit -Wno-unused-variable -Wno-error=deprecated-declarations -fmessage-length=0 -std=c++11 $GCC_EXTRA_CXXFLAGS -Wno-unused-function -Wreturn-type"

CXXFLAGS_release="-Os"
CXXFLAGS_debug="-O0 -g"
CXXFLAGS_asan="-O0 -g ${ASAN_CFLAGS}"

CFLAGS="-DPLATFORM_${PLATFORM} ${CFLAGS_EXTRAS} -D${MNEMONIC} $GCC_EXTRA_CFLAGS -Wall -Wunused-variable -Wextra -Wno-clobbered -Wno-unused-parameter -Wno-attributes -Wparentheses -Wmissing-declarations -Werror=implicit-function-declaration -Wredundant-decls -Wimplicit -Wno-unused-variable -Wno-error=deprecated-declarations -fmessage-length=0 -Wno-unused-function -Wreturn-type"

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
AC_CONFIG_FILES([$OUTPUT_ROOT/spec.inc:$SRC_ROOT/autoconf/spec.inc.in])
AC_CONFIG_FILES([$OUTPUT_ROOT/Makefile:$SRC_ROOT/make/Makefile.in])
# Make sure config.status ends up in the build directory instead of the src root.
CONFIG_STATUS="$OUTPUT_ROOT/config.status"
# Write out spec.gmk and build/Makefile
AC_OUTPUT
