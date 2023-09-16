#ifndef FFH_H
#define FFH_H

#include <complex.h>
#include <math.h>
#include <stddef.h>

void dft(float in[], float complex out[], size_t n);

void idft(float complex in[], float out[], size_t n);

#endif // FFH_H
