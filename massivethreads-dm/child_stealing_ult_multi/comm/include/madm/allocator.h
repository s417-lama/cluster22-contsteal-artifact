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
        alc_header *header0_;

    public:
        template <class T>
        allocator(MemRegion *mr, T& param);
        ~allocator();

        template <bool extend, class T>
        void * allocate(size_t size, T& param);
        void deallocate(void *p);
    };


    template <class MR>
    template <class T>
    allocator<MR>::allocator(MR *mr, T& param) : free_list_(NULL), mr_(mr)
    {
        const size_t init_size = get_env("MADM_COMM_ALLOCATOR_INIT_SIZE", 2 * 1024 * 1024);

        MADI_ASSERT(init_size % sizeof(alc_header) == 0);

        alc_header *header = (alc_header *)mr_->extend_to(init_size, param);

        if (header == NULL) {
            madi::die("initial allocation failed");
        }

        header->next = NULL;
        header->size = init_size / sizeof(alc_header);

        header0_ = new alc_header;
        header0_->next = header;
        header0_->size = 0;

        free_list_ = header0_;
    }

    template <class MR>
    allocator<MR>::~allocator()
    {
        delete header0_;
    }

#define MADI_ALC_ASSERT(h) \
    do { \
        MADI_ASSERT((uintptr_t)((h)->next) >= 4096); \
        MADI_ASSERT((h)->size <= 32 * 1024 * 1024); \
    } while (false)

    // K&R malloc
    template <class MR>
    template <bool extend, class T>
    void * allocator<MR>::allocate(size_t size, T& param)
    {
        size_t n_units =
            ((size + sizeof(alc_header) - 1)) / sizeof(alc_header) + 1;

        alc_header *prev = free_list_;
        alc_header *h = prev->next;
        for (;;) {
            if (h == NULL) {
                if (extend) {
                    size_t prev_size = mr_->size();

                    alc_header *new_header =
                        (alc_header *)mr_->extend_to(prev_size * 2, param);

                    if (new_header == NULL)
                        return NULL;

                    size_t ext_size = mr_->size() - prev_size;
                    size_t new_n_units = ext_size / sizeof(alc_header);

                    MADI_ASSERT(ext_size % sizeof(alc_header) == 0);

                    new_header->next = NULL;
                    new_header->size = new_n_units;

                    deallocate((void *)(new_header + 1));

                    // retry this loop
                    h = free_list_;
                } else {
                    return NULL;
                }
            }

            if (h->size >= n_units)
                break;

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

        h->next = NULL;

        MADI_DPUTS1("allocate [%p, %p) (size = %ld; requested = %ld)",
                    h, h + n_units, n_units * sizeof(alc_header), size);

        return (void *)(h + 1);
    }

    template <class MR>
    void allocator<MR>::deallocate(void *p)
    {
        alc_header *header = (alc_header *)p - 1;

        MADI_DPUTS1("deallocate [%p, %p) (size = %ld)",
                    header, header + header->size, header->size * sizeof(alc_header));

        alc_header *h = free_list_->next;

        if (h == NULL) {
            free_list_->next = header;
            return;
        }

        if (header < h) {
            if (header + header->size == h) {
                header->size += h->size;
                header->next = h->next;
            } else {
                header->next = h;
            }
            free_list_->next = header;
            return;
        }

        for (;;) {
            if (h < header && h->next == NULL)
                break;

            if (h < header && header < h->next)
                break;

            if (h->next == NULL) {
                madi::die("Something is wrong with free list");
            }

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
    }
}
}

#endif
