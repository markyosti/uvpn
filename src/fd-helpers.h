#ifndef FD_HELPERS_H
# define FD_HELPERS_H

# include "base.h"

# include <unistd.h>
# include <errno.h>
# include <string>

inline int fd_read_once(int fd, char* buffer, int bsize) {
  int r;
  while (1) {
    r = read(fd, buffer, bsize);
    if (r >= 0)
      break;

    if (errno != EAGAIN) {
      return -1;
    }
  }

  return r;
}

inline bool fd_write(int fd, const char* buffer, int size) {
  int towrite(size);
  while (towrite > 0) {
    int written = write(fd, buffer, towrite);
    if (written < 0) {
      if (errno == EAGAIN)
	continue;
      return false;
    }

    buffer += written;
    towrite -= written;
  }

  return true;
}

inline bool fd_write(int fd, const string& buffer) {
  return fd_write(fd, buffer.c_str(), buffer.length());
}

inline bool fd_write(int fd, const char* str) {
  return fd_write(fd, str, strlen(str));
}


#endif /* FD_HELPERS_H */
