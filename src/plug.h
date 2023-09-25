#ifndef PLUG_H
#define PLUG_H

#include <stdbool.h>

typedef struct State State;

// from:
// https://stackoverflow.com/questions/36384195/how-to-correctly-assign-a-pointer-returned-by-dlsym-into-a-variable-of-function
typedef struct {
  // void update(void);
  bool (*init)(void);
  void (*update)(void);
  State *(*getState)(void);
  bool (*reload)(void);
  void (*pause)(void);
  bool (*resume)(State *);
  bool (*finished)(void);
  void (*terminate)(void);
} exports_t;

typedef exports_t PLUG;
#define PLUG_SYM "exports"
// In plug.c:
extern const exports_t exports;

#endif // PLUG_H
