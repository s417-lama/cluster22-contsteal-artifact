#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <unistd.h>
#include <random>

#include "uth.h"

#include "dnplogger.hpp"

#include "sched_bench.hpp"

namespace uth = madm::uth;

#ifndef DNP_LOGGER_PRINT_STAT
#define DNP_LOGGER_PRINT_STAT 0
#endif
constexpr size_t logger_print_stat = DNP_LOGGER_PRINT_STAT;
#undef DNP_LOGGER_PRINT_STAT

#ifndef DNP_LOGGER_N_THREADS
#define DNP_LOGGER_N_THREADS 1
#endif
constexpr int logger_n_threads = DNP_LOGGER_N_THREADS;
#undef DNP_LOGGER_N_THREADS

#ifndef DNP_LCS_CUTOFF
#define DNP_LCS_CUTOFF 128
#endif
constexpr size_t cutoff_n = DNP_LCS_CUTOFF;
#undef DNP_LCS_CUTOFF

#ifndef DNP_LCS_SEED
#define DNP_LCS_SEED 0
#endif
constexpr size_t seed = DNP_LCS_SEED;
#undef DNP_LCS_SEED

int my_rank = -1;
int n_procs = -1;

size_t n_input      = 1024;
int    n_repeats    = 10;
size_t n_warmup     = 10;
int    enable_check = 1;
int    serial_exec  = 0;

using elem_t = char;
using result_t = uint32_t;

/*
 *                   n
 *         -------------------- ^
 *         |                 x| |
 *         |                 x| |
 *         |                 x| |
 *         |                 x| | right
 *         |                 x| |
 *         |                 x| |
 * lb --> x|xxxxxxxxxxxxxxxxxx| |
 *         -------------------- v
 *         <------ down ------>
 */

struct partial_result_t {
  result_t lb;
  result_t right[cutoff_n];
  result_t down[cutoff_n];
};

enum lcs_dep_t {
  VERT = 0,
  HORZ,
  DIAG,
};

template <class T>
struct lcs_future_t {
  madm::uth::thread<T, 3> th;

  lcs_future_t() {}

  template <class F>
  lcs_future_t(F func) : th(func) {}

  template <lcs_dep_t DEP>
  T get() {
    return th.join(DEP);
  }

  template <lcs_dep_t DEP>
  void discard() {
    return th.discard(DEP);
  }
};

struct inner_ret_t {
  union {
    struct {
      lcs_future_t<inner_ret_t> f_tr; // top right
      lcs_future_t<inner_ret_t> f_bl; // bottom left
      lcs_future_t<inner_ret_t> f_br; // bottom right
    } inner;
    struct {
      lcs_future_t<partial_result_t> f_tr;
      lcs_future_t<partial_result_t> f_bl;
      lcs_future_t<partial_result_t> f_br;
    } leaf;
  };
  inner_ret_t() {};
};

elem_t* g_X;
elem_t* g_Y;

result_t l_buf[cutoff_n + 1];
partial_result_t init_pr;

/*
 * d  v
 *  \ |
 *   \|
 * h--x
 */

inline result_t lcs_elem(size_t i, size_t j, result_t v, result_t h, result_t d) {
  if (i == 0 || j == 0) {
    return 0;
  } else if (g_X[i] == g_Y[j]) {
    return d + 1;
  } else {
    return std::max(v, h);
  }
}

partial_result_t lcs_kernel(size_t i,
                            size_t j,
                            const result_t* v,
                            const result_t* h,
                            const result_t d,
                            result_t* buf,
                            size_t n) {
  dnplogger::begin_tl();

  partial_result_t ret;

  buf[0] = d;
  for (size_t jj = 0; jj < n; jj++) {
    buf[jj + 1] = v[jj];
  }

  for (size_t ii = 0; ii < n; ii++) {
    result_t left = h[ii];
    for (size_t jj = 0; jj < n; jj++) {
      result_t tmp = lcs_elem(i + ii, j + jj, buf[jj + 1], left, buf[jj]);
      buf[jj] = left;
      left = tmp;
    }
    buf[n] = left;
    ret.right[ii] = left;
  }

  ret.lb = buf[0];
  for (size_t jj = 0; jj < n; jj++) {
    ret.down[jj] = buf[jj + 1];
  }

  dnplogger::end_tl("Compute");

  return ret;
}

result_t lcs_serial(result_t* buf, size_t n) {
  for (size_t j = 0; j < n + 1; j++) {
    buf[j] = 0;
  }

  for (size_t i = 0; i < n; i++) {
    result_t left = 0;
    for (size_t j = 0; j < n; j++) {
      result_t tmp = lcs_elem(i, j, buf[j + 1], left, buf[j]);
      buf[j] = left;
      left = tmp;
    }
    buf[n] = left;
  }

  return buf[n];
}

partial_result_t lcs_leaf(size_t i,
                          size_t j,
                          lcs_future_t<partial_result_t> f_v,
                          lcs_future_t<partial_result_t> f_h,
                          size_t n) {
  /* printf("[leaf] i: %ld j: %ld n: %ld\n", i, j, n); */
  partial_result_t pr_v = i == 0 ? init_pr : f_v.get<VERT>();
  partial_result_t pr_h = j == 0 ? init_pr : f_h.get<HORZ>();

  result_t* next_v = pr_v.down;
  result_t* next_h = pr_h.right;
  result_t  next_d = pr_v.lb;

  return lcs_kernel(i, j, next_v, next_h, next_d, l_buf, n);
}

inner_ret_t lcs_rec(size_t i,
                    size_t j,
                    lcs_future_t<inner_ret_t> f_v,
                    lcs_future_t<inner_ret_t> f_h,
                    size_t n) {
  /* printf("[rec] i: %ld j: %ld n: %ld\n", i, j, n); */
  inner_ret_t v = i == 0 ? inner_ret_t{} : f_v.get<VERT>();
  inner_ret_t h = j == 0 ? inner_ret_t{} : f_h.get<HORZ>();

  if (n / 2 <= cutoff_n) {
    auto v_bl = i == 0 ? lcs_future_t<partial_result_t>() : v.leaf.f_bl;
    auto v_br = i == 0 ? lcs_future_t<partial_result_t>() : v.leaf.f_br;
    auto h_tr = j == 0 ? lcs_future_t<partial_result_t>() : h.leaf.f_tr;
    auto h_br = j == 0 ? lcs_future_t<partial_result_t>() : h.leaf.f_br;

    lcs_future_t<partial_result_t> tl([=] { return lcs_leaf(i        , j        , v_bl, h_tr, n / 2); });
    lcs_future_t<partial_result_t> tr([=] { return lcs_leaf(i        , j + n / 2, v_br, tl  , n / 2); });
    lcs_future_t<partial_result_t> bl([=] { return lcs_leaf(i + n / 2, j        , tl  , h_br, n / 2); });
    lcs_future_t<partial_result_t> br([=] { return lcs_leaf(i + n / 2, j + n / 2, tr  , bl  , n / 2); });

    tl.get<DIAG>();
    tr.discard<DIAG>();
    bl.discard<DIAG>();
    br.discard<DIAG>();

    inner_ret_t ret;
    ret.leaf = {
      .f_tr = tr,
      .f_bl = bl,
      .f_br = br,
    };
    return ret;
  } else {
    auto v_bl = i == 0 ? lcs_future_t<inner_ret_t>() : v.inner.f_bl;
    auto v_br = i == 0 ? lcs_future_t<inner_ret_t>() : v.inner.f_br;
    auto h_tr = j == 0 ? lcs_future_t<inner_ret_t>() : h.inner.f_tr;
    auto h_br = j == 0 ? lcs_future_t<inner_ret_t>() : h.inner.f_br;

    lcs_future_t<inner_ret_t> tl([=] { return lcs_rec(i        , j        , v_bl, h_tr, n / 2); });
    lcs_future_t<inner_ret_t> tr([=] { return lcs_rec(i        , j + n / 2, v_br, tl  , n / 2); });
    lcs_future_t<inner_ret_t> bl([=] { return lcs_rec(i + n / 2, j        , tl  , h_br, n / 2); });
    lcs_future_t<inner_ret_t> br([=] { return lcs_rec(i + n / 2, j + n / 2, tr  , bl  , n / 2); });

    tl.get<DIAG>();
    tr.discard<DIAG>();
    bl.discard<DIAG>();
    br.discard<DIAG>();

    inner_ret_t ret;
    ret.inner = {
      .f_tr = tr,
      .f_bl = bl,
      .f_br = br,
    };
    return ret;
  }
}

result_t lcs_consume(inner_ret_t ret, size_t n) {
  if (n / 2 <= cutoff_n) {
    partial_result_t pr = ret.leaf.f_br.get<VERT>();
    return pr.right[n / 2 - 1];
  } else {
    return lcs_consume(ret.inner.f_br.get<VERT>(), n / 2);
  }
}

result_t lcs_main(size_t n) {
  auto empty = lcs_future_t<inner_ret_t>();
  inner_ret_t ret = lcs_rec(0, 0, empty, empty, n);
  return lcs_consume(ret, n);
}

template <class F, class... Args>
result_t run_root(F fn, Args... args) {
  uth::thread<result_t> f(fn, args...);
  return f.join();
}

elem_t gen_random_elem() {
  static std::default_random_engine engine(seed);

  constexpr char chars[] = "abcdefghijklmnopqrstuvwxyz";
  static std::uniform_int_distribution<> dist(0, strlen(chars) - 1);

  return chars[dist(engine)];
}

void gen_random_array(elem_t* a, size_t n) {
  for (size_t i = 0; i < n; i++) {
    a[i] = gen_random_elem();
  }
}

void show_help_and_exit(int argc, char** argv) {
  if (my_rank == 0) {
    printf("Usage: %s [options]\n"
           "  options:\n"
           "    -n : Input size (size_t)\n"
           "    -r : # of repeats (int)\n"
           "    -w : # of warmup runs (size_t)\n"
           "    -c : check the result (int)\n"
           "    -s : serial execution (int)\n", argv[0]);
  }
  exit(1);
}

void real_main(int argc, char** argv) {
  my_rank = uth::get_pid();
  n_procs = uth::get_n_procs();

  int opt;
  while ((opt = getopt(argc, argv, "n:r:w:c:s:h")) != EOF) {
    switch (opt) {
      case 'n':
        n_input = atoll(optarg);
        break;
      case 'r':
        n_repeats = atoi(optarg);
        break;
      case 'w':
        n_warmup = atol(optarg);
        break;
      case 'c':
        enable_check = atoi(optarg);
        break;
      case 's':
        serial_exec = atoi(optarg);
        break;
      case 'h':
      default:
        show_help_and_exit(argc, argv);
    }
  }

  if (n_input & (n_input - 1)) {
    if (my_rank == 0) {
      printf("Input size (-n) must be 2^i but %ld was given.\n", n_input);
    }
    exit(1);
  }

  if (my_rank == 0) {
    setlocale(LC_NUMERIC, "en_US.UTF-8");
    printf("=============================================================\n"
           "[Longest Common Subsequence (LCS)]\n"
           "# of processes:                %d\n"
           "N (Input size):                %ld\n"
           "# of repeats:                  %d\n"
           "# of warmup runs:              %ld\n"
           "Check enabled:                 %d\n"
           "Serial execution:              %d\n"
           "Cutoff:                        %ld\n"
           "-------------------------------------------------------------\n",
           n_procs, n_input, n_repeats, n_warmup, enable_check, serial_exec, cutoff_n);
    printf("uth options:\n");
    uth::print_options(stdout);
    printf("=============================================================\n\n");

    fflush(stdout);
  }

  dnplogger::init(my_rank, n_procs, logger_n_threads);

  for (size_t i = 0; i < cutoff_n; i++) {
    init_pr.lb       = 0;
    init_pr.right[i] = 0;
    init_pr.down[i]  = 0;
  }

  g_X = (elem_t*)malloc(sizeof(elem_t) * (n_input + 1));
  g_Y = (elem_t*)malloc(sizeof(elem_t) * (n_input + 1));
  gen_random_array(g_X, n_input);
  gen_random_array(g_Y, n_input);
  g_X[n_input] = '\0';
  g_Y[n_input] = '\0';

  result_t* buf_seq = NULL;

  result_t ans = 0;
  if (enable_check && my_rank == 0) {
    /* printf("X: %s\n", g_X); */
    /* printf("Y: %s\n", g_Y); */
    printf("Calculating the answer sequentially...\n");

    buf_seq = (result_t*)malloc(sizeof(result_t) * (n_input + 1));
    ans = lcs_serial(buf_seq, n_input);
    free(buf_seq);

    printf("Answer: %d\n", ans);
  }

  if (serial_exec) {
    buf_seq = (result_t*)malloc(sizeof(result_t) * (n_input + 1));
  }

  for (size_t it = 0; it < n_repeats + n_warmup; it++) {
    uth::barrier();

    result_t ret = 0;
    uint64_t t0 = madi::global_clock::get_time();

    if (my_rank == 0) {
      if (serial_exec) {
        ret = lcs_serial(buf_seq, n_input);
      } else {
        ret = run_root(lcs_main, n_input);
      }
    }

    uint64_t t1 = madi::global_clock::get_time();

    uth::barrier();

    madm::uth::discard_all_futures();

    if (my_rank == 0) {
      printf("[%ld] %ld ns ( result: %d )\n",
             it, t1 - t0, ret);
      fflush(stdout);
    }

    if (enable_check && my_rank == 0) {
      if (ans != ret) {
        printf("Wrong answer: %d is correct but %d was returned.\n", ans, ret);
        exit(1);
      }
    }

    madi::comm::broadcast(&t0, 1, 0);
    madi::comm::broadcast(&t1, 1, 0);

    if (logger_print_stat) {
      madi::logger::flush_and_print_stat(t0, t1);
      dnplogger::flush_and_print_stat(t0, t1);
    } else {
      madi::logger::flush(t0, t1);
      dnplogger::flush(t0, t1);
    }

    uth::barrier();
  }

  if (serial_exec) {
    free(buf_seq);
  }
}

int main(int argc, char** argv) {
  uth::start(real_main, argc, argv);
  return 0;
}
