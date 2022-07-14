#ifndef MADI_COMM_MEMORY_IBV_H
#define MADI_COMM_MEMORY_IBV_H

#include "../process_config.h"
#include <cstddef>
#include <cstdint>

namespace madi {
namespace comm {

    // collective (registered) memory region
    class comm_memory : noncopyable {
        uint8_t *region_begin_;
        uint8_t *region_end_;
        uint8_t *ptr_;

    public:
        explicit comm_memory(void *ptr, size_t size,
                             process_config& config);
        ~comm_memory() = default;

        size_t size() const;

        void * extend_to(size_t size, process_config& config);
    };

}
}

#endif
