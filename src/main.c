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
#define LIBPLUG_OBJECT_FILE_NAME "build/plug.o"
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
  plugin->pause();
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

bool init_watches(int *fd_, int *wd_, struct pollfd *pfd_) {
  const int fd = inotify_init1(IN_NONBLOCK);
  if (fd == -1) {
    fprintf(stderr, "Could not initialize inotify: %s\n", strerror(errno));
    return false;
  }
  const int wd =
      inotify_add_watch(fd, LIBPLUG_OBJECT_FILE_NAME, IN_MODIFY | IN_IGNORED);
  if (wd == -1) {
    fprintf(stderr, "Cannot watch '%s': %s\n", LIBPLUG_OBJECT_FILE_NAME,
            strerror(errno));
    return false;
  }
  pfd_->fd = fd;
  pfd_->events = POLLIN;
  *fd_ = fd;
  *wd_ = wd;
  return true;
}

bool dynlibModified(bool *modified, int *fd, int *wd, struct pollfd *pfd) {

  const int poll_num = poll(pfd, 1, 1); // block for 1ms
  bool modified_internal = false;

  if (poll_num == -1) {
    if (errno != EINTR) {
      fprintf(stderr, "poll\n");
      return false;
    }
  }

  if (poll_num > 0) {
    printf("Poll number > 0\n");
    if (pfd->revents & POLLIN) {
      printf("Pollin event\n");
      /* Some systems cannot read integer variables if they are not
        properly aligned. On other systems, incorrect alignment may
        decrease performance. Hence, the buffer used for reading from
        the inotify file descriptor should have the same alignment as
        struct inotify_event. */
      char buf[4096]
          __attribute__((aligned(__alignof__(struct inotify_event))));
      ssize_t len;
      len = read(*fd, buf, sizeof(buf));
      if (len == -1 && errno != EAGAIN) {
        fprintf(stderr, "Could not read from file descriptor");
        return false;
      }

      if (len > 0) {
        printf("Data length: %zd\n", len);
        // Data available
        const struct inotify_event *event;
        for (char *ptr = buf; ptr < buf + len;
             ptr += sizeof(struct inotify_event) + event->len) {
          event = (const struct inotify_event *)ptr;

          printf("Iterating event mask %u\n", event->mask);

          if (event->mask & IN_IGNORED) {
            printf("Ignored\n");
            modified_internal = true;
            if (close(*fd) == -1) {
              fprintf(stderr, "close\n");
              return false;
            }
            if (!init_watches(fd, wd, pfd)) {
              return false;
            }
          }
          if (event->mask & IN_MODIFY) {
            if (*wd == event->wd) {
              printf("File modified\n");
              modified_internal = true;
            }
          }
        }
      }
    }
  }
  if (modified_internal) {
    *modified = true;
    usleep(200000); // Wait until file has been written
  } else {
    *modified = false;
  }
  return true;
}

int main(void) {

  if (!loadPlugin()) {
    return 1;
  }

  int fd = -1;
  int wd = -1;
  struct pollfd pfd = {0};
  if (!init_watches(&fd, &wd, &pfd)) {
    return 1;
  }

  plugin->init();

  while (!plugin->finished()) {
    bool dynlib_modified = false;
    if (!dynlibModified(&dynlib_modified, &fd, &wd, &pfd)) {
      return 1;
    }
    const bool plug_modified = plugin->reload() | dynlib_modified;

    if (plug_modified) {
      printf("Plugin file was modified!\n");
      if (!hotReload())
        return 1;
      continue;
    }

    plugin->update();
  }
  plugin->terminate();
  close(fd);

  return 0;
}
