
# if MADI_THREAD_TYPE_UNI
thread_type_cppflags = -I$(top_srcdir)/uth/include/uth/uni
thread_type_sources  = uni/taskq.cc uni/worker.cc uni/context.S
# endif
# if MADI_THREAD_TYPE_ISO
# thread_type_cppflags = -I$(top_srcdir)/uth/include/uth/iso
# thread_type_sources  = iso/worker.cc iso/myth_context.s
# endif

#if MADI_COMM_LAYER_SHMEM
#comm_layer_sources = shmem.cc
#endif
#if MADI_COMM_LAYER_FX10
#comm_layer_sources = fjrdma.cc
#endif

lib_LTLIBRARIES = libuth.la
libuth_la_SOURCES = \
	uth_options.cc \
	uth_comm.cc \
	iso_space.cc \
	process.cc \
	madi.cc \
	uth.cc \
	$(thread_type_sources) \
	$(comm_layer_sources)

#collectives.cc

libuth_la_CPPFLAGS = $(thread_type_cppflags) \
		     -I$(top_srcdir)/uth/include \
		     -I$(top_srcdir)/uth/include/uth \
                     -I$(top_srcdir)/comm/include \
		     -I$(top_builddir)/comm/include/madm \
                     -Wundef

libuth_la_LDFLAGS  =
libuth_la_LIBADD   = $(top_builddir)/comm/src/libmcomm.la

