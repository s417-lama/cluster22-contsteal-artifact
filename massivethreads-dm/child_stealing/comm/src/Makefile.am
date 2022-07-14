
if MADI_COMM_LAYER_SEQ
defines = -DMADI_COMM_LAYER=MADI_COMM_LAYER_SEQ
sources = \
    seq/comm_base.cc
endif

if MADI_COMM_LAYER_SHMEM
defines = -DMADI_COMM_LAYER=MADI_COMM_LAYER_SHMEM
sources = \
    shmem/comm_memory.cc \
    shmem/comm_base.cc
endif

if MADI_COMM_LAYER_MPI3
defines = -DMADI_COMM_LAYER=MADI_COMM_LAYER_MPI3
sources = \
    mpi3/comm_memory.cc \
    mpi3/comm_base.cc
endif

if MADI_COMM_LAYER_GASNET
defines = -DMADI_COMM_LAYER=MADI_COMM_LAYER_GASNET
sources = \
    gasnet/comm_memory.cc \
    gasnet/comm_base.cc
endif

if MADI_COMM_LAYER_IBV
defines = -DMADI_COMM_LAYER=MADI_COMM_LAYER_IBV
sources = \
    ibv/comm_memory.cc \
    ibv/comm_base.cc
endif

if MADI_COMM_LAYER_FX10
defines = -DMADI_COMM_LAYER=MADI_COMM_LAYER_FX10
sources = \
    fjmpi/comm_memory.cc \
    fjmpi/comm_base.cc
endif

lib_LTLIBRARIES = libmcomm.la
libmcomm_la_SOURCES = \
    options.cc \
    process_config.cc \
    comm_system.cc \
    madm_comm.cc \
    $(sources)

libmcomm_la_CPPFLAGS  = -I$(top_srcdir)/comm/include \
                        -I$(top_srcdir)/comm/include/madm \
                        -I$(top_srcdir)/comm/src \
                        -I$(top_builddir)/comm/include \
                        -Wundef \
			$(defines)
libmcomm_la_CXXFLAGS  =
libmcomm_la_CCASFLAGS =
libmcomm_la_LDFLAGS   =

