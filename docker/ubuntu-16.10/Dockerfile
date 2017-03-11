# FpDebug with Valgrind 3.12 on Ubuntu 16.10
FROM ubuntu:16.10

RUN apt-get update && apt-get install -y \
  git \
  bzip2 \
  build-essential \
  m4

RUN git clone https://github.com/fbenz/FpDebug.git && \
    cd FpDebug && \
    git checkout valgrind_3.12

WORKDIR /FpDebug

RUN ./install_gmp.sh
RUN ./install_mpfr.sh
RUN ./install_valgrind.sh

RUN gcc valgrind/fpdebug/examples/test_1.c -O0 -g -o valgrind/fpdebug/examples/test_1.out

RUN ./valgrind/install/bin/valgrind --tool=fpdebug valgrind/fpdebug/examples/test_1.out
