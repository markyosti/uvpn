#ifndef TERMINAL_H
# define TERMINAL_H

# include "password.h"
# include "base.h"

# include <termios.h>
# include <string>

class Terminal {
 public:
  Terminal();
  ~Terminal();

  bool Print(const char* message);

  // Buffer is allocated by ReadPassword. Caller must free it with
  // "delete". Caller is expected to eventually shred the content of
  // the buffer.
  bool ReadPassword(const char* message, StringBuffer* buffer);
  bool ReadLine(string* message);
  int ReadLine(char* buffer, int size);

 private:
  class State {
   public:
    State(int fd);
    ~State();

    const struct termios& Get() { return tty_state_; }
    bool SetPasswordReadMode();

   private:
    int tty_fd_;
    struct termios tty_state_;
  };

  static const char kTtyName[];
  static const int kBufferSize = 4096;

  int GetFd() { return tty_fd_; }
  bool SetPasswordReadMode();

  int tty_fd_;
};

#endif /* TERMINAL_H */
