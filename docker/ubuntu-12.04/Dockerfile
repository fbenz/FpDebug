# FpDebug with Valgrind 3.7 on Ubuntu 12.04
FROM ubuntu:12.04

RUN apt-get update && apt-get install -y \
  git \
  bzip2 \
  build-essential \
  m4

RUN git clone https://github.com/fbenz/FpDebug.git

WORKDIR /FpDebug

RUN ./install_gmp.sh
RUN ./install_mpfr.sh
RUN ./install_valgrind.sh

RUN gcc valgrind/fpdebug/examples/test_1.c -O0 -g -o valgrind/fpdebug/examples/test_1.out

RUN ./valgrind/install/bin/valgrind --tool=fpdebug valgrind/fpdebug/examples/test_1.out
