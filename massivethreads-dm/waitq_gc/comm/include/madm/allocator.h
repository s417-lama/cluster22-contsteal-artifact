#ifndef MADI_ALLOCATOR_H
#define MADI_ALLOCATOR_H

#include "madm_debug.h"
#include "madm_misc.h"
#include <memory>
#include <vector>
#include <cstdint>

namespace madi {
namespace comm {

    struct alc_header {
        alc_header *next;
        uintptr_t size;
    };

    template <class MemRegion>
    class allocator {
        alc_header *free_list_;
        MemRegion *mr_;
        alc_header *header_;

    public:
        allocator(MemRegion *mr);
        ~allocator();

        template <class T>
        void * allocate(size_t size, T& param);
        void deallocate(void *p);
    };


    template <class MR>
    allocator<MR>::allocator(MR *mr) : free_list_(NULL), mr_(mr)
    {
        header_ = new alc_header;
        header_->next = header_;
        header_->size = 0;

        free_list_ = header_;
    }

    template <class MR>
    allocator<MR>::~allocator()
    {
        delete header_;
    }

#define MADI_ALC_ASSERT(h) \
    do { \
        MADI_ASSERT((uintptr_t)((h)->next) >= 4096); \
        MADI_ASSERT((h)->size <= 32 * 1024 * 1024); \
    } while (false)

    // K&R malloc
    template <class MR>
    template <class T>
    void * allocator<MR>::allocate(size_t size, T& param)
    {
        const size_t init_size = 2 * 1024 * 1024;

        size_t n_units =
            ((size + sizeof(alc_header) - 1)) / sizeof(alc_header) + 1;

        alc_header *prev = free_list_;
        alc_header *h = prev->next;
        for (;;) {
            if (h->size >= n_units)
                break;

            if (prev == free_list_) {
                size_t prev_size = mr_->size();
                size_t total_size = (prev_size == 0) ? init_size : prev_size*2;

                alc_header *new_header =
                    (alc_header *)mr_->extend_to(total_size, param);

                if (new_header == NULL)
                    return NULL;

                size_t ext_size = mr_->size() - prev_size;
                size_t new_n_units = ext_size / sizeof(alc_header);

                MADI_ASSERT(ext_size % sizeof(alc_header) == 0);

                new_header->next = NULL;
                new_header->size = new_n_units;

                deallocate((void *)(new_header + 1));

                // retry this loop
                prev = free_list_;
            }
           
            prev = h;
            h = h->next;
        }

        if (h->size == n_units) {
            prev->next = h->next;
        } else {
            h->size -= n_units;

            h += h->size;
            h->size = n_units;
        }

        free_list_ = prev;
        h->next = NULL;

        return (void *)(h + 1);
    }

    template <class MR>
    void allocator<MR>::deallocate(void *p)
    {
        alc_header *header = (alc_header *)p - 1;

        alc_header *h = free_list_;
        for (;;) {
            if (h < header && header < h->next)
                break;

            if (h >= h->next && (h < header || header < h->next))
                break;

            h = h->next;
        }

        if (header + header->size == h->next) {
            header->size += h->next->size;
            header->next = h->next->next;
        } else {
            header->next = h->next;
        }

        if (h + h->size == header) {
            h->size += header->size;
            h->next = header->next;
        } else {
            h->next = header;
        }

        free_list_ = h;
    }
}
}

#endif
