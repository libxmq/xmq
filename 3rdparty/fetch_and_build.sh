#!/bin/sh

if [ -z "$1" ]
then
    echo "Usage:"
    echo "fetch_and_build.sh x86_64-pc-linux-gnu"
    echo "fetch_and_build.sh x86_64-w64-mingw32"
    echo "fetch_and_build.sh aarch64-linux-gnu"
    echo "fetch_and_build.sh armv7l-unknown-linux-gnueabihf"
    exit 0
fi

DIR=$(pwd)

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
        git clone https://github.com/GNOME/libxml2.git libxml2-posix
    fi

    # ./.libs/libxml2.a
    cd libxml2-posix
    if [ ! -f ./.libs/libxml2.a ]; then
        echo
        echo Building libxml2 posix
        echo

        ./autogen.sh --enable-static=yes --with-zlib=no --with-lzma=no --with-python=no --with-http=no
        make
    fi
    cd ..

    if [ ! -d libxslt-posix ]; then
        echo
        echo Fetching libxslt posix
        echo
        git clone https://github.com/GNOME/libxslt.git libxslt-posix
    fi

    cd libxslt-posix
    if [ ! -f ./.libs/libxslt.a ]; then
        echo
        echo Building static libxslt posix
        echo

        ./autogen.sh --enable-static=yes --with-libxml-src=${DIR}/libxml2-posix/ --with-python=no
        make
    fi
    cd ..

    exit 0
fi

if [ "$1" = "aarch64-linux-gnu" ]
then
    if [ ! -d zlib-1.3-posix-aarch64 ]; then
        echo
        echo Fetching zlib posix aarch64
        echo
        wget https://github.com/madler/zlib/releases/download/v1.3/zlib-1.3.tar.gz && tar xzf zlib-1.3.tar.gz
        mv zlib-1.3 zlib-1.3-posix-aarch64
    fi

    cd zlib-1.3-posix-aarch64
    if [ ! -f zlib1.dll ]; then
        echo
        echo Building static zlib posix
        echo
        CROSS_PREFIX=aarch64-linux-gnu ./configure
        make
    fi
    cd ..

    if [ ! -d libxml2-posix-aarch64 ]; then
        echo
        echo Fetching libxml2 posix aarch64
        echo
        git clone https://github.com/GNOME/libxml2.git libxml2-posix
        mv libxml2-posix libxml2-posix-aarch64
    fi

    # ./.libs/libxml2.a
    cd libxml2-posix-aarch64
    if [ ! -f ./.libs/libxml2.a ]; then
        echo
        echo Building libxml2 posix aarch64
        echo

        ./autogen.sh --host=aarch64-linux-gnu --enable-static=yes --with-zlib=no --with-lzma=no --with-python=no --with-http=no
        make
    fi
    cd ..

    if [ ! -d libxslt-posix-aarch64 ]; then
        echo
        echo Fetching libxslt posix aarch64
        echo
        git clone https://github.com/GNOME/libxslt.git libxslt-posix
        mv libxslt-posix libxslt-posix-aarch64
    fi

    cd libxslt-posix-aarch64
    if [ ! -f ./.libs/libxslt.a ]; then
        echo
        echo Building static libxslt posix aarch64
        echo

        ./autogen.sh --host=aarch64-linux-gnu --enable-static=yes --with-libxml-src=${DIR}/libxml2-posix-aarch64/ --with-python=no
        make
    fi
    cd ..

    exit 0
fi

if [ "$1" = "armv7l-unknown-linux-gnueabihf" ]
then
    if [ ! -d zlib-1.3-posix-armv7l ]; then
        echo
        echo Fetching zlib posix armv7l
        echo
        wget https://github.com/madler/zlib/releases/download/v1.3/zlib-1.3.tar.gz && tar xzf zlib-1.3.tar.gz
        mv zlib-1.3 zlib-1.3-posix-armv7l
    fi

    cd zlib-1.3-posix-armv7l
    if [ ! -f zlib1.dll ]; then
        echo
        echo Building static zlib posix
        echo
        CROSS_PREFIX=armv7l-linux-gnu ./configure
        make
    fi
    cd ..

    if [ ! -d libxml2-posix-armv7l ]; then
        echo
        echo Fetching libxml2 posix armv7l
        echo
        git clone https://github.com/GNOME/libxml2.git libxml2-posix
        mv libxml2-posix libxml2-posix-armv7l
    fi

    # ./.libs/libxml2.a
    cd libxml2-posix-armv7l
    if [ ! -f ./.libs/libxml2.a ]; then
        echo
        echo Building libxml2 posix armv7l
        echo

        ./autogen.sh --host=armv7l-linux-gnu --enable-static=yes --with-zlib=no --with-lzma=no --with-python=no --with-http=no
        make
    fi
    cd ..

    if [ ! -d libxslt-posix-armv7l ]; then
        echo
        echo Fetching libxslt posix armv7l
        echo
        git clone https://github.com/GNOME/libxslt.git libxslt-posix
        mv libxslt-posix libxslt-posix-armv7l
    fi

    cd libxslt-posix-armv7l
    if [ ! -f ./.libs/libxslt.a ]; then
        echo
        echo Building static libxslt posix armv7l
        echo

        ./autogen.sh --host=armv7l-linux-gnu --enable-static=yes --with-libxml-src=${DIR}/libxml2-posix-armv7l/ --with-python=no
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
        git clone https://github.com/GNOME/libxml2.git libxml2-winapi
    fi

    cd libxml2-winapi
    if [ ! -f ./.libs/libxml2-2.dll ]; then
        echo
        echo Building libxml2 winapi
        echo

        ./autogen.sh --host=x86_64-w64-mingw32 --with-iconv=no --with-zlib=no --with-lzma=no --with-python=no --with-http=no
        if [ "$?" != 0 ]
        then
            # Run autogen again since ltmain.sh was not found?
            echo "Strange error? Lets try again."
            ./autogen.sh --host=x86_64-w64-mingw32 --with-iconv=no --with-zlib=no --with-lzma=no --with-python=no --with-http=no
        fi

        make
    fi
    cd ..

    if [ ! -d libxslt-winapi ]; then
        echo
        echo Fetching libxslt winapi
        echo
        git clone https://github.com/GNOME/libxslt.git libxslt-winapi
    fi

    cd libxslt-winapi
    if [ ! -f ./.libs/libxslt.a ]; then
        echo
        echo Building libxslt winapi
        echo

        ./autogen.sh --host=x86_64-w64-mingw32  --with-libxml-src=${DIR}/libxml2-winapi --with-python=no
        if [ "$?" != 0 ]
        then
            # Run autogen again since ltmain.sh was not found?
            echo "Strange error? Lets try again."
            ./autogen.sh --host=x86_64-w64-mingw32  --with-libxml-src=${DIR}/libxml2-winapi --with-python=no
        fi

        make
    fi
    cd ..

    exit 0
fi
