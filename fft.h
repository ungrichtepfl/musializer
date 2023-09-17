#ifndef FFH_H
#define FFH_H

#include <complex.h>
#include <math.h>
#include <stddef.h>

void dft(float in[], float complex out[], const size_t n);

void idft(float complex in[], float out[], const size_t n);

void fft(float in[], float complex out[], const size_t n);

void ifft(float complex in[], float out[], const size_t n);

#endif // FFH_H
