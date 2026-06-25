#!/bin/bash

set -e

if [ $# -ne 2 ]
then
    export MYCC=clang
    export MYCXX=clang++
else
    export MYCC=$1
    export MYCXX=$2
fi

echo "CC=$MYCC"
echo "CXX=$MYCXX"

autoreconf -v -i -f
./configure --enable-systemd --disable-sodium --disable-docker --enable-asan --enable-ubsan --disable-silent-rules CC=$MYCC CXX=$MYCXX
make clean
make install
cd regression-tests
./runtests
cd ..
make dist
export RF_VERSION=`grep PACKAGE_VERSION Makefile | awk  '{ print $3}'`
tar xvf replfwd-$RF_VERSION.tar.gz
cd replfwd-$RF_VERSION
autoreconf -i
cd ..
rm -rf build
mkdir build
cd build
../replfwd-$RF_VERSION/configure CC=$MYCC CXX=$MYCXX
make
cd ..
make distcheck
