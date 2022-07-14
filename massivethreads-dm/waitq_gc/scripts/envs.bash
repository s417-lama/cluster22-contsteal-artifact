#!/bin/bash

if ${MY_ENVS_LOADED:-false}; then
  return
fi

if [[ $(hostname) =~ "obcx" ]]; then
  export MACHINE_NAME=obcx

  export PATH=/work/gc64/c64050/.isola/bin:$PATH
  export PATH=/work/gc64/c64050/.local/bin:$PATH

  module unload intel
  module load gcc/7.5.0

  # Open MPI
  module unload impi
  export PATH=/work/gc64/c64050/opt/openmpi/bin:$PATH

elif [[ $(hostname) =~ "ito" ]] || [[ $(hostname) =~ "sca" ]]; then
  export MACHINE_NAME=ito

  # module load gcc/9.2.0
  # module load gcc/10.2.0
  export PATH=$HOME/opt/gcc/11.2.0/bin:$PATH
  export LD_LIBRARY_PATH=$HOME/opt/gcc/11.2.0/lib64:$LD_LIBRARY_PATH

  # export PATH=$HOME/opt/clang/13.0.0/bin:$PATH
  # export LD_LIBRARY_PATH=$HOME/opt/clang/13.0.0/lib:$LD_LIBRARY_PATH
  # export OMPI_CC=clang
  # export OMPI_CXX=clang++

  case ${MPI_BUILD:-ompi5} in
    ompi5)
      export PATH=$HOME/opt/openmpi5/bin:$PATH
      export LD_LIBRARY_PATH=$HOME/opt/openmpi5/lib:$LD_LIBRARY_PATH
      ;;
    ompi4)
      export PATH=$HOME/opt/openmpi/bin:$PATH
      export LD_LIBRARY_PATH=$HOME/opt/openmpi/lib:$LD_LIBRARY_PATH
      ;;
    impi)
      export I_MPI_CC=gcc
      export I_MPI_CXX=g++
      # module load intel/2020.1
      set +u
      source $HOME/intel/oneapi/setvars.sh
      set -u
      ;;
  esac

elif [[ $(hostname) =~ ^wisteria ]] || [[ $(hostname) =~ ^wo ]]; then
  export MACHINE_NAME=wisteria-o

  export PATH=/work/gc64/c64050/.isola/bin:$PATH

  module load odyssey
  # module load fj/1.2.32

else
  export MACHINE_NAME=local

  export PATH=$HOME/opt/openmpi5/bin:$PATH
fi

export MY_ENVS_LOADED=true
