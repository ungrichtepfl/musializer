#include <dlfcn.h>
#include <errno.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>

#include "plug.h"

#define LIBPLUG_FILE_NAME "build/libplug.so"
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

  libplug = dlopen(LIBPLUG_FILE_NAME, RTLD_LAZY);

  if (libplug == NULL) {
    fprintf(stderr, "Could not load %s: %s\n", LIBPLUG_FILE_NAME, dlerror());
    return false;
  }
  plugin = dlsym(libplug, PLUG_SYM);

  if (plugin == NULL) {
    fprintf(stderr, "Could not load %s from %s: %s\n", PLUG_SYM,
            LIBPLUG_FILE_NAME, dlerror());
    return false;
  }

  return true;
}

bool hotReload(void) {
  State *state = plugin->getState();
  if (!unloadPlugin())
    return false;
  if (!loadPlugin())
    return false;
  if (!plugin->resume(state)) {
    fprintf(stderr, "Could not resume plugin\n");
    return false;
  }
  return true;
}

bool PLUG_MODIFIED = false;

bool dynlibModified(void) { return false; }

int main(void) {

  if (!loadPlugin()) {
    return 1;
  }

  plugin->init();

  while (!plugin->finished()) {
    const bool plug_modified = plugin->reload() | dynlibModified();

    if (plug_modified) {
      printf("Plugin file was modified!\n");
      if (!hotReload())
        return 1;
      continue;
    }

    plugin->update();
  }
  plugin->terminate();

  return 0;
}
