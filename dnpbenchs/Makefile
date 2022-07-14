HOME_DIR := $(or ${ISOLA_HOME},${HOME})

UTH_PATH     := $(HOME_DIR)/opt/massivethreads-dm
UTH_CXXFLAGS := -I$(UTH_PATH)/include -fno-stack-protector
UTH_LDFLAGS  := -L$(UTH_PATH)/lib -luth -lmcomm

UCX_INCLUDE_PATH := ${HOME}/opt/ucx/include

LIBUNWIND_CXXFLAGS := -I${HOME}/opt/libunwind/include
LIBUNWIND_LDFLAGS  := -L${HOME}/opt/libunwind/lib -lunwind

ifeq ($(MACHINE_NAME),wisteria-o)
	CXX := mpiFCC
	CXXFLAGS := $(UTH_CXXFLAGS) -I. -std=c++14 -O3 -g -Nclang -Wall $(CXXFLAGS)
else ifeq ($(MACHINE_NAME),fugaku)
	CXX := mpiFCC
	CXXFLAGS := $(UTH_CXXFLAGS) -I. -std=c++14 -O3 -g -Nclang -Wall $(CXXFLAGS)
else
	CXX := mpicxx
	CXXFLAGS := $(UTH_CXXFLAGS) -I. -std=c++14 -O3 -g -march=native -Wall $(CXXFLAGS)
endif
LDFLAGS  := $(UTH_LDFLAGS) -lpthread -lm -ldl

LIBS := $(wildcard ./lib*.cpp)
SRCS := $(filter-out $(LIBS),$(wildcard ./*.cpp))

LIB_TARGETS  := $(patsubst %.cpp,%.so,$(LIBS))
MAIN_TARGETS := $(patsubst %.cpp,%.out,$(SRCS)) uts.out

all: $(MAIN_TARGETS) $(LIB_TARGETS)

%.out: %.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.so: %.cpp
	$(CXX) $(CXXFLAGS) -I$(UCX_INCLUDE_PATH) $(LIBUNWIND_CXXFLAGS) -o $@ $^ -shared -fPIC -ldl $(LIBUNWIND_LDFLAGS)

uts.out: uts/uts.c uts/rng/brg_sha1.c uts/main.cc
	$(CXX) $(CXXFLAGS) -DBRG_RNG=1 -o $@ $^ $(LDFLAGS)

clean:
	rm -rf $(MAIN_TARGETS) $(LIB_TARGETS)
