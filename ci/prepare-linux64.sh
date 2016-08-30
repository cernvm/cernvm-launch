#!/bin/bash

# Tailored for the cvm-build21 machine

CONFIG=Release
BUILDDIR=build

# Let's use a newer compiler
export CMAKE_C_COMPILER='/opt/rh/devtoolset-2/root/usr/bin/gcc'
export CMAKE_CXX_COMPILER='/opt/rh/devtoolset-2/root/usr/bin/g++'
export CC='/opt/rh/devtoolset-2/root/usr/bin/gcc'
export CXX='/opt/rh/devtoolset-2/root/usr/bin/g++'

if [ ! -z "$1" ]; then
	[ "$1" != "Debug" -a "$1" != "Release" ] && echo "Configuration can either be 'Debug' or 'Release'!" && exit 1  
	CONFIG=$1
	shift
fi

echo "Acquiring required submodules [requires git]..."
# Run it in subshell on purpose
$(cd .. && git submodule init && git submodule update)

echo "Using following configuration: $CONFIG"

[ ! -d $BUILDDIR ] && mkdir $BUILDDIR
set -e
cd $BUILDDIR
cmake ../.. -DUSE_SYSTEM_LIBS=ON -DCMAKE_BUILD_TYPE=${CONFIG} -DTARGET_ARCH="x86_64" $*
