#include "fft.h"
#include <stdio.h>
#include <time.h>

#define P ((size_t)1 << 15)
#define N 8

static inline void print_cvec(float complex sig[], size_t n) {

  for (size_t i = 0; i < n; ++i)
    printf("[%ld] |%.1f + %.1fi| = %.1f\n", i, crealf(sig[i]), cimagf(sig[i]),
           cabsf(sig[i]));
}

static inline void print_fvec(float sig[], size_t n) {
  for (size_t i = 0; i < n; ++i)
    printf("[%ld] %.1f\n", i, fabsf(sig[i]));
}

int main(void) {
  clock_t start, diff;
  int msec;

  float sig[N] = {0};

  for (size_t i = 0; i < N; ++i) {
    float t = (float)i / N; // such that f_k = k / N * f_s = k / N * N = k
    float f1 = 2.0;
    float f2 = 3.0;
    float dc = 3.0;

    sig[i] = dc + sinf(2 * M_PI * f1 * t) + cosf(2 * M_PI * f2 * t);
  }
  float complex freq[N];

  printf("======= DFT =======\n");
  printf("------ Signal Before ------\n");
  print_fvec(sig, N);

  dft(sig, freq, N);

  printf("------ Signal After ------\n");
  print_cvec(freq, N);

  float sig_rev[N] = {0};
  idft(freq, sig_rev, N);

  printf("------ Signal Reversed \n");
  print_fvec(sig_rev, N);

  printf("========= FFT ==========\n");

  printf("------ Signal Before ------\n");
  print_fvec(sig, N);
  printf("------ Signal After ------\n");
  fft(sig, freq, N);
  print_cvec(freq, N);

  printf("\n====== PERFORMANCE ======\n");

  float sig_perf[P] = {0};

  for (size_t i = 0; i < P; ++i) {
    float t = (float)i / P; // such that f_k = k / P * f_s = k / P * P = k
    float f1 = 2.0;
    float f2 = 3.0;
    float dc = 3.0;

    sig_perf[i] = dc + sinf(2 * M_PI * f1 * t) + cosf(2 * M_PI * f2 * t);
  }
  float complex freq_perf[P];
  printf("Signal length: %ld samples\n", P);

  printf("======= FFT =======\n");
  start = clock();
  fft(sig_perf, freq_perf, P);
  diff = clock() - start;
  msec = diff * 1000 / CLOCKS_PER_SEC;
  printf("Time taken %d seconds %d milliseconds\n", msec / 1000, msec % 1000);

  printf("======= DFT =======\n");
  start = clock();
  dft(sig_perf, freq_perf, P);
  diff = clock() - start;
  msec = diff * 1000 / CLOCKS_PER_SEC;
  printf("Time taken %d seconds %d milliseconds\n", msec / 1000, msec % 1000);
}
