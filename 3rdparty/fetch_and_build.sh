#!/bin/sh

if [ -z "$1" ]
then
    echo "Usage:"
    echo "fetch_and_build.sh libxml2      (only for the differential tests, make testdiff)"
    echo "fetch_and_build.sh libxslt      (only for the differential tests, make testdiff)"
    exit 0
fi

DIR=$(pwd)

# All cases operate in (and create files in) the directory containing this
# script, so if it was invoked from somewhere else (e.g.
# 'sh 3rdparty/fetch_and_build.sh ...' from the repo root), move there.
case "$(pwd)" in
  *) case "$0" in
       */*) SDIR=$(cd "$(dirname "$0")" 2>/dev/null && pwd)
          if [ -n "$SDIR" ] && [ "$SDIR" != "$DIR" ]
          then
              cd "$SDIR" || exit 1
              DIR=$(pwd)
          fi ;;
     esac ;;
esac

NPROC=2
if command -v nproc > /dev/null 2>&1
then
    NPROC=$(nproc)
fi

if [ "$1" = "libxml2" ]
then
    if [ ! -d libxml2 ]; then
        echo
        echo Fetching libxml2
        echo
        git clone https://github.com/libxmq/libxml2.git
    fi

    # ./.libs/libxml2.a
    cd libxml2
    if [ ! -f ./.libs/libxml2.a ]; then
        echo
        echo Building libxml2
        echo

        autoreconf -fi
        ./autogen.sh --enable-static=yes --with-zlib=no --with-lzma=no --with-python=no --with-http=no
        make -j${NPROC}
    fi
    cd ..
    exit 0
fi

if [ "$1" = "libxslt" ]
then
    if [ ! -d libxslt ]; then
        echo
        echo Fetching libxslt
        echo
        git clone https://github.com/libxmq/libxslt.git
    fi

    cd libxslt
    if [ ! -f ./libxslt/.libs/libxslt.a ]; then
        echo
        echo Building static libxslt
        echo

        # Do this autogen twice to workaround problem with ltmain.sh missing.
        autoreconf -fi
        ./autogen.sh --enable-static=yes --with-libxml-src=${DIR}/libxml2/ --with-python=no || true
        ./autogen.sh --enable-static=yes --with-libxml-src=${DIR}/libxml2/ --with-python=no
        make -j${NPROC}
    fi
    cd ..

    exit 0
fi
