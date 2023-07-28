#!/bin/bash

set -e

if [ $# -ne 3 ]
then
    export MYCC=clang
    export MYCXX=clang++
    export SODIUM=
else
    export MYCC=$1
    export MYCXX=$2
    export SODIUM=$3
fi

echo "CC=$MYCC"
echo "CXX=$MYCXX"

autoreconf -v -i -f
./configure --enable-trackalert --enable-systemd --disable-docker --enable-unit-tests --enable-asan --enable-ubsan $SODIUM --disable-silent-rules CC=$MYCC CXX=$MYCXX
make clean
make
make check || (cat common/test-suite.log && false)
cd regression-tests
./runtests
cd ..
make dist
export WF_VERSION=`grep PACKAGE_VERSION Makefile | awk  '{ print $3}'`
tar xvf wforce-$WF_VERSION.tar.gz
cd wforce-$WF_VERSION
autoreconf -i
cd ..
rm -rf build
mkdir build
cd build
../wforce-$WF_VERSION/configure --enable-trackalert CC=$MYCC CXX=$MYCXX
make
cd ..
make distcheck
