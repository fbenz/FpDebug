#!/bin/bash
set -e

BASE=$(pwd)
cd gmp
tar -jxvf gmp-5.0.1.tar.bz2
cd gmp-5.0.1
patch -p1 -i ../gmp-5.0.1.patch
./configure --prefix="${BASE}"/gmp/gmp-5.0.1/install
make install
# running 'make check' would fail because of the patch

