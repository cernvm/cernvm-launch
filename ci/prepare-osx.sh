#!/bin/bash

CONFIG=Release
BUILDDIR=build

if [ ! -z "$1" ]; then
	[ "$1" != "Debug" -a "$1" != "Release" ] && echo "Configuration can either be 'Debug' or 'Release'!" && exit 1  
	CONFIG=$1
	shift
fi

echo "Acquiring required submodules [requires git]..."
#Run it in subshell on purpose
$(cd .. && git submodule init && git submodule update)


[ ! -d $BUILDDIR ] && mkdir $BUILDDIR
cd $BUILDDIR
cmake .. -DCMAKE_BUILD_TYPE=${CONFIG} -DCRASH_REPORTING=OFF -DLOGGING=OFF -DTARGET_ARCH="x86_64" \
        -DSYSTEM_CURL=ON \
        -DSYSTEM_JSONCPP=ON \
        -DSYSTEM_ZLIB=ON \
        -DSYSTEM_BOOST=ON -DBOOST_ROOT="/usr/local/opt/boost155/" \
        -DCMAKE_OSX_ARCHITECTURES="x86_64" -G"Xcode" $*
