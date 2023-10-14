#!/bin/sh

#sudo apt-get install gcc-mingw-w64
#sudo apt-get install gcc-arm-linux-gnueabihf

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
