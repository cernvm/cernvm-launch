#!/bin/bash

CONFIG=Release
BUILDDIR=build

if [ ! -z "$1" ]; then
	[ "$1" != "Debug" -a "$1" != "Release" ] && echo "Configuration can either be 'Debug' or 'Release'!" && exit 1  
	CONFIG=$1
	shift
fi

echo "Using following configuration: $CONFIG"

[ ! -d $BUILDDIR ] && mkdir $BUILDDIR
cd $BUILDDIR
#TODO add -Wno-dev flag to the release version
cmake .. -DCMAKE_BUILD_TYPE=${CONFIG} -DTARGET_ARCH="i386" $*

