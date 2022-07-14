#ifndef MADM_LOGGER_STATS_H
#define MADM_LOGGER_STATS_H

#include "madm_logger_kind.h"
#include "madm_global_clock.h"

#include <mpi.h>

namespace madi {

    class logger_stats {
    public:
        using begin_data = uint64_t;

    private:
        using kind = typename logger_kind::kind;

        int rank_;
        int nproc_;

        uint64_t t_begin_;
        uint64_t t_end_;

        uint64_t t_checkpoint_;

        bool     stat_print_per_rank_;
        uint64_t stat_acc_[(size_t)kind::__N_KINDS];
        uint64_t stat_acc_total_[(size_t)kind::__N_KINDS];
        uint64_t stat_count_[(size_t)kind::__N_KINDS];
        uint64_t stat_count_total_[(size_t)kind::__N_KINDS];

        // special feature for kind::STEAL_TASK_COPY
        uint64_t stat_task_size_acc_;
        uint64_t stat_task_size_acc_total_;

        static inline logger_stats& get_instance_() {
            static logger_stats my_instance;
            return my_instance;
        }

        static void print_kind_stat_(kind k, int rank) {
            if (logger_kind::is_valid_kind(k)) {
                logger_stats& lgr = get_instance_();
                if (lgr.stat_print_per_rank_) {
                    uint64_t acc = lgr.stat_acc_[(size_t)k];
                    uint64_t acc_total = lgr.t_end_ - lgr.t_begin_;
                    uint64_t count = lgr.stat_count_[(size_t)k];
                    printf("(Rank %3d) %-23s : %10.6f %% ( %15ld ns / %15ld ns ) count: %8ld ave: %8ld ns\n",
                           rank, logger_kind::kind_name(k), (double)acc / acc_total * 100, acc, acc_total, count, acc / count);
                    if (k == kind::STEAL_TASK_COPY) {
                        printf("(Rank %3d) %-23s : %8ld bytes ( %8ld / %8ld )\n",
                               rank, "steal_task_size", lgr.stat_task_size_acc_ / count, lgr.stat_task_size_acc_, count);
                    }
                } else {
                    uint64_t acc = lgr.stat_acc_total_[(size_t)k];
                    uint64_t acc_total = (lgr.t_end_ - lgr.t_begin_) * lgr.nproc_;
                    uint64_t count = lgr.stat_count_total_[(size_t)k];
                    printf("  %-23s : %10.6f %% ( %15ld ns / %15ld ns ) count: %8ld ave: %8ld ns\n",
                           logger_kind::kind_name(k), (double)acc / acc_total * 100, acc, acc_total, count, acc / count);
                    if (k == kind::STEAL_TASK_COPY) {
                        printf("  %-23s : %8ld bytes ( %8ld / %8ld )\n",
                               "steal_task_size", lgr.stat_task_size_acc_total_ / count, lgr.stat_task_size_acc_total_, count);
                    }
                }
            }
        }

        static void acc_init_() {
            logger_stats& lgr = get_instance_();
            for (size_t k = 0; k < (size_t)kind::__N_KINDS; k++) {
                lgr.stat_acc_[k] = 0;
                lgr.stat_acc_total_[k] = 0;
                lgr.stat_count_[k] = 0;
                lgr.stat_count_total_[k] = 0;
            }
            lgr.stat_task_size_acc_ = 0;
            lgr.stat_task_size_acc_total_ = 0;
        }

        template <kind k>
        static void acc_stat_(uint64_t t0, uint64_t t1) {
            logger_stats& lgr = get_instance_();
            lgr.stat_acc_[(size_t)k] += t1 - t0;
            lgr.stat_count_[(size_t)k]++;
        }

        static void print_stat_(int rank) {
            for (size_t k = 1; k < (size_t)kind::__N_KINDS; k++) {
                print_kind_stat_((kind)k, rank);
            }
            printf("\n");
        }

    public:
        static void init(int rank, int nproc) {
            logger_stats& lgr = get_instance_();
            lgr.rank_ = rank;
            lgr.nproc_ = nproc;
            lgr.t_checkpoint_ = 0;

            lgr.stat_print_per_rank_ = get_env("MADM_LOGGER_PRINT_STAT_PER_RANK", false);

            acc_init_();
        }

        static void flush(uint64_t t_begin, uint64_t t_end) {}

        static void flush_and_print_stat(uint64_t t_begin, uint64_t t_end) {
            logger_stats& lgr = get_instance_();

            // TODO: it is not easy to accurately show only events within [t_begin, t_end]
            lgr.t_begin_ = t_begin;
            lgr.t_end_ = t_end;

            if (lgr.stat_print_per_rank_) {
                if (lgr.rank_ == 0) {
                    print_stat_(0);
                    for (int i = 1; i < lgr.nproc_; i++) {
                        MPI_Recv(lgr.stat_acc_, (size_t)kind::__N_KINDS, MPI_UINT64_T,
                                 i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                        MPI_Recv(lgr.stat_count_, (size_t)kind::__N_KINDS, MPI_UINT64_T,
                                 i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                        MPI_Recv(&lgr.stat_task_size_acc_, 1, MPI_UINT64_T,
                                 i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                        print_stat_(i);
                    }
                } else {
                    MPI_Send(lgr.stat_acc_, (size_t)kind::__N_KINDS, MPI_UINT64_T,
                             0, 0, MPI_COMM_WORLD);
                    MPI_Send(lgr.stat_count_, (size_t)kind::__N_KINDS, MPI_UINT64_T,
                             0, 0, MPI_COMM_WORLD);
                    MPI_Send(&lgr.stat_task_size_acc_, 1, MPI_UINT64_T,
                             0, 0, MPI_COMM_WORLD);
                }
            } else {
                MPI_Reduce(lgr.stat_acc_, lgr.stat_acc_total_, (size_t)kind::__N_KINDS,
                           MPI_UINT64_T, MPI_SUM, 0, MPI_COMM_WORLD);
                MPI_Reduce(lgr.stat_count_, lgr.stat_count_total_, (size_t)kind::__N_KINDS,
                           MPI_UINT64_T, MPI_SUM, 0, MPI_COMM_WORLD);
                MPI_Reduce(&lgr.stat_task_size_acc_, &lgr.stat_task_size_acc_total_, 1,
                           MPI_UINT64_T, MPI_SUM, 0, MPI_COMM_WORLD);
                if (lgr.rank_ == 0) {
                    print_stat_(0);
                }
            }

            acc_init_();
        }

        static void warmup() {}

        static void clear() {
            acc_init_();
        }

        template <kind k>
        static inline void checkpoint() {
            if (logger_kind::is_valid_kind(k)) {
                logger_stats& lgr = get_instance_();
                uint64_t t = global_clock::get_time();
                acc_stat_<k>(lgr.t_checkpoint_, t);
                lgr.t_checkpoint_ = t;
            }
        }

        template <kind k>
        static inline begin_data begin_event() {
            if (logger_kind::is_valid_kind(k)) {
                return global_clock::get_time();
            } else {
                return 0;
            }
        }

        template <kind k>
        static inline begin_data begin_event(uint64_t t) {
            if (logger_kind::is_valid_kind(k)) {
                return t;
            } else {
                return 0;
            }
        }

        template <kind k>
        static inline void end_event(begin_data t0) {
            if (logger_kind::is_valid_kind(k)) {
                uint64_t t = global_clock::get_time();
                acc_stat_<k>(t0, t);
            }
        }

        template <kind k, typename MISC>
        static inline void end_event(begin_data t0, MISC m) {
            if (logger_kind::is_valid_kind(k)) {
                uint64_t t = global_clock::get_time();
                acc_stat_<k>(t0, t);
                if (k == kind::STEAL_TASK_COPY) {
                    logger_stats& lgr = get_instance_();
                    lgr.stat_task_size_acc_ += m;
                }
            }
        }
    };

}

#endif
