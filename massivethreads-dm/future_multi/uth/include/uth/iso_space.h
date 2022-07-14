#ifndef MADI_ISO_SPACE_H
#define MADI_ISO_SPACE_H

#include "madi.h"
#include "misc.h"
#include "debug.h"
#include <cstddef>
#include <cstdint>

namespace madi {

    class uth_comm;
    
    class iso_space {
        MADI_NONCOPYABLE(iso_space);

        enum iso_space_bits {
            MAX_BITS             = 38,

            PROCESS_BITS         = 18,
            SHARED_LOCAL_BITS    = 20,
            SHARED_BITS          = PROCESS_BITS + SHARED_LOCAL_BITS,

            SHARED_LOCAL_SIZE    = 1ul << SHARED_LOCAL_BITS,
            SHARED_SIZE          = 1ul << SHARED_BITS,

            SHARED_BASE          = 0x700000000000,
        };

        static uint8_t * const DEFAULT_ISO_SPACE_BASE;

        uint8_t *shared_base_;
        size_t shared_size_;
        uint8_t *stack_;
        size_t stack_size_;

        uint8_t **remote_ptrs_;

    public:
        iso_space();
        ~iso_space() = default;

        void initialize(uth_comm& comm);
        void finalize(uth_comm& comm);
        
        void *allocate(size_t size);
        void deallocate(void *p, size_t size);

        void *stack();
        size_t stack_size();

        uint8_t *shared_base_ptr() { return shared_base_; }
        uint8_t *shared_end_ptr() { return shared_base_ + shared_size_; }
        size_t shared_size() { return shared_size_; }

//         uint8_t * shared_ptr(uth_pid_t pid)
//         {
//             return SHARED_BASE + SHARED_LOCAL_SIZE * pid;
//         }

        uint8_t *remote_ptr(uint8_t *frame_base, uth_pid_t pid)
        {
//             size_t idx = index_of_shared_ptr(frame_base);
//             return remote_ptrs_[pid] + idx;
            return frame_base;
        }

    private:
        void validate_iso_space_options();
    };

}

#endif
