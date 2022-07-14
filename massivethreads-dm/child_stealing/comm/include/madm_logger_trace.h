#ifndef MADM_LOGGER_TRACE_H
#define MADM_LOGGER_TRACE_H

#include "madm_logger_kind.h"
#include "madm_global_clock.h"

/* #define MLOG_DISABLE_CHECK_BUFFER_SIZE 1 */
/* #define MLOG_DISABLE_REALLOC_BUFFER    1 */
#include "mlog/mlog.h"

#include <mpi.h>

namespace madi {

    class logger_trace {
    public:
        using begin_data = void*;

    private:
        using kind = typename logger_kind::kind;

        int rank_;
        int nproc_;
        begin_data bp_ = nullptr;
        FILE* stream_;

        uint64_t t_begin_;
        uint64_t t_end_;

        bool     stat_print_per_rank_;
        uint64_t stat_acc_[(size_t)kind::__N_KINDS];
        uint64_t stat_acc_total_[(size_t)kind::__N_KINDS];
        uint64_t stat_count_[(size_t)kind::__N_KINDS];
        uint64_t stat_count_total_[(size_t)kind::__N_KINDS];

        bool output_trace_;

        mlog_data_t md_;

        static inline logger_trace& get_instance_() {
            static logger_trace my_instance;
            return my_instance;
        }

        template <kind k>
        static void* logger_decoder_tl_(FILE* stream, int _rank0, int _rank1, void* buf0, void* buf1) {
            logger_trace& lgr = get_instance_();

            uint64_t t0 = MLOG_READ_ARG(&buf0, uint64_t);
            uint64_t t1 = MLOG_READ_ARG(&buf1, uint64_t);

            if (t1 < lgr.t_begin_ || lgr.t_end_ < t0) {
                return buf1;
            }

            acc_stat_<k>(t0, t1);

            if (lgr.output_trace_) {
                fprintf(stream, "%d,%lu,%d,%lu,%s\n", lgr.rank_, t0, lgr.rank_, t1, logger_kind::kind_name(k));
            }
            return buf1;
        }

        template <kind k, typename MISC>
        static void* logger_decoder_tl_w_misc_(FILE* stream, int _rank0, int _rank1, void* buf0, void* buf1) {
            logger_trace& lgr = get_instance_();

            uint64_t t0 = MLOG_READ_ARG(&buf0, uint64_t);
            uint64_t t1 = MLOG_READ_ARG(&buf1, uint64_t);
            MISC     m  = MLOG_READ_ARG(&buf1, MISC);

            if (t1 < lgr.t_begin_ || lgr.t_end_ < t0) {
                return buf1;
            }

            acc_stat_<k>(t0, t1);

            if (lgr.output_trace_) {
                std::stringstream ss;
                ss << m;
                fprintf(stream, "%d,%lu,%d,%lu,%s,%s\n", lgr.rank_, t0, lgr.rank_, t1, logger_kind::kind_name(k), ss.str().c_str());
            }
            return buf1;
        }

        static void print_kind_stat_(kind k, int rank) {
            if (logger_kind::is_valid_kind(k)) {
                logger_trace& lgr = get_instance_();
                if (lgr.stat_print_per_rank_) {
                    uint64_t acc = lgr.stat_acc_[(size_t)k];
                    uint64_t acc_total = lgr.t_end_ - lgr.t_begin_;
                    uint64_t count = lgr.stat_count_[(size_t)k];
                    printf("(Rank %3d) %-23s : %10.6f %% ( %15ld ns / %15ld ns ) count: %8ld\n",
                           rank, logger_kind::kind_name(k), (double)acc / acc_total * 100, acc, acc_total, count);
                } else {
                    uint64_t acc = lgr.stat_acc_total_[(size_t)k];
                    uint64_t acc_total = (lgr.t_end_ - lgr.t_begin_) * lgr.nproc_;
                    uint64_t count = lgr.stat_count_total_[(size_t)k];
                    printf("  %-23s : %10.6f %% ( %15ld ns / %15ld ns ) count: %8ld\n",
                           logger_kind::kind_name(k), (double)acc / acc_total * 100, acc, acc_total, count);
                }
            }
        }

        template <kind k>
        static void acc_stat_(uint64_t t0, uint64_t t1) {
            logger_trace& lgr = get_instance_();
            uint64_t t0_ = std::max(t0, lgr.t_begin_);
            uint64_t t1_ = std::min(t1, lgr.t_end_);
            if (t1_ > t0_) {
                lgr.stat_acc_[(size_t)k] += t1_ - t0_;
                lgr.stat_count_[(size_t)k]++;
            }
        }

        static void print_stat_(int rank) {
            for (size_t k = 1; k < (size_t)kind::__N_KINDS; k++) {
                print_kind_stat_((kind)k, rank);
            }
            printf("\n");
        }

    public:
        static void init(int rank, int nproc) {
            size_t size = get_env("MADM_LOGGER_INITIAL_SIZE", 1 << 20);

            logger_trace& lgr = get_instance_();
            lgr.rank_ = rank;
            lgr.nproc_ = nproc;

            lgr.stat_print_per_rank_ = get_env("MADM_LOGGER_PRINT_STAT_PER_RANK", false);
            lgr.output_trace_        = get_env("MADM_LOGGER_OUTPUT_TRACE", true);

            mlog_init(&lgr.md_, 1, size);

            if (lgr.output_trace_) {
                char filename[128];
                sprintf(filename, "madm_log_%d.ignore", rank);
                lgr.stream_ = fopen(filename, "w+");
            }
        }

        static void flush(uint64_t t_begin, uint64_t t_end) {
            logger_trace& lgr = get_instance_();

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
            logger_trace& lgr = get_instance_();

            for (size_t k = 0; k < (size_t)kind::__N_KINDS; k++) {
                lgr.stat_acc_[k] = 0;
                lgr.stat_acc_total_[k] = 0;
                lgr.stat_count_[k] = 0;
                lgr.stat_count_total_[k] = 0;
            }

            if (lgr.bp_) {
                // The time from the last checkpoint to `t_end` should be added
                // assuming that the current context is in `WORKER_BUSY`
                uint64_t t0 = *((uint64_t*)lgr.bp_);
                acc_stat_<kind::WORKER_BUSY>(t0, t_end);
            }

            flush(t_begin, t_end);

            if (lgr.stat_print_per_rank_) {
                if (lgr.rank_ == 0) {
                    print_stat_(0);
                    for (int i = 1; i < lgr.nproc_; i++) {
                        MPI_Recv(lgr.stat_acc_, (size_t)kind::__N_KINDS, MPI_UINT64_T,
                                 i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                        MPI_Recv(lgr.stat_count_, (size_t)kind::__N_KINDS, MPI_UINT64_T,
                                 i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                        print_stat_(i);
                    }
                } else {
                    MPI_Send(lgr.stat_acc_, (size_t)kind::__N_KINDS, MPI_UINT64_T,
                             0, 0, MPI_COMM_WORLD);
                    MPI_Send(lgr.stat_count_, (size_t)kind::__N_KINDS, MPI_UINT64_T,
                             0, 0, MPI_COMM_WORLD);
                }
            } else {
                MPI_Reduce(lgr.stat_acc_, lgr.stat_acc_total_, (size_t)kind::__N_KINDS,
                           MPI_UINT64_T, MPI_SUM, 0, MPI_COMM_WORLD);
                MPI_Reduce(lgr.stat_count_, lgr.stat_count_total_, (size_t)kind::__N_KINDS,
                           MPI_UINT64_T, MPI_SUM, 0, MPI_COMM_WORLD);
                if (lgr.rank_ == 0) {
                    print_stat_(0);
                }
            }
        }

        static void warmup() {
            logger_trace& lgr = get_instance_();
            mlog_warmup(&lgr.md_, 0);
        }

        static void clear() {
            logger_trace& lgr = get_instance_();
            mlog_clear_all(&lgr.md_);
            lgr.bp_ = nullptr;
        }

        template <kind k>
        static inline void checkpoint() {
            if (logger_kind::is_valid_kind(k)) {
                logger_trace& lgr = get_instance_();
                uint64_t t = global_clock::get_time();
                if (lgr.bp_) {
                    auto fn = &logger_decoder_tl_<k>;
                    MLOG_END(&lgr.md_, 0, lgr.bp_, fn, t);
                }
                lgr.bp_ = MLOG_BEGIN(&lgr.md_, 0, t);
            }
        }

        template <kind k>
        static inline begin_data begin_event() {
            if (logger_kind::is_valid_kind(k)) {
                logger_trace& lgr = get_instance_();
                uint64_t t = global_clock::get_time();
                begin_data bp = MLOG_BEGIN(&lgr.md_, 0, t);
                return bp;
            } else {
                return nullptr;
            }
        }

        template <kind k>
        static inline begin_data begin_event(uint64_t t) {
            if (logger_kind::is_valid_kind(k)) {
                logger_trace& lgr = get_instance_();
                begin_data bp = MLOG_BEGIN(&lgr.md_, 0, t);
                return bp;
            } else {
                return nullptr;
            }
        }

        template <kind k>
        static inline void end_event(begin_data bp) {
            if (logger_kind::is_valid_kind(k)) {
                logger_trace& lgr = get_instance_();
                uint64_t t = global_clock::get_time();
                auto fn = &logger_decoder_tl_<k>;
                MLOG_END(&lgr.md_, 0, bp, fn, t);
            }
        }

        template <kind k, typename MISC>
        static inline void end_event(begin_data bp, MISC m) {
            if (logger_kind::is_valid_kind(k)) {
                logger_trace& lgr = get_instance_();
                uint64_t t = global_clock::get_time();
                auto fn = &logger_decoder_tl_w_misc_<k, MISC>;
                MLOG_END(&lgr.md_, 0, bp, fn, t, m);
            }
        }
    };

}

#endif
