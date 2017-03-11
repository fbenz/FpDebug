#!/bin/bash
set -e

# This script requires a patched GMP version to be installed at $(pwd)/gmp/gmp-5.0.1/install
# install_gmp.sh installs a pachted GMP version there.
BASE=$(pwd)
cd mpfr
tar -jxvf mpfr-3.0.0.tar.bz2
cd mpfr-3.0.0
cp ../valgrind_additions.c .
patch -p1 -i ../mpfr-3.0.0.patch
./configure --prefix="${BASE}"/mpfr/mpfr-3.0.0/install -with-gmp="${BASE}"/gmp/gmp-5.0.1/install
make install
# running 'make check' would fail because of the patch

