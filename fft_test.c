#include "fft.h"
#include <stdio.h>

#define N 12

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
  float sig[N] = {0};

  for (int i = 0; i < N; ++i) {
    float t = (float)i / N; // such that f_k = k / N * f_s = k / N * N = k
    float f1 = 2.0;
    float f2 = 5.0;
    float dc = 3.0;

    sig[i] = dc + sinf(2 * M_PI * f1 * t) + cosf(2 * M_PI * f2 * t);
  }

  printf("------ Signal Before ------\n");
  print_fvec(sig, N);

  float complex freq[N];
  dft(sig, freq, N);

  printf("------- DFT -------\n");
  print_cvec(freq, N);

  idft(freq, sig, N);

  printf("------ Signal After \n");
  print_fvec(sig, N);
}
