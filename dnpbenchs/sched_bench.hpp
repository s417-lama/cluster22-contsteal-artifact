#pragma once
#ifndef SCHED_BENCH_HPP_
#define SCHED_BENCH_HPP_

#include <cstdio>
#include <cstdint>
#include <ctime>
#include <unistd.h>

#if __AVX__ || __AVX2__
#include <immintrin.h>
#define SIMD_REGS 16
#elif __ARM_FEATURE_SVE
#include <arm_sve.h>
#define SIMD_REGS 32
#else
#define SIMD_REGS 1
#endif

namespace {

double cpu_freq = 3.0;

inline uint64_t get_time() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000 + (uint64_t)ts.tv_nsec;
}

inline size_t n_iterations_for_simd(uint64_t leaf_size_ns) {
  return leaf_size_ns * 2 * cpu_freq / SIMD_REGS;
}

inline void compute_kernel(size_t ns) {
#if __AVX2__
  __m256d A[SIMD_REGS];

  for (int i = 0; i < SIMD_REGS; i++) {
    A[i] = _mm256_set_pd(1.0f, 1.0f, 1.0f, 1.0f);
  }

  size_t n_iter = n_iterations_for_simd(ns);
  for (size_t it = 0; it < n_iter; it++) {
    for (int i = 0; i < SIMD_REGS; i++) {
      A[i] = _mm256_fmadd_pd(A[i], A[i], A[i]);
    }
  }

  for (int i = 0; i < SIMD_REGS; i++) {
    if (*(double*)(A + i) == 0) {
      printf("error\n");
    }
  }
#elif __ARM_FEATURE_SVE
  svfloat64_t A0  = svdup_f64(1.0f);
  svfloat64_t A1  = svdup_f64(1.0f);
  svfloat64_t A2  = svdup_f64(1.0f);
  svfloat64_t A3  = svdup_f64(1.0f);
  svfloat64_t A4  = svdup_f64(1.0f);
  svfloat64_t A5  = svdup_f64(1.0f);
  svfloat64_t A6  = svdup_f64(1.0f);
  svfloat64_t A7  = svdup_f64(1.0f);
  svfloat64_t A8  = svdup_f64(1.0f);
  svfloat64_t A9  = svdup_f64(1.0f);
  svfloat64_t A10 = svdup_f64(1.0f);
  svfloat64_t A11 = svdup_f64(1.0f);
  svfloat64_t A12 = svdup_f64(1.0f);
  svfloat64_t A13 = svdup_f64(1.0f);
  svfloat64_t A14 = svdup_f64(1.0f);
  svfloat64_t A15 = svdup_f64(1.0f);
  svfloat64_t A16 = svdup_f64(1.0f);
  svfloat64_t A17 = svdup_f64(1.0f);
  svfloat64_t A18 = svdup_f64(1.0f);
  svfloat64_t A19 = svdup_f64(1.0f);
  svfloat64_t A20 = svdup_f64(1.0f);
  svfloat64_t A21 = svdup_f64(1.0f);
  svfloat64_t A22 = svdup_f64(1.0f);
  svfloat64_t A23 = svdup_f64(1.0f);
  svfloat64_t A24 = svdup_f64(1.0f);
  svfloat64_t A25 = svdup_f64(1.0f);
  svfloat64_t A26 = svdup_f64(1.0f);
  svfloat64_t A27 = svdup_f64(1.0f);
  svfloat64_t A28 = svdup_f64(1.0f);
  svfloat64_t A29 = svdup_f64(1.0f);
  svfloat64_t A30 = svdup_f64(1.0f);
  svfloat64_t A31 = svdup_f64(1.0f);
  svbool_t pd = svptrue_b64();

  size_t n_iter = ns * 2 * cpu_freq / SIMD_REGS;
  for (size_t it = 0; it < n_iter; it++) {
    A0  = svmla_f64_x(pd, A0 , A0 , A0 );
    A1  = svmla_f64_x(pd, A1 , A1 , A1 );
    A2  = svmla_f64_x(pd, A2 , A2 , A2 );
    A3  = svmla_f64_x(pd, A3 , A3 , A3 );
    A4  = svmla_f64_x(pd, A4 , A4 , A4 );
    A5  = svmla_f64_x(pd, A5 , A5 , A5 );
    A6  = svmla_f64_x(pd, A6 , A6 , A6 );
    A7  = svmla_f64_x(pd, A7 , A7 , A7 );
    A8  = svmla_f64_x(pd, A8 , A8 , A8 );
    A9  = svmla_f64_x(pd, A9 , A9 , A9 );
    A10 = svmla_f64_x(pd, A10, A10, A10);
    A11 = svmla_f64_x(pd, A11, A11, A11);
    A12 = svmla_f64_x(pd, A12, A12, A12);
    A13 = svmla_f64_x(pd, A13, A13, A13);
    A14 = svmla_f64_x(pd, A14, A14, A14);
    A15 = svmla_f64_x(pd, A15, A15, A15);
    A16 = svmla_f64_x(pd, A16, A16, A16);
    A17 = svmla_f64_x(pd, A17, A17, A17);
    A18 = svmla_f64_x(pd, A18, A18, A18);
    A19 = svmla_f64_x(pd, A19, A19, A19);
    A20 = svmla_f64_x(pd, A20, A20, A20);
    A21 = svmla_f64_x(pd, A21, A21, A21);
    A22 = svmla_f64_x(pd, A22, A22, A22);
    A23 = svmla_f64_x(pd, A23, A23, A23);
    A24 = svmla_f64_x(pd, A24, A24, A24);
    A25 = svmla_f64_x(pd, A25, A25, A25);
    A26 = svmla_f64_x(pd, A26, A26, A26);
    A27 = svmla_f64_x(pd, A27, A27, A27);
    A28 = svmla_f64_x(pd, A28, A28, A28);
    A29 = svmla_f64_x(pd, A29, A29, A29);
    A30 = svmla_f64_x(pd, A30, A30, A30);
    A31 = svmla_f64_x(pd, A31, A31, A31);
  }

  if (svlasta_f64(pd, A0 ) == 0) { printf("error\n"); }
  if (svlasta_f64(pd, A1 ) == 0) { printf("error\n"); }
  if (svlasta_f64(pd, A2 ) == 0) { printf("error\n"); }
  if (svlasta_f64(pd, A3 ) == 0) { printf("error\n"); }
  if (svlasta_f64(pd, A4 ) == 0) { printf("error\n"); }
  if (svlasta_f64(pd, A5 ) == 0) { printf("error\n"); }
  if (svlasta_f64(pd, A6 ) == 0) { printf("error\n"); }
  if (svlasta_f64(pd, A7 ) == 0) { printf("error\n"); }
  if (svlasta_f64(pd, A8 ) == 0) { printf("error\n"); }
  if (svlasta_f64(pd, A9 ) == 0) { printf("error\n"); }
  if (svlasta_f64(pd, A10) == 0) { printf("error\n"); }
  if (svlasta_f64(pd, A11) == 0) { printf("error\n"); }
  if (svlasta_f64(pd, A12) == 0) { printf("error\n"); }
  if (svlasta_f64(pd, A13) == 0) { printf("error\n"); }
  if (svlasta_f64(pd, A14) == 0) { printf("error\n"); }
  if (svlasta_f64(pd, A15) == 0) { printf("error\n"); }
  if (svlasta_f64(pd, A16) == 0) { printf("error\n"); }
  if (svlasta_f64(pd, A17) == 0) { printf("error\n"); }
  if (svlasta_f64(pd, A18) == 0) { printf("error\n"); }
  if (svlasta_f64(pd, A19) == 0) { printf("error\n"); }
  if (svlasta_f64(pd, A20) == 0) { printf("error\n"); }
  if (svlasta_f64(pd, A21) == 0) { printf("error\n"); }
  if (svlasta_f64(pd, A22) == 0) { printf("error\n"); }
  if (svlasta_f64(pd, A23) == 0) { printf("error\n"); }
  if (svlasta_f64(pd, A24) == 0) { printf("error\n"); }
  if (svlasta_f64(pd, A25) == 0) { printf("error\n"); }
  if (svlasta_f64(pd, A26) == 0) { printf("error\n"); }
  if (svlasta_f64(pd, A27) == 0) { printf("error\n"); }
  if (svlasta_f64(pd, A28) == 0) { printf("error\n"); }
  if (svlasta_f64(pd, A29) == 0) { printf("error\n"); }
  if (svlasta_f64(pd, A30) == 0) { printf("error\n"); }
  if (svlasta_f64(pd, A31) == 0) { printf("error\n"); }
#else
  uint64_t t0 = get_time();
  uint64_t t1 = get_time();
  while (t1 - t0 < ns) {
    t1 = get_time();
  }
#endif
}

}

#endif
