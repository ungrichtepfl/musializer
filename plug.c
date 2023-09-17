#include "plug.h"
#include <stdio.h>

void plug_hello(void) { printf("Hello from plugin!\n"); }

const exports_t exports = {plug_hello};
