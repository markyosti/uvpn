#include "terminal.h"
#include "errors.h"
#include "password.h"
#include "fd-helpers.h"

#include <errno.h>
// tcgetattr, tcsetattr, ...
#include <termios.h>
// open
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
// read
#include <unistd.h>

const char Terminal::kTtyName[] = "/dev/tty";

Terminal::State::State(int fd) : tty_fd_(fd) {
  if (tcgetattr(tty_fd_, &tty_state_))
    LOG_ERROR("tcgetattr failed for %s", kTtyName);
}

Terminal::State::~State() {
  if (tcsetattr(tty_fd_, TCSANOW, &tty_state_) != 0)
    LOG_PERROR("tcsetattr failed to restore original parameters for %s", kTtyName);
}

bool Terminal::State::SetPasswordReadMode() {
  struct termios new_state(tty_state_);

  new_state.c_iflag &= ~IGNCR;
  new_state.c_iflag |= ICRNL;
  new_state.c_oflag |= OPOST | ONLCR;
  new_state.c_lflag &= ~ECHO;
  new_state.c_lflag |= ICANON | ECHONL;

  if (tcsetattr(tty_fd_, TCSANOW, &new_state) != 0) {
    LOG_PERROR("tcsetattr failed for %s", kTtyName);
    return false;
  }

  return true;
}

Terminal::Terminal() {
  tty_fd_ = open(kTtyName, O_RDWR);
  if (tty_fd_ < 0)
    return;
}

Terminal::~Terminal() {
  close(tty_fd_);
}

bool Terminal::Print(const char* message) {
  return fd_write(GetFd(), message);
}

// TODO: this is horrible, we should have a better API to read a line
// from terminal.
int Terminal::ReadLine(char* buffer, int size) {
  int r = fd_read_once(GetFd(), buffer, size - 1);
  if (r < 0)
    return -1;

  if (r >= 1 && buffer[r - 1] == '\n')
    r -= 1;

  buffer[r] = '\0';
  return r;
}

bool Terminal::ReadLine(string* line) {
  char buffer[kBufferSize];
  int r = ReadLine(buffer, kBufferSize - 1);
  if (r < 0) {
    LOG_PERROR("reading from terminal failed (%s)", kTtyName);
    return false;
  }

  line->assign(buffer, r);
  return true;
}

bool Terminal::ReadPassword(const char* message, StringBuffer* buffer) {
  State state(GetFd());

  if (!state.SetPasswordReadMode())
    return false;

  if (!Print(message))
    LOG_PERROR("writing to terminal failed (%s)", kTtyName);

  int r = ReadLine(buffer->Data(), buffer->Size());
  if (r < 0) {
    LOG_PERROR("reading from terminal failed (%s)", kTtyName);
    return false;
  }

  buffer->Resize(r);
  return true;
}
