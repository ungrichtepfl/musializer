#include <dlfcn.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "plug.h"
#include <assert.h>
#include <pthread.h>
#include <raylib.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

PLUG *plugin = NULL;
void *libplug = NULL;
bool unloadPlugin(void) {
  if (dlclose(libplug) != 0) {
    fprintf(stderr, "Could not unload plugin: %s\n", dlerror());
    return false;
  }
  return true;
}
bool loadPlugin(void) {

  const char *libplug_file_name = "build/libplug.so";

  libplug = dlopen(libplug_file_name, RTLD_LAZY);

  if (libplug == NULL) {
    fprintf(stderr, "Could not load %s: %s\n", libplug_file_name, dlerror());
    return false;
  }
  plugin = dlsym(libplug, PLUG_SYM);

  if (plugin == NULL) {
    fprintf(stderr, "Could not load %s from %s: %s\n", PLUG_SYM,
            libplug_file_name, dlerror());
    return false;
  }

  return true;
}

int main(void) {

  if (!loadPlugin()) {
    return 1;
  }

  plugin->init();

  while (!plugin->finished()) {

    if (plugin->reload()) {
      printf("Hot reloading plugin\n");
      State *state = plugin->getState();
      if (!unloadPlugin())
        return 1;
      if (!loadPlugin())
        return 1;
      if (!plugin->resume(state)) {
        fprintf(stderr, "Could not resume plugin\n");
        return 1;
      }
      continue;
    }
    plugin->update();
  }
  plugin->terminate();

  return 0;
}
