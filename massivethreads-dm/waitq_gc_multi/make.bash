#!/bin/bash
set -euo pipefail
export LC_ALL=C
export LANG=C

MPI_BUILD=${MPI_BUILD:-ompi5}
# MPI_BUILD=${MPI_BUILD:-ompi4}
# MPI_BUILD=${MPI_BUILD:-impi}

MPI_BUILD=$MPI_BUILD source scripts/envs.bash

ISOLA_HOME=${ISOLA_HOME:-${HOME}}
INSTALL_DIR=${INSTALL_DIR:-${ISOLA_HOME}/opt/massivethreads-dm}

CFLAGS=""
CFLAGS="$CFLAGS -O3 -g"

enabled_kinds=""
# enabled_kinds="WORKER_BUSY WORKER_SCHED WORKER_THREAD_FORK WORKER_THREAD_DIE WORKER_JOIN_RESOLVED WORKER_JOIN_UNRESOLVED WORKER_RESUME_PARENT WORKER_RESUME_STOLEN"
# enabled_kinds="STEAL_SUCCESS STEAL_FAIL STEAL_TASK_COPY JOIN_OUTSTANDING"

disabled_kinds=""
# disabled_kinds="COMM_PUT COMM_GET COMM_FENCE COMM_FETCH_AND_ADD COMM_TRYLOCK COMM_LOCK COMM_UNLOCK COMM_POLL COMM_MALLOC COMM_FREE"

logger_options=() # logger disabled
# logger_options=(--enable-logger --with-logger-enabled-kinds="$enabled_kinds" --with-logger-disabled-kinds="$disabled_kinds") # tracing
# logger_options=(--enable-logger --disable-logger-trace --with-logger-enabled-kinds="$enabled_kinds" --with-logger-disabled-kinds="$disabled_kinds") # stats

if [[ "$MACHINE_NAME" == ito ]]; then
  CCFLAGS=$CFLAGS CXXFLAGS=$CFLAGS ./configure --with-comm-layer=mpi3 ${logger_options[@]+"${logger_options[@]}"} --prefix=${INSTALL_DIR}

  # debug
  # DEBUG_LEVEL=3
  # CFLAGS="$CFLAGS -O0 -g"
  # CCFLAGS=$CFLAGS CXXFLAGS=$CFLAGS ./configure --with-comm-layer=mpi3 --with-madi-debug-level=$DEBUG_LEVEL --prefix=${INSTALL_DIR}
elif [[ "$MACHINE_NAME" == wisteria-o ]]; then
  CFLAGS="-Nclang"
  CCFLAGS=$CFLAGS CXXFLAGS=$CFLAGS ./configure MPICC=mpifccpx MPICXX=mpiFCCpx --host=aarch64 --with-comm-layer=mpi3 ${logger_options[@]+"${logger_options[@]}"} --prefix=${INSTALL_DIR}

  # debug
  # DEBUG_LEVEL=3
  # CFLAGS="$CFLAGS -O0 -g"
  # CCFLAGS=$CFLAGS CXXFLAGS=$CFLAGS ./configure MPICC=mpifccpx MPICXX=mpiFCCpx --host=aarch64 --with-comm-layer=mpi3 --with-madi-debug-level=$DEBUG_LEVEL --prefix=${INSTALL_DIR}
else
  CCFLAGS=$CFLAGS CXXFLAGS=$CFLAGS ./configure --with-comm-layer=mpi3 --enable-polling ${logger_options[@]+"${logger_options[@]}"} --prefix=${INSTALL_DIR}

  # debug
  # DEBUG_LEVEL=3
  # CFLAGS="$CFLAGS -O0 -g"
  # CCFLAGS=$CFLAGS CXXFLAGS=$CFLAGS ./configure --with-comm-layer=mpi3 --enable-polling --with-madi-debug-level=$DEBUG_LEVEL --prefix=${INSTALL_DIR}
fi

make clean
make -j
make install
