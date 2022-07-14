#ifndef MADM_LOGGER_DUMMY_H
#define MADM_LOGGER_DUMMY_H

#include "madm_logger_kind.h"

namespace madi {

    class logger_dummy {
    private:
        using kind = typename logger_kind::kind;

    public:
        using begin_data = void*;

        static void init(int rank, int nproc) {}
        static void flush(uint64_t t_begin, uint64_t t_end) {}
        static void flush_and_print_stat(uint64_t t_begin, uint64_t t_end) {}
        static void warmup() {}
        static void clear() {}
        template <kind k>
        static inline void checkpoint() {}
        template <kind k>
        static inline begin_data begin_event() { return NULL; }
        template <kind k>
        static inline void end_event(begin_data bp) {}
        template <kind k, typename MISC>
        static inline void end_event(begin_data bp, MISC m) {}
    };

}

#endif

