#include <cblas.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <stdint.h>

static inline uint64_t get_ns() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000 + (uint64_t)ts.tv_nsec;
}

int main() {
  int N = 8000;
  int iter = 10;

  double *A = (double*)malloc(sizeof(double) * N * N);
  double *B = (double*)malloc(sizeof(double) * N * N);
  double *C = (double*)malloc(sizeof(double) * N * N);
  for (size_t i = 0; i < N * N; i++) {
    A[i] = 1.0;
    B[i] = 1.0;
    C[i] = 0.0;
  }

  for (int it = 0; it < iter; it++) {
    uint64_t t1 = get_ns();
    cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
                N, N, N, 1.0, A, N, B, N, 1.0, C, N);
    uint64_t t2 = get_ns();
    printf("%d : %ld ns\n", it, t2 - t1);
  }

  /* for (int i = 0; i < N; i++) { */
  /*   for (int j = 0; j < N; j++) { */
  /*     printf("%f ", C[i * N + j]); */
  /*   } */
  /*   printf("\n"); */
  /* } */

  return 0;
}
