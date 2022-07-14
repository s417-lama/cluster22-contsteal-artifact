#ifndef MADM_LOGGER_H
#define MADM_LOGGER_H

#include <cstdio>
#include <sstream>

#include "madm/madm_comm_config.h"
#include "madm_misc.h"

#include "madm_logger_dummy.h"
#include "madm_logger_stats.h"
#include "madm_logger_trace.h"

namespace madi {

    template <class Impl>
    class logger_ {
    public:
        using begin_data = typename Impl::begin_data;
        using kind = typename logger_kind::kind;

        static void init(int rank, int nproc) {
            Impl::init(rank, nproc);
        }

        static void flush(uint64_t t_begin, uint64_t t_end) {
            Impl::flush(t_begin, t_end);
        }

        static void flush_and_print_stat(uint64_t t_begin, uint64_t t_end) {
            Impl::flush_and_print_stat(t_begin, t_end);
        }

        static void warmup() {
            Impl::warmup();
        }

        static void clear() {
            Impl::clear();
        }

        template <kind k>
        static inline void checkpoint() {
            Impl::template checkpoint<k>();
        }

        template <kind k>
        static inline begin_data begin_event() {
            return Impl::template begin_event<k>();
        }

        template <kind k>
        static inline void end_event(begin_data bp) {
            Impl::template end_event<k>(bp);
        }

        template <kind k, typename MISC>
        static inline void end_event(begin_data bp, MISC m) {
            Impl::template end_event<k, MISC>(bp, m);
        }
    };

#if MADI_LOGGER_DISABLE
    using logger = logger_<logger_dummy>;
#elif MADI_LOGGER_DISABLE_TRACE
    using logger = logger_<logger_stats>;
#else
    using logger = logger_<logger_trace>;
#endif

}

#endif
