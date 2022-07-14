#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cstdint>
#include <unistd.h>
#include <locale.h>
#include <algorithm>

#include <mpi.h>

#include "sched_bench.hpp"

int my_rank = -1;
int n_procs = -1;

int    n_repeats    = 10;
int    n_blocks     = 10;
size_t n_input      = 1000;
size_t leaf_size_ns = 1000000;
size_t n_warmup     = 100;

uint64_t do_work(size_t ns) {
  uint64_t tb = get_time();
  compute_kernel(ns);
  uint64_t te = get_time();
  return te - tb;
}

struct compute_result {
  uint64_t work;
  uint64_t count;
  compute_result() : work(0), count(0) {}
};

compute_result do_pfor(size_t n) {
  compute_result ret;
  for (int i = 0; i < n_blocks; i++) {

    size_t b = ((n + n_procs - 1) / n_procs) * my_rank;
    size_t e = std::min(n, ((n + n_procs - 1) / n_procs) * (my_rank + 1));
    for (size_t i = b; i < e; i++) {
      uint64_t w = do_work(leaf_size_ns);
      ret.work += w;
      ret.count++;
    }

    MPI_Barrier(MPI_COMM_WORLD);
  }
  return ret;
}

void show_help_and_exit(int argc, char** argv) {
  if (my_rank == 0) {
    printf("Usage: %s [options]\n"
           "  options:\n"
           "    -n : Input size (size_t)\n"
           "    -m : Execution time for each task in nanoseconds (size_t)\n"
           "    -b : # of consecutive parallel blocks (int)\n"
           "    -r : # of repeats (int)\n"
           "    -w : # of warmup runs (size_t)\n"
           "    -f : CPU frequency while executing SIMD instructions (double)\n", argv[0]);
  }
  exit(1);
}

int main(int argc, char** argv) {
  MPI_Init(&argc, &argv);

  MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &n_procs);

  int opt;
  while ((opt = getopt(argc, argv, "n:m:b:r:f:w:h")) != EOF) {
    switch (opt) {
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

  if (my_rank == 0) {
    setlocale(LC_NUMERIC, "en_US.UTF-8");
    printf("=============================================================\n"
           "[pfor_mpi]\n"
           "# of processes:                %d\n"
           "N (Input size):                %ld\n"
           "M (Leaf execution time in ns): %ld\n"
           "# of blocks:                   %d\n"
           "# of repeats:                  %d\n"
           "# of warmup runs:              %ld\n"
           "CPU frequency:                 %f GHz\n"
           "# of loop iterations at leaf:  %ld\n"
           "=============================================================\n\n",
           n_procs, n_input, leaf_size_ns, n_blocks, n_repeats,
           n_warmup, cpu_freq, n_iterations_for_simd(leaf_size_ns));
  }

  // warmup
  {
    size_t leaf_size_ns_tmp = leaf_size_ns;
    leaf_size_ns = 10;

    uint64_t t0 = get_time();

    for (int i = 0; i < n_warmup; i++) {
      do_pfor(n_procs * 1024);
    }

    uint64_t t1 = get_time();

    if (my_rank == 0) {
      printf("warmup time: %ld ns\n", t1 - t0);
    }

    leaf_size_ns = leaf_size_ns_tmp;
  }

  for (int it = 0; it < n_repeats; it++) {
    MPI_Barrier(MPI_COMM_WORLD);

    uint64_t t0 = get_time();

    MPI_Barrier(MPI_COMM_WORLD);

    compute_result ret = do_pfor(n_input);

    uint64_t t1 = get_time();

    uint64_t n_tasks;
    MPI_Reduce(&ret.count, &n_tasks, 1, MPI_UINT64_T, MPI_SUM, 0, MPI_COMM_WORLD);

    uint64_t work;
    MPI_Reduce(&ret.work, &work, 1, MPI_UINT64_T, MPI_SUM, 0, MPI_COMM_WORLD);

    uint64_t span;
    MPI_Reduce(&ret.work, &span, 1, MPI_UINT64_T, MPI_MAX, 0, MPI_COMM_WORLD);

    if (my_rank == 0) {
      printf("[%d] %ld ns ( #tasks: %ld work: %ld ns span: %ld ns busy: %f %% )\n",
             it, t1 - t0, n_tasks, work, span,
             (double)work / (t1 - t0) / n_procs * 100);
    }
  }

  MPI_Finalize();
}
