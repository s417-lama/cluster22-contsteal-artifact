#pragma once
#ifndef DNPLOGGER_HPP_
#define DNPLOGGER_HPP_

#include <cstdio>

/* #define MLOG_DISABLE_CHECK_BUFFER_SIZE 1 */
/* #define MLOG_DISABLE_REALLOC_BUFFER    1 */
#include "mlog/mlog.h"

#include "madm_comm.h"
#include "madm_global_clock.h"

class dnplogger {
private:
#ifndef DNPLOGGER_ENABLE
#define DNPLOGGER_ENABLE 0
#endif

#ifndef DNPLOGGER_DEFAULT_SIZE
#define DNPLOGGER_DEFAULT_SIZE (1 << 20)
#endif
  static constexpr size_t default_size_ = DNPLOGGER_DEFAULT_SIZE;
#undef DNPLOGGER_DEFAULT_SIZE

#ifndef DNPLOGGER_PRINT_STAT_PER_RANK
#define DNPLOGGER_PRINT_STAT_PER_RANK 0
#endif
  static constexpr bool print_stat_per_rank_ = DNPLOGGER_PRINT_STAT_PER_RANK;
#undef DNPLOGGER_PRINT_STAT_PER_RANK

#ifndef DNPLOGGER_OUTPUT_TRACE
#define DNPLOGGER_OUTPUT_TRACE 1
#endif
  static constexpr bool output_trace_ = DNPLOGGER_OUTPUT_TRACE;
#undef DNPLOGGER_OUTPUT_TRACE

  mlog_data_t md_;
  int rank_;
  int nproc_;
  void* bp_;
  FILE* stream_;
  bool initialized_ = false;

  uint64_t compute_d_acc_ = 0;
  uint64_t begin_t_;
  uint64_t end_t_;

  static inline dnplogger& get_instance_() {
    static dnplogger my_instance;
    return my_instance;
  }

  static void* dnplogger_decoder_tl_(FILE* stream, int _rank0, int _rank1, void* buf0, void* buf1) {
    uint64_t t0   = MLOG_READ_ARG(&buf0, uint64_t);
    uint64_t t1   = MLOG_READ_ARG(&buf1, uint64_t);
    char*    kind = MLOG_READ_ARG(&buf1, char*);

    dnplogger& lgr = get_instance_();

    if (t1 < lgr.begin_t_ || lgr.end_t_ < t0) {
      return buf1;
    }

    if (strcmp(kind, "Compute") == 0) {
      lgr.compute_d_acc_ += t1 - t0;
    }

    if (output_trace_) {
      fprintf(stream, "%d,%lu,%d,%lu,%s\n", lgr.rank_, t0, lgr.rank_, t1, kind);
    }
    return buf1;
  }

public:
  static void init(int rank, int nproc, int n_threads = 1, size_t size = default_size_) {
    dnplogger& lgr = get_instance_();

    mlog_init(&lgr.md_, n_threads, size);

#if DNPLOGGER_ENABLE
    // TODO: sync may be called twice in madm and here
    madi::global_clock::sync();
#endif

    if (output_trace_) {
      char filename[128];
      sprintf(filename, "dnp_log_%d.ignore", rank);
      lgr.stream_ = fopen(filename, "w+");
    }

    lgr.rank_ = rank;
    lgr.nproc_ = nproc;
    lgr.initialized_ = true;
  }

  static void warmup() {
    dnplogger& lgr = get_instance_();
    mlog_warmup(&lgr.md_, 0);
  }

  static void clear() {
    dnplogger& lgr = get_instance_();
    mlog_clear_all(&lgr.md_);
  }

  static void flush(uint64_t t0, uint64_t t1) {
    dnplogger& lgr = get_instance_();
    lgr.begin_t_ = t0;
    lgr.end_t_ = t1;
    mlog_flush_all(&lgr.md_, lgr.stream_);
  }

  static void flush_and_print_stat(uint64_t t0, uint64_t t1) {
#if DNPLOGGER_ENABLE
    dnplogger& lgr = get_instance_();

    lgr.compute_d_acc_ = 0;

    flush(t0, t1);

    if (print_stat_per_rank_) {
      double busy_ratio = (double)lgr.compute_d_acc_ / (t1 - t0) * 100;
      printf("(Rank %d) Busy: %f %% ( %ld ns / %ld ns )\n", lgr.rank_, busy_ratio, lgr.compute_d_acc_, t1 - t0);
    } else {
      uint64_t duration_total = 0;
      madi::comm::reduce(&duration_total, &lgr.compute_d_acc_, 1, 0, madi::comm::reduce_op_sum);
      if (lgr.rank_ == 0) {
        double busy_ratio = (double)duration_total / (t1 - t0) / lgr.nproc_ * 100;
        printf("    Busy: %f %% ( %ld ns / %ld ns )\n", busy_ratio, duration_total, (t1 - t0) * lgr.nproc_);
      }
    }
#endif
  }

  static inline void begin_tl(int tid = 0) {
#if DNPLOGGER_ENABLE
    dnplogger& lgr = get_instance_();
    uint64_t t = madi::global_clock::get_time();
    lgr.bp_ = MLOG_BEGIN(&lgr.md_, tid, t);
#endif
  }

  static inline void end_tl(const char* kind, int tid = 0) {
#if DNPLOGGER_ENABLE
    dnplogger& lgr = get_instance_();
    uint64_t t = madi::global_clock::get_time();
    MLOG_END(&lgr.md_, tid, lgr.bp_, &dnplogger_decoder_tl_, t, kind);
#endif
  }

  static inline void* begin_event(int tid = 0) {
#if DNPLOGGER_ENABLE
    dnplogger& lgr = get_instance_();
    uint64_t t = madi::global_clock::get_time();
    void* bp = MLOG_BEGIN(&lgr.md_, tid, t);
    return bp;
#else
    return nullptr;
#endif
  }

  static inline void end_event(void* bp, const char* kind, int tid = 0) {
#if DNPLOGGER_ENABLE
    dnplogger& lgr = get_instance_();
    uint64_t t = madi::global_clock::get_time();
    MLOG_END(&lgr.md_, tid, bp, &dnplogger_decoder_tl_, t, kind);
#endif
  }

  static inline bool initialized() {
    dnplogger& lgr = get_instance_();
    return lgr.initialized_;
  }
};

#endif
