#include "fft.h"

void dft(float in[], float complex out[], size_t n) {
  for (size_t k = 0; k < n; ++k) {
    out[k] = 0;
    for (size_t j = 0; j < n; ++j) {
      out[k] += in[j] * cexp(-2.0f * M_PI * j * k / n * I);
    }
  }
}

void idft(float complex in[], float out[], size_t n) {
  for (size_t k = 0; k < n; ++k) {
    float complex tmp = 0;
    for (size_t j = 0; j < n; ++j) {
      tmp += in[j] * cexp(2.0f * M_PI * j * k / n * I);
    }
    out[k] = tmp / n;
  }
}
