#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>
#include <errno.h>

#include "socket-transport.h"

#include "dispatcher.h"
#include "fd-helpers.h"

SocketTransport::SocketTransport(Dispatcher* dispatcher)
    : dispatcher_(dispatcher) {
}

SocketTransport::~SocketTransport() {
}

BoundChannel* SocketTransport::DatagramConnect(const Sockaddr& address) {
  LOG_DEBUG("%s", address.AsString().c_str());

  ScopedFd fd(socket(address.Family(), SOCK_DGRAM, 0));
  if (fd.IsValid()) {
    LOG_PERROR("cannot create socket");
    return NULL;
  }

  if (connect(fd.Get(), address.Data(), address.Size()) != 0) {
    LOG_PERROR("connect failed");
    return NULL;
  }

  return new BoundSocket(dispatcher_, fd.Release());
}

DatagramChannel* SocketTransport::DatagramListenOn(const Sockaddr& address) {
  LOG_DEBUG("%s", address.AsString().c_str());

  ScopedFd fd(socket(address.Family(), SOCK_DGRAM, 0));
  if (fd.IsValid()) {
    LOG_PERROR("cannot create socket");
    return NULL;
  }

  if (bind(fd.Get(), address.Data(), address.Size()) != 0) {
    LOG_PERROR("cannot bind");
    return NULL;
  }

  return new DatagramSocket(dispatcher_, fd.Release());
}

BoundChannel* SocketTransport::StreamConnect(const Sockaddr& address) {
  LOG_DEBUG("%s", address.AsString().c_str());

  ScopedFd fd(socket(address.Family(), SOCK_STREAM, 0));
  if (fd.Get() < 0) {
    LOG_PERROR("cannot create socket");
    return NULL;
  }

  // Make socket non blocking!
 
  int enabled = 1;
  if (setsockopt(
      fd.Get(), SOL_TCP, TCP_NODELAY, &enabled, sizeof(enabled)) < 0) {
    LOG_PERROR("cannot set TCP_NODELAY");
  }

  if (connect(fd.Get(), address.Data(), address.Size()) != 0) {
    LOG_PERROR("connect failed");
    return NULL;
  }

  return new BoundSocket(dispatcher_, fd.Release());
}

AcceptingChannel* SocketTransport::StreamListenOn(const Sockaddr& address) {
  LOG_DEBUG("%s", address.AsString().c_str());

  ScopedFd fd(socket(address.Family(), SOCK_STREAM, 0));
  if (fd.IsValid()) {
    LOG_PERROR("cannot create socket");
    return false;
  }

  // Don't keep lingering data around when we close the socket, close it
  // immediately, violently and for real.
  linger l;
  l.l_onoff = 1;
  l.l_linger = 0;
  if (setsockopt(fd.Get(), SOL_SOCKET, SO_LINGER,
		 reinterpret_cast<char*>(&l), sizeof(l)) < 0) {
    LOG_PERROR("cannot set SO_LINGER");
  }

  // If server crashes and restarts, re-open the socket immediately.
  int reuseaddr = 1;
  if (setsockopt(fd.Get(), SOL_SOCKET, SO_REUSEADDR,
		 reinterpret_cast<char*>(&reuseaddr), sizeof(reuseaddr)) < 0) {
    LOG_PERROR("cannot set SO_REUSEADDR");
  }

  int enabled = 1;
  if (setsockopt(
          fd.Get(), SOL_TCP, TCP_NODELAY, &enabled, sizeof(enabled)) < 0) {
    LOG_PERROR("cannot set TCP_NODELAY");
  }

  // TODO: change size of SNDBUFFER and RECVBUFFER? all those things
  // should happen automagically.

  if (bind(fd.Get(), address.Data(), address.Size()) != 0) {
    LOG_PERROR("cannot bind");
    return false;
  }

  // TODO: 10? tunable parameter!
  if (listen(fd.Get(), 10) != 0) {
    LOG_PERROR("cannot listen");
    return false;
  }

  return new AcceptingSocket(dispatcher_, fd.Release());
}

SocketTransport::Socket::Socket(Dispatcher* dispatcher, int fd)
  : dispatcher_(dispatcher), fd_(fd),
    read_handler_(bind(&SocketTransport::Socket::HandleRead, this)),
    write_handler_(bind(&SocketTransport::Socket::HandleWrite, this)),
    read_callback_(NULL), write_callback_(NULL),
    pending_writes_(0), pending_reads_(0) {
}

SocketTransport::Socket::~Socket() {
  LOG_DEBUG("deleting socket");
  Close();
}

void SocketTransport::Socket::Close() {
  SetFd(-1);
}

BoundChannel* SocketTransport::AcceptingSocket::AcceptConnection(
    Sockaddr** address) {
  LOG_DEBUG();

  sockaddr_storage client;
  socklen_t client_size(sizeof(client));

  int fd;
  while (1) {
    fd = accept(GetFd(), reinterpret_cast<sockaddr*>(&client), &client_size);
    if (fd >= 0)
      break;

    if (errno != EAGAIN || errno != EWOULDBLOCK)
      return NULL;
  }

  BoundSocket* retval = new BoundSocket(dispatcher_, fd);
  if (address) {
    *address = Sockaddr::Parse(
        *reinterpret_cast<sockaddr*>(&client), client_size);
  }

  return retval;
}

void SocketTransport::Socket::SetFd(int fd) {
  LOG_DEBUG("fd %d, pending reads %d, pending writes %d", fd,
	    pending_reads_, pending_writes_);

  if (fd_ >= 0 && fd_ != fd) {
    dispatcher_->DelFd(fd_);
    close(fd_);
  }

  fd_ = fd;
  if (fd < 0)
    return;

  if (pending_reads_)
    dispatcher_->SetFd(fd, Dispatcher::READ, Dispatcher::NONE, &read_handler_, NULL);
  if (pending_writes_)
    dispatcher_->SetFd(fd, Dispatcher::WRITE, Dispatcher::NONE, NULL, &write_handler_);
}

void SocketTransport::Socket::WantRead(const BaseChannel::event_handler_t* callback) {
  LOG_DEBUG("%d", fd_);

  if (callback)
    read_callback_ = callback;
  if (!pending_reads_)
    dispatcher_->SetFd(fd_, Dispatcher::READ, Dispatcher::NONE, &read_handler_, NULL);
  ++pending_reads_;
}

void SocketTransport::Socket::HandleRead() {
  LOG_DEBUG("%d", fd_);

  BaseChannel::processing_state_e status(BaseChannel::DONE);
  if (read_callback_)
    status = (*read_callback_)();

  switch (status) {
    case BaseChannel::MORE:
      LOG_DEBUG("%d handler wants more data", fd_);
      return;

    case BaseChannel::DONE:
      pending_reads_ = 0;

      // for read events, and eventually propagate errors upstreams? won't this cause infinite timeouts?
      dispatcher_->SetFd(fd_, Dispatcher::NONE, Dispatcher::READ, NULL, NULL);
      return;
  }
}

void SocketTransport::Socket::WantWrite(
    const BaseChannel::event_handler_t* callback) {
  LOG_DEBUG("%d", fd_);

  if (callback)
    write_callback_ = callback;
  if (!pending_writes_)
    dispatcher_->SetFd(fd_, Dispatcher::WRITE, Dispatcher::NONE, NULL, &write_handler_);
  ++pending_writes_;
}

void SocketTransport::Socket::HandleWrite() {
  LOG_DEBUG("%d", fd_);

  BaseChannel::processing_state_e status(BaseChannel::DONE);
  if (write_callback_)
    status = (*write_callback_)();

  switch (status) {
    case BaseChannel::MORE:
      LOG_DEBUG("%d handler wants more data", fd_);
      return;

    case BaseChannel::DONE:
      pending_writes_ = 0;
      dispatcher_->SetFd(fd_, Dispatcher::NONE, Dispatcher::WRITE, NULL, NULL);
      return;
  }
}

SocketTransport::Socket::io_result_e SocketTransport::Socket::Write(
    OutputCursor* buffer, const Sockaddr* remote) {
  // Turn content of buffer into iovec.
  OutputCursor::Iovec vect;
  unsigned int iovecsize;
  int bytes = buffer->GetIovec(&vect, &iovecsize);

  // Prepare a msghdr.
  msghdr msg;
  memset(&msg, 0, sizeof(msghdr));
  if (remote) {
    msg.msg_name = const_cast<sockaddr*>(remote->Data());
    msg.msg_namelen = remote->Size();
  }
  msg.msg_iov = vect;
  msg.msg_iovlen = iovecsize;

  // Send message out.
  int written;
  while ((written = sendmsg(fd_, &msg, MSG_CONFIRM | MSG_NOSIGNAL)) < 0) {
    if (errno != EAGAIN) {
      if (errno == EPIPE)
	return CLOSED;
      return ERROR;
    }
  }

  LOG_DEBUG("*** WRITE - written %d bytes of %d prepared", written, bytes);
  buffer->Increment(bytes);
  return OK;
}

SocketTransport::Socket::io_result_e SocketTransport::Socket::Read(
    InputCursor* cursor, Sockaddr** remote) {
  struct sockaddr_storage sockaddr;
  socklen_t socksize = sizeof(sockaddr);

  DEBUG_FATAL_UNLESS(cursor->ContiguousSize())(
      "really want to read 0 bytes? no space in buffer...");

  ssize_t size;
  while (true) {
    // TODO(protocol): use recvmsg, and handle MSG_ERRQUEUE.
    size = recvfrom(
	fd_, cursor->Data(), cursor->ContiguousSize(), 0,
        reinterpret_cast<struct sockaddr*>(&sockaddr), &socksize);
    if (!size) {
      LOG_DEBUG("*** CLOSED fd %d", fd_);
      return CLOSED;
    }

    LOG_DEBUG("*** READ %d bytes from fd %d", size, fd_);
    if (size > 0) {
      cursor->Increment(size);
      break;
    }

    if (errno != EWOULDBLOCK && errno != EAGAIN) {
      LOG_ERROR("recvrom on fd %d returned %s (%d)",
		fd_, strerror(errno), errno);
      return ERROR;
    }
  }
  if (remote)
    *remote = Sockaddr::Parse(*((struct sockaddr*)&sockaddr), socksize);
  return OK;
}
