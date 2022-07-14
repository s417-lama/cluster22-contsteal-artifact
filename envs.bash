#!/bin/bash

if ${MY_ENVS_LOADED:-false}; then
  return
fi

# ITO supercomputer
# ------------------------------------------------------------------------------
if [[ $(hostname) =~ "ito" ]] || [[ $(hostname) =~ "sca" ]]; then
  export MACHINE_NAME=ito

  # module load gcc/9.2.0
  # module load gcc/10.2.0
  export PATH=$HOME/opt/gcc/11.2.0/bin:$PATH
  export LD_LIBRARY_PATH=$HOME/opt/gcc/11.2.0/lib64:$LD_LIBRARY_PATH

  # export PATH=$HOME/opt/clang/13.0.0/bin:$PATH
  # export LD_LIBRARY_PATH=$HOME/opt/clang/13.0.0/lib:$LD_LIBRARY_PATH
  # export OMPI_CC=clang
  # export OMPI_CXX=clang++

  export PATH=$HOME/opt/openmpi5/bin:$PATH
  export LD_LIBRARY_PATH=$HOME/opt/openmpi5/lib:$LD_LIBRARY_PATH

  export CPU_SIMD_FREQ=3.3

elif [[ $(hostname) =~ ^wisteria ]] || [[ $(hostname) =~ ^wo ]]; then
# Wisteria/BDEC-01 Odyssey
# ------------------------------------------------------------------------------
  export MACHINE_NAME=wisteria-o

  export PATH=/work/gc64/c64050/.isola/bin:$PATH

  module load odyssey
  # module load fj/1.2.32

  export CPU_SIMD_FREQ=2.2

# Fugaku
# ------------------------------------------------------------------------------
elif [[ $(hostname) =~ ^fn01sv ]] || [[ $(hostname) =~ ^[a-z][0-9]{2}-[0-9]{4}[a-z]$ ]]; then
  export MACHINE_NAME=fugaku

  export CPU_SIMD_FREQ=2.2

# Others
# ------------------------------------------------------------------------------
else
  export MACHINE_NAME=local

  export CPU_SIMD_FREQ=3.6
fi

export MY_ENVS_LOADED=true
