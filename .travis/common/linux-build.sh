#!/usr/bin/env bash

set -e

SRC=$1
BUILD=$2
CPUS=$(nproc)

mkdir -p $BUILD
cd $BUILD

cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/usr $SRC

echo Building on $CPUS CPUs
make -j$((CPUS))

fakeroot make package

LIBDIR=$(grep VEYON_LIB_DIR CMakeCache.txt |cut -d "=" -f2)
BUILD_PWD=$(pwd)

mkdir -p $LIBDIR
cd $LIBDIR
find $BUILD_PWD/plugins -name "*.so" -exec ln -s '{}' ';'
cd $BUILD_PWD

./cli/veyon-cli help
./cli/veyon-cli about

