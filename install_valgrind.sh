#!/bin/bash

# Requires patched verions of GMP and MPFR to be present at the following paths.
BASE=$(pwd)
GMP_INSTALL_DIR="${BASE}"/gmp/gmp-5.0.1/install
MPFR_INSTALL_DIR="${BASE}"/mpfr/mpfr-3.0.0/install
sed -e 's,<GMP_INSTALL_DIR>,'"${GMP_INSTALL_DIR}"',g' valgrind/fpdebug/Makefile_template.in > valgrind/fpdebug/Makefile_tmp.in
sed -e 's,<MPFR_INSTALL_DIR>,'"${MPFR_INSTALL_DIR}"',g' valgrind/fpdebug/Makefile_tmp.in > valgrind/fpdebug/Makefile.in
rm valgrind/fpdebug/Makefile_tmp.in

cd valgrind
tar -jxvf valgrind-3.12.0.tar.bz2 --strip 1
# Add fpdebug Makefile to ac_config_files in configure
sed -e 's,none/Makefile none/tests/Makefile,fpdebug/Makefile none/Makefile none/tests/Makefile,g' configure > configure_tmp
mv configure_tmp configure
chmod +x configure
# Add fpdebug to EXP_TOOLS
sed -e 's,exp-dhat,exp-dhat fpdebug,g' Makefile.in > Makefile_tmp.in
mv Makefile_tmp.in Makefile.in
./configure --prefix="${BASE}"/valgrind/install
make install

