#ifndef MADM_GLOBAL_CLOCK_H
#define MADM_GLOBAL_CLOCK_H

#include <ctime>
#include <limits>
#include <algorithm>

#include <mpi.h>

#include "madm/madm_comm_config.h"

namespace madi {

    class global_clock {
#ifndef GLOBAL_CLOCK_N_SYNC_REPEATS
#define GLOBAL_CLOCK_N_SYNC_REPEATS 10000
#endif
        static constexpr int n_sync_repeats_ = GLOBAL_CLOCK_N_SYNC_REPEATS;
#undef GLOBAL_CLOCK_N_SYNC_REPEATS

        bool initialized_ = false;

        MPI_Comm intra_comm_;
        MPI_Comm inter_comm_;
        int intra_rank_;
        int inter_rank_;
        int global_rank_;
        int intra_nproc_;
        int inter_nproc_;
        int global_nproc_;

        int64_t offset_;

        static inline global_clock& get_instance_() {
            static global_clock my_instance;
            return my_instance;
        }

        static uint32_t jenkins_hash_(const uint8_t* key, int len) {
            int i = 0;
            uint32_t hash = 0;
            while (i != len) {
                hash += key[i++];
                hash += hash << 10;
                hash ^= hash >> 6;
            }
            hash += hash << 3;
            hash ^= hash >> 11;
            hash += hash << 15;
            return hash;
        }

    public:
        static inline uint64_t get_local_time() {
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            return (uint64_t)ts.tv_sec * 1000000000 + (uint64_t)ts.tv_nsec;
        }

        static inline uint64_t get_time() {
            global_clock& gc = get_instance_();
            uint64_t t = get_local_time();
            return t - gc.offset_;
        }

        static inline uint64_t get_offset() {
            global_clock& gc = get_instance_();
            return gc.offset_;
        }

        static void sync() {
            global_clock& gc = get_instance_();

            MPI_Barrier(MPI_COMM_WORLD);

            // Only the leader of each node involves in clock synchronization
            if (gc.intra_rank_ == 0) {
                int64_t* offsets = new int64_t[gc.inter_nproc_]();

                // uses the reference clock of the node of rank 0
                if (gc.inter_rank_ == 0) {
                    // O(n) where n = # of nodes
                    for (int i = 1; i < gc.inter_nproc_; i++) {
                        uint64_t min_gap = std::numeric_limits<uint64_t>::max();
                        for (int j = 0; j < n_sync_repeats_; j++) {
                            uint64_t t0 = get_local_time();
                            uint64_t t1;
                            MPI_Send(&t0, 1, MPI_UINT64_T, i, j, gc.inter_comm_);
                            MPI_Recv(&t1, 1, MPI_UINT64_T, i, j, gc.inter_comm_, MPI_STATUS_IGNORE);
                            uint64_t t2 = get_local_time();

                            // adopt the fastest communitation
                            if (t2 - t0 < min_gap) {
                                min_gap = t2 - t0;
                                offsets[i] = t1 - (int64_t)((t0 + t2) / 2);
                            }
                        }
                    }

                    // Adjust the offset to begin with t=0
                    int64_t begin_time = get_local_time();
                    for (int i = 0; i < gc.inter_nproc_; i++) {
                        offsets[i] += begin_time;
                    }
                } else {
                    for (int j = 0; j < n_sync_repeats_; j++) {
                        uint64_t t0;
                        MPI_Recv(&t0, 1, MPI_UINT64_T, 0, j, gc.inter_comm_, MPI_STATUS_IGNORE);
                        uint64_t t1 = get_local_time();
                        MPI_Send(&t1, 1, MPI_UINT64_T, 0, j, gc.inter_comm_);
                    }
                }

                MPI_Scatter(offsets, 1, MPI_INT64_T,
                        &gc.offset_, 1, MPI_INT64_T,
                        0, gc.inter_comm_);

                delete[] offsets;
            }

            // Share the offset within the node
            MPI_Bcast(&gc.offset_, 1, MPI_INT64_T, 0, gc.intra_comm_);

            MPI_Barrier(MPI_COMM_WORLD);
        }

        static void init() {
            global_clock& gc = get_instance_();

            if (!gc.initialized_) {
                MPI_Comm_rank(MPI_COMM_WORLD, &gc.global_rank_);
                MPI_Comm_size(MPI_COMM_WORLD, &gc.global_nproc_);
                gc.offset_ = 0;

                char proc_name[MPI_MAX_PROCESSOR_NAME];
                int proc_name_len;
                MPI_Get_processor_name(proc_name, &proc_name_len);
                int color = abs((int)jenkins_hash_((uint8_t*)proc_name, proc_name_len));

                MPI_Comm_split(MPI_COMM_WORLD, color, 0, &gc.intra_comm_);
                MPI_Comm_rank(gc.intra_comm_, &gc.intra_rank_);
                MPI_Comm_size(gc.intra_comm_, &gc.intra_nproc_);

                MPI_Comm_split(MPI_COMM_WORLD, gc.intra_rank_, 0, &gc.inter_comm_);
                MPI_Comm_rank(gc.inter_comm_, &gc.inter_rank_);
                MPI_Comm_size(gc.inter_comm_, &gc.inter_nproc_);

#if !(MADI_LOGGER_DISABLE || MADI_LOGGER_DISABLE_TRACE)
                sync();
#endif

                gc.initialized_ = true;
            }
        }
    };

}

#endif
