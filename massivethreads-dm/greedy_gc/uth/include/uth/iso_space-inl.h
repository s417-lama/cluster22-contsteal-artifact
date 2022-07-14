#ifndef MADI_ISO_SPACE_INL_H
#define MADI_ISO_SPACE_INL_H

#include <cstdio>
#include "iso_space.h"
#include "madi.h"

namespace madi {

    inline void iso_space::deallocate(void *p, size_t size) {
        MADI_UNDEFINED;
    }

    inline void *iso_space::stack() {
        return stack_;
    }

    inline size_t iso_space::stack_size() {
        return stack_size_;
    }
}

#endif

