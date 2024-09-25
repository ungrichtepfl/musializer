#ifndef MUSIALIZER_H
#define MUSIALIZER_H

#include <stdbool.h>

typedef struct State State;

// from:
// https://stackoverflow.com/questions/36384195/how-to-correctly-assign-a-pointer-returned-by-dlsym-into-a-variable-of-function
typedef struct {
  bool (*init)(void);
  void (*update)(void);
  State *(*getState)(void);
  bool (*reload)(void);
  void (*pause)(void);
  bool (*resume)(State *);
  bool (*finished)(void);
  void (*terminate)(void);
} exports_t;

typedef exports_t MUSIALIZER;
#define MUSIALIZER_SYM "exports"
// In musializer.c:
extern const exports_t exports;

#endif // MUSIALIZER_H
