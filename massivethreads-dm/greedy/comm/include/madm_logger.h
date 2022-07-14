#ifndef MADM_LOGGER_H
#define MADM_LOGGER_H

#include <cstdio>
#include <sstream>

#include "madm/madm_comm_config.h"
#include "madm_misc.h"
#include "madm_global_clock.h"

/* #define MLOG_DISABLE_CHECK_BUFFER_SIZE 1 */
/* #define MLOG_DISABLE_REALLOC_BUFFER    1 */
#include "mlog/mlog.h"
#include "mpi.h"

namespace madi {

    class logger {
    public:
        using begin_data = void*;
        enum class kind {
            INIT,
            TEST,
            SCHED,
            THREAD,

            TASKQ_PUSH,
            TASKQ_POP,
            TASKQ_STEAL,
            TASKQ_EMPTY,

            FUTURE_POOL_SYNC,
            FUTURE_POOL_FILL,
            FUTURE_POOL_GET,

            DIST_POOL_PUSH,
            DIST_POOL_POP,

            DIST_SPINLOCK_LOCK,
            DIST_SPINLOCK_UNLOCK,

            WORKER_RESUME_SUSPENDED,
            WORKER_RESUME_STOLEN,
            WORKER_TRY_STEAL,

            COMM_PUT,
            COMM_GET,
            COMM_FENCE,
            COMM_FETCH_AND_ADD,
            COMM_TRYLOCK,
            COMM_LOCK,
            COMM_UNLOCK,
            COMM_POLL,
            COMM_MALLOC,
            COMM_FREE,

            OTHER,
            __N_KINDS,
        };

    private:
        mlog_data_t md_;
        int rank_;
        int nproc_;
        begin_data bp_ = nullptr;
        FILE* stream_;

        uint64_t t_begin_;
        uint64_t t_end_;

        bool     stat_print_per_rank_;
        uint64_t stat_acc_[(size_t)kind::__N_KINDS];
        uint64_t stat_acc_total_[(size_t)kind::__N_KINDS];

        bool output_trace;

        static inline logger& get_instance_() {
            static logger my_instance;
            return my_instance;
        }

        static constexpr bool kind_included_(kind k, kind kinds[], int n) {
            return n > 0 && (k == *kinds || kind_included_(k, kinds + 1, n - 1));
        }

        static constexpr bool is_valid_kind_(kind k) {
            kind enabled_kinds[] = MADI_LOGGER_ENABLED_KINDS;
            kind disabled_kinds[] = MADI_LOGGER_DISABLED_KINDS;

            static_assert(!(sizeof(enabled_kinds) > 0 && sizeof(disabled_kinds) > 0),
                          "Enabled kinds and disabled kinds cannot be specified at the same time.");

            if (sizeof(enabled_kinds) > 0) {
                return kind_included_(k, enabled_kinds, sizeof(enabled_kinds) / sizeof(*enabled_kinds));
            } else if (sizeof(disabled_kinds) > 0) {
                return !kind_included_(k, disabled_kinds, sizeof(disabled_kinds) / sizeof(*disabled_kinds));
            } else {
                return true;
            }
        }

        static constexpr const char* kind_name_(kind k) {
            switch (k) {
                case kind::INIT:                    return "";
                case kind::TEST:                    return "test";
                case kind::SCHED:                   return "sched";
                case kind::THREAD:                  return "thread";

                case kind::TASKQ_PUSH:              return "taskq_push";
                case kind::TASKQ_POP:               return "taskq_pop";
                case kind::TASKQ_STEAL:             return "taskq_steal";
                case kind::TASKQ_EMPTY:             return "taskq_empty";

                case kind::FUTURE_POOL_SYNC:        return "future_pool_sync";
                case kind::FUTURE_POOL_FILL:        return "future_pool_fill";
                case kind::FUTURE_POOL_GET:         return "future_pool_get";

                case kind::DIST_POOL_PUSH:          return "dist_pool_push";
                case kind::DIST_POOL_POP:           return "dist_pool_pop";

                case kind::DIST_SPINLOCK_LOCK:      return "dist_spinlock_lock";
                case kind::DIST_SPINLOCK_UNLOCK:    return "dist_spinlock_unlock";

                case kind::WORKER_RESUME_SUSPENDED: return "worker_resume_suspended";
                case kind::WORKER_RESUME_STOLEN:    return "worker_resume_stolen";
                case kind::WORKER_TRY_STEAL:        return "worker_try_steal";

                case kind::COMM_PUT:                return "comm_put";
                case kind::COMM_GET:                return "comm_get";
                case kind::COMM_FENCE:              return "comm_fence";
                case kind::COMM_FETCH_AND_ADD:      return "comm_fetch_and_add";
                case kind::COMM_TRYLOCK:            return "comm_trylock";
                case kind::COMM_LOCK:               return "comm_lock";
                case kind::COMM_UNLOCK:             return "comm_unlock";
                case kind::COMM_POLL:               return "comm_poll";
                case kind::COMM_MALLOC:             return "comm_malloc";
                case kind::COMM_FREE:               return "comm_free";

                default:                            return "other";
            }
        }

        template <kind k>
        static void print_kind_stat_(int rank) {
            if (is_valid_kind_(k)) {
                logger& lgr = get_instance_();
                if (lgr.stat_print_per_rank_) {
                    uint64_t acc = lgr.stat_acc_[(size_t)k];
                    uint64_t acc_total = lgr.t_end_ - lgr.t_begin_;
                    printf("(Rank %3d) %25s: %10.6f %% ( %15ld ns / %15ld ns )\n",
                           rank, kind_name_(k), (double)acc / acc_total * 100, acc, acc_total);
                } else {
                    uint64_t acc = lgr.stat_acc_total_[(size_t)k];
                    uint64_t acc_total = (lgr.t_end_ - lgr.t_begin_) * lgr.nproc_;
                    printf("%25s: %10.6f %% ( %15ld ns / %15ld ns )\n",
                           kind_name_(k), (double)acc / acc_total * 100, acc, acc_total);
                }
            }
        }

        template <kind k>
        static void acc_stat_(uint64_t t0, uint64_t t1) {
            logger& lgr = get_instance_();
            uint64_t t0_ = std::max(t0, lgr.t_begin_);
            uint64_t t1_ = std::min(t1, lgr.t_end_);
            if (t1_ > t0_) {
                lgr.stat_acc_[(size_t)k] += t1_ - t0_;
            }
        }

        static void print_stat_(int rank) {
            print_kind_stat_<kind::TEST>(rank);
            print_kind_stat_<kind::SCHED>(rank);
            print_kind_stat_<kind::THREAD>(rank);

            print_kind_stat_<kind::TASKQ_PUSH>(rank);
            print_kind_stat_<kind::TASKQ_POP>(rank);
            print_kind_stat_<kind::TASKQ_STEAL>(rank);
            print_kind_stat_<kind::TASKQ_EMPTY>(rank);

            print_kind_stat_<kind::FUTURE_POOL_SYNC>(rank);
            print_kind_stat_<kind::FUTURE_POOL_FILL>(rank);
            print_kind_stat_<kind::FUTURE_POOL_GET>(rank);

            print_kind_stat_<kind::DIST_POOL_PUSH>(rank);
            print_kind_stat_<kind::DIST_POOL_POP>(rank);

            print_kind_stat_<kind::DIST_SPINLOCK_LOCK>(rank);
            print_kind_stat_<kind::DIST_SPINLOCK_UNLOCK>(rank);

            print_kind_stat_<kind::WORKER_RESUME_SUSPENDED>(rank);
            print_kind_stat_<kind::WORKER_RESUME_STOLEN>(rank);
            print_kind_stat_<kind::WORKER_TRY_STEAL>(rank);

            print_kind_stat_<kind::COMM_PUT>(rank);
            print_kind_stat_<kind::COMM_GET>(rank);
            print_kind_stat_<kind::COMM_FENCE>(rank);
            print_kind_stat_<kind::COMM_FETCH_AND_ADD>(rank);
            print_kind_stat_<kind::COMM_TRYLOCK>(rank);
            print_kind_stat_<kind::COMM_LOCK>(rank);
            print_kind_stat_<kind::COMM_UNLOCK>(rank);
            print_kind_stat_<kind::COMM_POLL>(rank);
            print_kind_stat_<kind::COMM_MALLOC>(rank);
            print_kind_stat_<kind::COMM_FREE>(rank);

            print_kind_stat_<kind::OTHER>(rank);

            printf("\n");
        }

        template <kind k>
        static void* logger_decoder_tl_(FILE* stream, int _rank0, int _rank1, void* buf0, void* buf1) {
            logger& lgr = get_instance_();

            uint64_t t_offset = global_clock::get_offset();

            uint64_t t0 = MLOG_READ_ARG(&buf0, uint64_t) - t_offset;
            uint64_t t1 = MLOG_READ_ARG(&buf1, uint64_t) - t_offset;

            if (t1 < lgr.t_begin_ || lgr.t_end_ < t0) {
                return buf1;
            }

            acc_stat_<k>(t0, t1);

            if (lgr.output_trace) {
                fprintf(stream, "%d,%lu,%d,%lu,%s\n", lgr.rank_, t0, lgr.rank_, t1, kind_name_(k));
            }
            return buf1;
        }

        template <kind k, typename MISC>
        static void* logger_decoder_tl_w_misc_(FILE* stream, int _rank0, int _rank1, void* buf0, void* buf1) {
            logger& lgr = get_instance_();

            uint64_t t_offset = global_clock::get_offset();

            uint64_t t0 = MLOG_READ_ARG(&buf0, uint64_t) - t_offset;
            uint64_t t1 = MLOG_READ_ARG(&buf1, uint64_t) - t_offset;
            MISC     m  = MLOG_READ_ARG(&buf1, MISC);

            if (t1 < lgr.t_begin_ || lgr.t_end_ < t0) {
                return buf1;
            }

            acc_stat_<k>(t0, t1);

            if (lgr.output_trace) {
                std::stringstream ss;
                ss << m;
                fprintf(stream, "%d,%lu,%d,%lu,%s,%s\n", lgr.rank_, t0, lgr.rank_, t1, kind_name_(k), ss.str().c_str());
            }
            return buf1;
        }

    public:
#if MADI_ENABLE_LOGGER
        static void init(int rank, int nproc) {
            size_t size = get_env("MADM_LOGGER_INITIAL_SIZE", 1 << 20);

            logger& lgr = get_instance_();
            lgr.rank_ = rank;
            lgr.nproc_ = nproc;

            lgr.stat_print_per_rank_ = get_env("MADM_LOGGER_PRINT_STAT_PER_RANK", false);
            lgr.output_trace         = get_env("MADM_LOGGER_OUTPUT_TRACE", true);

            mlog_init(&lgr.md_, 1, size);

            if (lgr.output_trace) {
                char filename[128];
                sprintf(filename, "madm_log_%d.ignore", rank);
                lgr.stream_ = fopen(filename, "w+");
            }
        }

        static void flush(uint64_t t_begin, uint64_t t_end) {
            logger& lgr = get_instance_();

            lgr.t_begin_ = t_begin;
            lgr.t_end_ = t_end;

            uint64_t t = 0;
            if (lgr.bp_) {
                // The last checkpoint can be lost, so restore it
                t = *((uint64_t*)lgr.bp_);
            }

            mlog_flush_all(&lgr.md_, lgr.stream_);

            if (lgr.bp_) {
                lgr.bp_ = MLOG_BEGIN(&lgr.md_, 0, t);
            }
        }

        static void flush_and_print_stat(uint64_t t_begin, uint64_t t_end) {
            logger& lgr = get_instance_();

            for (size_t k = 0; k < (size_t)kind::__N_KINDS; k++) {
                lgr.stat_acc_[k] = 0;
                lgr.stat_acc_total_[k] = 0;
            }

            if (lgr.bp_) {
                // The time from the last checkpoint to `t_end` should be added
                // assuming that the current context is in `THREAD`
                uint64_t t_offset = global_clock::get_offset();
                uint64_t t0 = *((uint64_t*)lgr.bp_) - t_offset;
                acc_stat_<kind::THREAD>(t0, t_end);
            }

            flush(t_begin, t_end);

            if (lgr.stat_print_per_rank_) {
                if (lgr.rank_ == 0) {
                    print_stat_(0);
                    for (int i = 1; i < lgr.nproc_; i++) {
                        MPI_Recv(lgr.stat_acc_, (size_t)kind::__N_KINDS, MPI_UINT64_T,
                                 i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                        print_stat_(i);
                    }
                } else {
                    MPI_Send(lgr.stat_acc_, (size_t)kind::__N_KINDS, MPI_UINT64_T,
                             0, 0, MPI_COMM_WORLD);
                }
            } else {
                MPI_Reduce(lgr.stat_acc_, lgr.stat_acc_total_, (size_t)kind::__N_KINDS,
                           MPI_UINT64_T, MPI_SUM, 0, MPI_COMM_WORLD);
                if (lgr.rank_ == 0) {
                    print_stat_(0);
                }
            }
        }

        static void warmup() {
            logger& lgr = get_instance_();
            mlog_warmup(&lgr.md_, 0);
        }

        static void clear() {
            logger& lgr = get_instance_();
            mlog_clear_all(&lgr.md_);
            lgr.bp_ = nullptr;
        }

        template <kind k>
        static inline void checkpoint() {
            if (is_valid_kind_(k)) {
                logger& lgr = get_instance_();
                uint64_t t = global_clock::get_local_time();
                if (lgr.bp_) {
                    auto fn = &logger_decoder_tl_<k>;
                    MLOG_END(&lgr.md_, 0, lgr.bp_, fn, t);
                }
                lgr.bp_ = MLOG_BEGIN(&lgr.md_, 0, t);
            }
        }

        template <kind k>
        static inline begin_data begin_event() {
            if (is_valid_kind_(k)) {
                logger& lgr = get_instance_();
                uint64_t t = global_clock::get_local_time();
                begin_data bp = MLOG_BEGIN(&lgr.md_, 0, t);
                return bp;
            } else {
                return nullptr;
            }
        }

        template <kind k>
        static inline void end_event(begin_data bp) {
            if (is_valid_kind_(k)) {
                logger& lgr = get_instance_();
                uint64_t t = global_clock::get_local_time();
                auto fn = &logger_decoder_tl_<k>;
                MLOG_END(&lgr.md_, 0, bp, fn, t);
            }
        }

        template <kind k, typename MISC>
        static inline void end_event(begin_data bp, MISC m) {
            if (is_valid_kind_(k)) {
                logger& lgr = get_instance_();
                uint64_t t = global_clock::get_local_time();
                auto fn = &logger_decoder_tl_w_misc_<k, MISC>;
                MLOG_END(&lgr.md_, 0, bp, fn, t, m);
            }
        }
#else
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
#endif
    };

}

#endif
