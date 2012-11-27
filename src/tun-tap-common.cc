#include "tun-tap-common.h"
#include "errors.h"
#include "buffer.h"

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/if_tun.h>
#include <linux/if.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

TunTapDevice::TunTapDevice() : fd_(-1) {
}

TunTapDevice::~TunTapDevice() {
  if (fd_ > 0)
    Close();
}

void TunTapDevice::Close() {
  LOG_DEBUG("closing fd %d", fd_);
  close(fd_);
}

bool TunTapDevice::Open(int type) {
  // TODO(SECURITY): set close on exec!
  fd_ = open("/dev/net/tun", O_RDWR);
  if (fd_ < 0) {
    LOG_ERROR("could not open /dev/net/tun");
    // TODO: error!
    return false;
  }

  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  // Just change IFF_TUN in IFF_TAP for a tap device.
  ifr.ifr_flags = static_cast<short int>(type | IFF_NO_PI);

  int status = ioctl(fd_, TUNSETIFF, &ifr);
  if (status < 0) {
    LOG_ERROR("could not ioctl fd %d", fd_);
    close(fd_);
    // TODO: error!
    return false;
  }

  device_.assign(ifr.ifr_name);
  LOG_DEBUG("tun tap device is %s", device_.c_str());
  return true;
}

bool TunTapDevice::Read(InputCursor* input, int max_size) {
  input->Reserve(max_size);

  int size;
  while (1) {
    LOG_DEBUG("receiving from fd %d, %08x, %d", fd_,
	      (unsigned int)input->Data(), input->ContiguousSize());
//    size = recv(fd_, input->Data(), input->ContiguousSize(), 0);
    size = read(fd_, input->Data(), input->ContiguousSize());
    if (size >= 0) {
      input->Increment(size);
      LOG_DEBUG("read from fd %d, %d bytes", fd_, size);
      break;
    }

    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      LOG_PERROR("recv error: %s", strerror(errno));
      // TODO: handle errors!
      return false;
    }
  }

  return true;
}

bool TunTapDevice::Write(OutputCursor* cursor) {
  OutputCursor::Iovec vect;
  unsigned int iovecsize;
  int towrite(cursor->GetIovec(&vect, &iovecsize));

  msghdr message;
  memset(&message, 0, sizeof(message));
  message.msg_iov = vect;
  message.msg_iovlen = iovecsize;

  int sent;
  while (1) {
    sent = writev(fd_, vect, iovecsize);
    if (sent >= 0) {
      LOG_DEBUG("write to fd %d, %d bytes", fd_, sent);
      if (sent < towrite)
        LOG_DEBUG("partial write, should not happen.");
      cursor->Increment(towrite);
      return true;
    }

    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      LOG_PERROR("recv error");
      // TODO: handle errors!
      return false;
    }
  }

  return false;
}
