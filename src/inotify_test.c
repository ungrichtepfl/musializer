
#include <dlfcn.h>
#include <errno.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>

#define LIBPLUG_FILE_NAME "build/libplug.so"
#define FILTER_LENGTH 30

bool init_watches(int *fd_, int *wd_, struct pollfd *pfd_) {
  const int fd = inotify_init1(IN_NONBLOCK);
  if (fd == -1) {
    fprintf(stderr, "Could not initialize inotify: %s\n", strerror(errno));
    return false;
  }
  const int wd = inotify_add_watch(fd, LIBPLUG_FILE_NAME, IN_MODIFY);
  if (wd == -1) {
    fprintf(stderr, "Cannot watch '%s': %s\n", LIBPLUG_FILE_NAME,
            strerror(errno));
    return false;
  }
  pfd_->fd = fd;
  pfd_->events = POLLIN;
  *fd_ = fd;
  *wd_ = wd;
  return true;
}

bool checkIfModified(bool *modified, int *fd, int *wd, struct pollfd *pfd,
                     const int filter_length) {
  static int filter_counter = 0;
  static bool modified_raw = false;

  const int poll_num = poll(pfd, 1, 1); // block for 1ms

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
              modified_raw = true;
            }
          }
        }
      }
    }
  }
  if (modified_raw) {
    filter_counter++;
    if (filter_counter >= filter_length) {
      filter_counter = 0;
      *modified = true;
      modified_raw = false;
    } else {
      *modified = false;
    }
  }
  return true;
}

int main(void) {

  int fd = -1;
  int wd = -1;
  struct pollfd pfd = {0};
  if (!init_watches(&fd, &wd, &pfd)) {
    return 1;
  }
  printf("start polling\n");
  int counter = 0;
  while (counter < 2) {
    bool plug_modified = false;
    if (!checkIfModified(&plug_modified, &fd, &wd, &pfd, FILTER_LENGTH)) {
      return 1;
    }
    if (plug_modified) {
      printf("--------------------\n");
      printf("Plugin has been modified\n");
      printf("--------------------\n");
      counter++;
      plug_modified = false;
    }
  }
  printf("Plugin has been modified 2 times.\n");
  close(fd);

  return 0;
}
