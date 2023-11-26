#!/bin/sh

if [ -z "$1" ]
then
    echo "Usage:"
    echo "fetch_and_build.sh x86_64-pc-linux-gnu"
    echo "fetch_and_build.sh x86_64-w64-mingw32"
    exit 0
fi

if [ "$1" = "x86_64-pc-linux-gnu" ]
then
    if [ ! -d zlib-1.3-posix ]; then
        echo
        echo Fetching zlib posix
        echo
        wget https://github.com/madler/zlib/releases/download/v1.3/zlib-1.3.tar.gz && tar xzf zlib-1.3.tar.gz
        mv zlib-1.3 zlib-1.3-posix
    fi

    cd zlib-1.3-posix
    if [ ! -f zlib1.dll ]; then
        echo
        echo Building static zlib posix
        echo
        make
    fi
    cd ..

    if [ ! -d libxml2-posix ]; then
        echo
        echo Fetching libxml2 posix
        echo
        git clone https://gitlab.gnome.org/GNOME/libxml2.git libxml2-posix
    fi

    cd libxml2-posix
    if [ ! -f ./.libs/libxml2.a ]; then
        echo
        echo Building static libxml2 posix
        echo

        ./autogen.sh --with-zlib=no --with-lzma=no --with-python=no --disable-shared
        make
    fi
    cd ..
    exit 0
fi

if [ "$1" = "x86_64-w64-mingw32" ]
then
    if [ ! -d zlib-1.3-winapi ]; then
        echo
        echo Fetching zlib winapi
        echo
        wget https://github.com/madler/zlib/releases/download/v1.3/zlib-1.3.tar.gz && tar xzf zlib-1.3.tar.gz
        mv zlib-1.3 zlib-1.3-winapi
    fi

    cd zlib-1.3-winapi
    if [ ! -f zlib1.dll ]; then
        echo
        echo Building static zlib winapi
        echo
        make -f win32/Makefile.gcc PREFIX=x86_64-w64-mingw32-
    fi
    cd ..

    if [ ! -d libxml2-winapi ]; then
        echo
        echo Fetching libxml2 winapi
        echo
        git clone https://gitlab.gnome.org/GNOME/libxml2.git libxml2-winapi
    fi

    cd libxml2-winapi
    if [ ! -f .libs/libxml2.a ]; then
        echo
        echo Building static libxml2 winapi
        echo

        ./autogen.sh --host=x86_64-w64-mingw32 --with-zlib=no --with-lzma=no --with-python=no
        make
    fi
    cd ..
    exit 0
fi
