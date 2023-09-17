#ifndef PLUG_H
#define PLUG_H

#include <pthread.h>

// from:
// https://stackoverflow.com/questions/36384195/how-to-correctly-assign-a-pointer-returned-by-dlsym-into-a-variable-of-function
typedef struct {
  // void plug_hello(void);
  void (*plug_hello)(void);
  // void fillSampleBuffer(void *buffer, unsigned int frames);
  void (*fillSampleBuffer)(void *buffer, unsigned int frames);
  // void drawMusic(void);
  void (*drawMusic)(void);
  pthread_mutex_t *BUFFER_LOCK;
  unsigned int *CHANNELS;
  unsigned int *FRAME_BUFFER_SIZE;
} exports_t;

typedef exports_t *PLUG;
#define PLUG_SYM "exports"
// In plug.c:
extern const exports_t exports;

#endif // PLUG_H
