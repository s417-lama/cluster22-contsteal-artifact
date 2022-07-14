#!/bin/bash
set -euo pipefail
export LC_ALL=C
export LANG=C

source ./envs.bash

ISOLA_HOME=${ISOLA_HOME:-${HOME}}
INSTALL_DIR=${INSTALL_DIR:-${ISOLA_HOME}/opt/massivethreads-dm}

LOGGER_TYPE=${LOGGER_TYPE:-none}
# LOGGER_TYPE=${LOGGER_TYPE:-stats}
# LOGGER_TYPE=${LOGGER_TYPE:-trace}

CFLAGS=""
CFLAGS="$CFLAGS -O3 -g"

if [[ $LOGGER_TYPE == stats ]]; then
  enabled_kinds="STEAL_SUCCESS STEAL_FAIL STEAL_TASK_COPY JOIN_OUTSTANDING"
  disabled_kinds=""
  logger_options=(--enable-logger --disable-logger-trace --with-logger-enabled-kinds="$enabled_kinds" --with-logger-disabled-kinds="$disabled_kinds")
elif [[ $LOGGER_TYPE == trace ]]; then
  enabled_kinds="JOIN_OUTSTANDING"
  disabled_kinds=""
  logger_options=(--enable-logger --with-logger-enabled-kinds="$enabled_kinds" --with-logger-disabled-kinds="$disabled_kinds")
else
  enabled_kinds=""
  disabled_kinds=""
  logger_options=() # logger disabled
fi

if [[ "$MACHINE_NAME" == ito ]]; then
  CCFLAGS=$CFLAGS CXXFLAGS=$CFLAGS ./configure --with-comm-layer=mpi3 ${logger_options[@]+"${logger_options[@]}"} --prefix=${INSTALL_DIR}

elif [[ "$MACHINE_NAME" == wisteria-o ]]; then
  CFLAGS="-Nclang $CFLAGS"
  CCFLAGS=$CFLAGS CXXFLAGS=$CFLAGS ./configure MPICC=mpifccpx MPICXX=mpiFCCpx --host=aarch64 --with-comm-layer=mpi3 ${logger_options[@]+"${logger_options[@]}"} --prefix=${INSTALL_DIR}

elif [[ "$MACHINE_NAME" == fugaku ]]; then
  CFLAGS="-Nclang $CFLAGS"
  CCFLAGS=$CFLAGS CXXFLAGS=$CFLAGS ./configure MPICC=mpifccpx MPICXX=mpiFCCpx --host=aarch64 --with-comm-layer=mpi3 ${logger_options[@]+"${logger_options[@]}"} --prefix=${INSTALL_DIR}

else
  # CCFLAGS=$CFLAGS CXXFLAGS=$CFLAGS ./configure --with-comm-layer=mpi3 --enable-polling ${logger_options[@]+"${logger_options[@]}"} --prefix=${INSTALL_DIR}
  CCFLAGS=$CFLAGS CXXFLAGS=$CFLAGS ./configure --with-comm-layer=mpi3 ${logger_options[@]+"${logger_options[@]}"} --prefix=${INSTALL_DIR}

fi

make clean
make -j
make install
