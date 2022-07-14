noinst_PROGRAMS = uts
uts_SOURCES = uts.c rng/brg_sha1.c main.cc
uts_CPPFLAGS = -DBRG_RNG \
               -I$(top_srcdir)/uth/include \
               -I$(top_srcdir)/comm/include \
               -I$(top_builddir)/comm/include/madm
uts_CXXFLAGS = -Wno-conversion
uts_CFLAGS   = -Wno-conversion
uts_LDADD    = $(top_builddir)/uth/src/libuth.la
