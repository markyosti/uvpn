#ifndef TUN_TAP_IO_CHANNEL_H
# define TUN_TAP_IO_CHANNEL_H

# include <string>
# include "base.h"
# include "dispatcher.h"

class InputCursor;
class OutputCursor;

class TunTapDevice {
 public:
  TunTapDevice();
  ~TunTapDevice();

  // type can be: IFF_TUN, or IFF_TAP.
  bool Open(int type);
  void Close();

  bool Read(InputCursor* cursor, int maxsize);
  bool Write(OutputCursor* cursor);

  int Fd() const { return fd_; }
  const string& Name() const { return device_; }

 private:
  int fd_;
  string device_;
};

#endif /* TUN_TAP_IO_CHANNEL_H */
