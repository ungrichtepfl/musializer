#include "fft.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void dft(float in[], float complex out[], const size_t n) {
  for (size_t k = 0; k < n; ++k) {
    out[k] = 0;
    for (size_t j = 0; j < n; ++j) {
      out[k] += in[j] * cexp(-2.0f * M_PI * j * k / n * I);
    }
  }
}

void idft(float complex in[], float out[], const size_t n) {
  for (size_t k = 0; k < n; ++k) {
    float complex tmp = 0;
    for (size_t j = 0; j < n; ++j) {
      tmp += in[j] * cexp(2.0f * M_PI * j * k / n * I);
    }
    out[k] = tmp / n;
  }
}

static void _fft(float in[], float complex out[], const size_t n,
                 const size_t stride) {
  if (n == 1) {
    // base case f^hat_1 = f_1

    out[0] = in[0];
    return;
  }

  // we put the even in the first half and the odd in the second half of out:
  _fft(&in[0], &out[0], n / 2, 2 * stride);
  _fft(&in[stride], &out[n / 2], n / 2, 2 * stride);

  for (size_t k = 0; k < n / 2; ++k) {
    //  f_hat = [ I_n/2   D_n/2 ] [F_n/2   0   ] [ f_even ]
    //          [ I_n/2  -D_n/2 ] [  0   F_n/2 ] [ f_odd  ]
    //
    // I_n = identity matrix size n
    //
    // w_n = exp(-2*pi*i/n)
    //
    // D_n = [ 1    0      0     ...     0      ]
    //       [ 0   w_2n    0     ...     0      ]
    //       [ 0    0    w^2_2n  ...     0      ]
    //       [ 0    0      0     ...     0      ]
    //       [ ... ...    ...    ...    ...     ]
    //       [ 0    0      0     ... w_2n^(n-1) ]
    //
    const float complex first =
        out[k] + out[n / 2 + k] * cexpf(-2.0f * M_PI * k / n * I);
    const float complex second =
        out[k] - out[n / 2 + k] * cexpf(-2.0f * M_PI * k / n * I);

    out[k] = first;
    out[n / 2 + k] = second;
  }
}

void fft(float in[], float complex out[], const size_t n) {
  assert(ceilf(log2f(n)) == floorf(log2f(n)) && "n must be a power of two!");
  _fft(in, out, n, 1); // X(m) = sum_n=0^N-1 x(n) * exp(-2*pi*n*m/N)
}

static void _ifft(float complex in[], float complex out[], const size_t n,
                  const size_t stride) {
  if (n == 1) {
    // base case f^hat_1 = f_1

    out[0] = in[0];
    return;
  }

  // we put the even in the first half and the odd in the second half of out:
  _ifft(&in[0], &out[0], n / 2, 2 * stride);
  _ifft(&in[stride], &out[n / 2], n / 2, 2 * stride);

  for (size_t k = 0; k < n / 2; ++k) {
    //  f_hat = [ I_n/2   D_n/2 ] [F_n/2   0   ] [ f_even ]
    //          [ I_n/2  -D_n/2 ] [  0   F_n/2 ] [ f_odd  ]
    //
    // I_n = identity matrix size n
    //
    // w_n = exp(2*pi*i/n)
    //
    // D_n = [ 1    0      0     ...     0      ]
    //       [ 0   w_2n    0     ...     0      ]
    //       [ 0    0    w^2_2n  ...     0      ]
    //       [ 0    0      0     ...     0      ]
    //       [ ... ...    ...    ...    ...     ]
    //       [ 0    0      0     ... w_2n^(n-1) ]
    //
    const float complex first =
        out[k] + out[n / 2 + k] * cexpf(2.0f * M_PI * k / n * I);
    const float complex second =
        out[k] - out[n / 2 + k] * cexpf(2.0f * M_PI * k / n * I);

    out[k] = first;
    out[n / 2 + k] = second;
  }
}
void ifft(float complex in[], float out[], const size_t n) {
  assert(ceilf(log2f(n)) == floorf(log2f(n)) && "n must be a power of two!");

  float complex cout[n];
  for (size_t i = 0; i < n; ++i)
    cout[i] = (float complex)out[i];

  // IFFT but not yet normalized by n:
  _ifft(in, cout, n, 1); // x(n) = 1/n * sum_m=0^N-1 X(m) * exp(2*pi*m*n/N)

  for (size_t i = 0; i < n; ++i)
    out[i] = cout[i] / n; // normalize by n
}
