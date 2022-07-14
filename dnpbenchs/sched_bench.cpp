#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <unistd.h>

#include "uth.h"

#include "dnppattern.hpp"
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

int my_rank = -1;
int n_procs = -1;

std::string computation = "pfor";

int    n_repeats    = 10;
int    n_blocks     = 10;
size_t n_input      = 1000;
size_t leaf_size_ns = 1000000;
size_t n_warmup     = 100;

dnpret<void> do_work(size_t ns) {
  dnppattern dnpp;
  dnpp.do_serial([=] {
    dnplogger::begin_tl();
    compute_kernel(ns);
    dnplogger::end_tl("Compute");
  });
  return dnpp.ret();
}

dnpret<void> do_pfor(size_t n) {
  dnppattern dnpp;
  for (int i = 0; i < n_blocks; i++) {
    dnpp.parallel_for(std::size_t(0), n, [=](size_t i) {
      return do_work(leaf_size_ns);
    });
  }
  return dnpp.ret();
}

dnpret<void> do_rrm(size_t n) {
  dnppattern dnpp;
  if (n == 0) {
    // do nothing
    return dnpp.ret();
  } else if (n == 1) {
    dnpp.invoke([=] { return do_work(leaf_size_ns); });
    return dnpp.ret();
  } else {
    for (int k = 0; k < n_blocks; k++) {
      dnpp.parallel_for(std::size_t(0), n, [=](size_t i) {
        return do_work(leaf_size_ns);
      });
    }
    dnpp.parallel_invoke(
      [=] { return do_rrm(n / 2); },
      [=] { return do_rrm(n / 2); }
    );
    return dnpp.ret();
  }
}

dnpret<void> do_lcs(size_t n) {
  dnppattern dnpp;
  if (n == 0) {
    // do nothing
    return dnpp.ret();
  } else if (n == 1) {
    dnpp.invoke([=] { return do_work(leaf_size_ns); });
    return dnpp.ret();
  } else {
    dnpp.invoke([=] { return do_lcs(n / 2); });
    dnpp.parallel_invoke(
      [=] { return do_lcs(n / 2); },
      [=] { return do_lcs(n / 2); }
    );
    dnpp.invoke([=] { return do_lcs(n / 2); });
    return dnpp.ret();
  }
}

dnpret<void> do_mm(size_t n) {
  dnppattern dnpp;
  if (n == 0) {
    // do nothing
    return dnpp.ret();
  } else if (n == 1) {
    dnpp.invoke([=] { return do_work(leaf_size_ns); });
    return dnpp.ret();
  } else {
    dnpp.parallel_for(0, 4, [=](int i) {
      return do_mm(n / 2);
    });
    dnpp.parallel_for(0, 4, [=](int i) {
      return do_mm(n / 2);
    });
    return dnpp.ret();
  }
}

template <class F, class... Args>
dnpret<void> run_root(F fn, Args... args) {
  uth::thread< dnpret<void> > f(fn, args...);
  return f.join();
}

void show_help_and_exit(int argc, char** argv) {
  if (my_rank == 0) {
    printf("Usage: %s [options]\n"
           "  options:\n"
           "    -c : Computation type (string: 'pfor', 'rrm', 'lcs', 'mm')\n"
           "    -n : Input size (size_t)\n"
           "    -m : Execution time for each leaf task in nanoseconds (size_t)\n"
           "    -b : [pfor, rrm] # of consecutive parallel for blocks (int)\n"
           "    -r : # of repeats (int)\n"
           "    -w : # of warmup runs (size_t)\n"
           "    -f : CPU frequency while executing SIMD instructions (double)\n", argv[0]);
  }
  exit(1);
}

void real_main(int argc, char** argv) {
  my_rank = uth::get_pid();
  n_procs = uth::get_n_procs();

  int opt;
  while ((opt = getopt(argc, argv, "c:n:m:b:r:f:w:h")) != EOF) {
    switch (opt) {
      case 'c':
        computation = optarg;
        break;
      case 'n':
        n_input = atoll(optarg);
        break;
      case 'm':
        leaf_size_ns = atoll(optarg);
        break;
      case 'b':
        n_blocks = atoi(optarg);
        break;
      case 'r':
        n_repeats = atoi(optarg);
        break;
      case 'f':
        cpu_freq = atof(optarg);
        break;
      case 'w':
        n_warmup = atol(optarg);
        break;
      case 'h':
      default:
        show_help_and_exit(argc, argv);
    }
  }

  dnpret<void> (*computation_fn)(size_t);
  if (computation == "pfor") {
    computation_fn = do_pfor;
  } else if (computation == "rrm") {
    computation_fn = do_rrm;
  } else if (computation == "lcs") {
    computation_fn = do_lcs;
  } else if (computation == "mm") {
    computation_fn = do_mm;
  } else {
    printf("'%s' is an invalid computation type.\n", computation.c_str());
    show_help_and_exit(argc, argv);
  }

  if (my_rank == 0) {
    setlocale(LC_NUMERIC, "en_US.UTF-8");
    printf("=============================================================\n"
           "[%s]\n"
           "# of processes:                %d\n"
           "N (Input size):                %ld\n"
           "M (Leaf execution time in ns): %ld\n"
           "# of blocks:                   %d\n"
           "# of repeats:                  %d\n"
           "# of warmup runs:              %ld\n"
           "CPU frequency:                 %f GHz\n"
           "# of loop iterations at leaf:  %ld\n"
           "-------------------------------------------------------------\n",
           computation.c_str(), n_procs, n_input, leaf_size_ns, n_blocks, n_repeats,
           n_warmup, cpu_freq, n_iterations_for_simd(leaf_size_ns));
    printf("uth options:\n");
    uth::print_options(stdout);
    printf("=============================================================\n\n");
  }

  dnplogger::init(my_rank, n_procs, logger_n_threads);

  // warmup
  {
    size_t leaf_size_ns_tmp = leaf_size_ns;
    leaf_size_ns = 10;

    uint64_t t0 = madi::global_clock::get_time();

    for (int i = 0; i < n_warmup; i++) {
      if (my_rank == 0) {
        run_root(do_pfor, n_procs * 1024);
      }

      uth::barrier();
    }

    uint64_t t1 = madi::global_clock::get_time();

    if (my_rank == 0) {
      printf("warmup time: %ld ns\n", t1 - t0);
    }

    madi::logger::clear();
    dnplogger::clear();

    leaf_size_ns = leaf_size_ns_tmp;
  }

  for (int it = 0; it < n_repeats; it++) {
    uth::barrier();

    dnppattern dnpp;
    uint64_t t0 = madi::global_clock::get_time();

    if (my_rank == 0) {
      dnpp = run_root(computation_fn, n_input);
    }

    uint64_t t1 = madi::global_clock::get_time();

    uth::barrier();

    if (my_rank == 0) {
      /* printf("[%d] %ld ns ( #tasks: %ld ideal: %ld ns work: %ld ns span: %ld ns busy (ideal): %f %% busy (measured): %f %% )\n", */
      /*        it, t1 - t0, */
      /*        dnpp.n_tasks, dnpp.n_tasks * leaf_size_ns / n_procs, */
      /*        dnpp.work, dnpp.span, */
      /*        (double)dnpp.n_tasks * leaf_size_ns / (t1 - t0) / n_procs * 100, */
      /*        (double)dnpp.work / (t1 - t0) / n_procs * 100); */
      printf("[%d] %ld ns ( #tasks: %ld work: %ld ns span: %ld ns busy: %f %% )\n",
             it, t1 - t0, dnpp.n_tasks, dnpp.work, dnpp.span,
             (double)dnpp.work / (t1 - t0) / n_procs * 100);
      fflush(stdout);
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
}

int main(int argc, char** argv) {
  uth::start(real_main, argc, argv);
  return 0;
}
