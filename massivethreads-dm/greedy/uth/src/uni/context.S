#include "uth_config.h"

#if   MADI_ARCH_TYPE == MADI_ARCH_X86_64

#elif MADI_ARCH_TYPE == MADI_ARCH_SPARC64

#include "context_sparc64.S"

#elif MADI_ARCH_TYPE == MADI_ARCH_AARCH64

#else
#error ""
#endif
