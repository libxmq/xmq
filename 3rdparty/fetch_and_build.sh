#!/bin/sh

if [ ! -d libxml2 ]; then
    echo
    echo Fetching libxml2
    echo
    git clone https://gitlab.gnome.org/GNOME/libxml2.git
fi

cd libxml2

if [ ! -f ./.libs/libxml2.so ]; then
    echo
    echo Building libxml2
    echo

    ./autogen.sh
    make
fi

mkdir libxml2_package
cp libxml2/.libx/* libxml2_package
cp -r libxml2/include/libxml libxml2_package
