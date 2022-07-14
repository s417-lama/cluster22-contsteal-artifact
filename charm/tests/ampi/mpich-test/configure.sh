#!/bin/bash
CC=${PWD}/../../../bin/ampicc CFLAGS="-g -O0" MPICC=${PWD}/../../../bin/ampicc CXX=${PWD}/../../../bin/ampicxx CXXFLAGS="-g -O0" MPICXX=${PWD}/../../../bin/ampicxx MPI_SIZEOF_AINT=8 MPI_SIZEOF_OFFSET=8 ./configure --enable-strictmpi --disable-threads --disable-spawn --disable-cxx --disable-checkfaults --disable-checkpointing --disable-ft-tests --disable-romio --disable-fortran --disable-long-double --disable-long-double-complex --disable-maintainer_mode

# To configure with Fortran90 support:
# CC=${PWD}/../../../bin/ampicc MPICC=${PWD}/../../../bin/ampicc CXX=${PWD}/../../../bin/ampicxx MPICXX=${PWD}/../../../bin/ampicxx MPIFC=${PWD}/../../../bin/ampif90 MPIF77=${PWD}/../../../bin/ampif77 FCFLAGS="-g -O0" FC=${PWD}/../../../bin/ampif90 FCLD=${PWD}/../../../bin/ampif90 MPI_SIZEOF_AINT=8 MPI_SIZEOF_OFFSET=8 ./configure --enable-strictmpi --disable-threads --disable-spawn --disable-cxx --disable-checkfaults --disable-checkpointing --disable-ft-tests --disable-romio --disable-long-double --disable-long-double-complex --disable-maintainer_mode

