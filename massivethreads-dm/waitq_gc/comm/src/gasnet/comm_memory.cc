#include "gasnet/comm_memory.h"

#include "sys.h"
#include "madm_misc.h"
#include "madm_debug.h"

#include <climits>
#include <cerrno>

namespace madi {
namespace comm {

    void touch_pages(void *p, size_t size)
    {
        uint8_t *array = reinterpret_cast<uint8_t *>(p);
        size_t page_size = options.page_size;

        for (size_t i = 0; i < size; i += page_size)
            array[i] = 0;
    }

    comm_memory::comm_memory(void *ptr, size_t size, process_config& config)
        : region_begin_ (reinterpret_cast<uint8_t *>(ptr))
        , region_end_   (reinterpret_cast<uint8_t *>(ptr) + size)
        , ptr_          (reinterpret_cast<uint8_t *>(ptr))
    {
    }

    size_t comm_memory::size() const
    {
        return ptr_ - region_begin_;
    }

    void * comm_memory::extend_to(size_t size, process_config& config)
    {
        if (ptr_ + size >= region_end_)
            return NULL;

        void *p = reinterpret_cast<void *>(ptr_);

        ptr_ += size;

        touch_pages(p, size);

        return p;
    }

}
}
