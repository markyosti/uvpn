#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>
#include <errno.h>

#include "socket-transport.h"

#include "dispatcher.h"

SocketTransport::SocketTransport(Dispatcher* dispatcher)
    : dispatcher_(dispatcher) {
}

SocketTransport::~SocketTransport() {
}

BoundChannel* SocketTransport::DatagramConnect(const Sockaddr& address) {
  LOG_DEBUG("%s", address.AsString().c_str());

  BoundSocket* socket(new BoundSocket(dispatcher_, SOCK_DGRAM));
  socket->ConnectTo(address);
  return socket;
}

DatagramChannel* SocketTransport::DatagramListenOn(const Sockaddr& address) {
  LOG_DEBUG("%s", address.AsString().c_str());

  DatagramChannel* socket(new DatagramSocket(dispatcher_));
  socket->ListenOn(address);
  return socket;
}

bool SocketTransport::DatagramSocket::ListenOn(const Sockaddr& address) {
  // TODO: tcp & udp socket should share the code here.

  int fd = socket(address.Family(), type_, 0);
  if (fd < 0) {
    LOG_FATAL("cannot create socket, %s", strerror(errno));
    // TODO: what the hell do we do? retry?
  }

  if (bind(fd, address.Data(), address.Size()) != 0) {
    LOG_FATAL("cannot bind socket, %s", strerror(errno));
    // TODO: what the hell do we do here?
  }

  SetFd(fd);
  return true;
}

BoundChannel* SocketTransport::StreamConnect(const Sockaddr& address) {
  BoundChannel* socket(new BoundSocket(dispatcher_, SOCK_STREAM));
  socket->ConnectTo(address);
  return socket;
}

AcceptingChannel* SocketTransport::StreamListenOn(const Sockaddr& address) {
  AcceptingChannel* socket(new Socket(dispatcher_, SOCK_STREAM));
  socket->ListenOn(address);
  return socket;
}

SocketTransport::Socket::Socket(Dispatcher* dispatcher, int type)
  : fd_(-1), type_(type), dispatcher_(dispatcher),
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

BoundChannel* SocketTransport::Socket::AcceptConnection(Sockaddr** address) {
  LOG_DEBUG();

  sockaddr_storage client;
  socklen_t client_size(sizeof(client));

  int fd;
  while (1) {
    fd = accept(fd_, reinterpret_cast<sockaddr*>(&client), &client_size);
    if (fd >= 0)
      break;

    if (errno != EAGAIN || errno != EWOULDBLOCK)
      return NULL;
  }

  BoundSocket* retval = new BoundSocket(dispatcher_, type_);
  retval->SetFd(fd);

  if (address) {
    *address = Sockaddr::Parse(
        *reinterpret_cast<sockaddr*>(&client), client_size);
  }

  return retval;
}

bool SocketTransport::Socket::ConnectTo(const Sockaddr& address) {
  LOG_DEBUG("%s", address.AsString().c_str());

  // TODO: for tcp sockets, we need to be non blocking!
  //       read the connect man page, and check SO_ERROR!

  int fd = socket(address.Family(), type_, 0);
  if (fd < 0) {
    LOG_DEBUG("cannot create socket, %s", strerror(errno));
    // TODO: what the hell do we do? retry?
  }
  if (connect(fd, address.Data(), address.Size()) != 0) {
    LOG_DEBUG("cannot connect socket, %s", strerror(errno));
    // TODO: what the hell do we do here?
  }

  int enabled = 1;
  if (setsockopt(fd, SOL_TCP, TCP_NODELAY, &enabled, sizeof(enabled)) < 0) {
    LOG_ERROR("cannot set TCP_NODELAY");
  }

  SetFd(fd);
  return true;
}

bool SocketTransport::Socket::ListenOn(const Sockaddr& address) {
  LOG_DEBUG("%s", address.AsString().c_str());

  int fd = socket(address.Family(), type_, 0);
  if (fd < 0) {
    LOG_FATAL("cannot create socket, %s", strerror(errno));
    // TODO: what the hell do we do? retry?
  }

  if (bind(fd, address.Data(), address.Size()) != 0) {
    LOG_FATAL("cannot bind socket, %s", strerror(errno));
    // TODO: what the hell do we do here?
  }

  // TODO: 10? tunable parameter!
  if (listen(fd, 10) != 0) {
    LOG_FATAL("cannot listen, %s", strerror(errno));
    // TODO: handle error!
  }

  // Don't keep lingering data around when we close the socket, close it
  // immediately, violently and for real.
  linger l;
  l.l_onoff = 1;
  l.l_linger = 0;
  if (setsockopt(fd, SOL_SOCKET, SO_LINGER,
		 reinterpret_cast<char*>(&l), sizeof(l)) < 0) {
    LOG_ERROR("cannot set SO_LINGER");
    // TODO: handle error!
  }

  // If server crashes and restarts, re-open the socket immediately.
  int reuseaddr = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
		 reinterpret_cast<char*>(&reuseaddr), sizeof(reuseaddr)) < 0) {
    LOG_ERROR("cannot set SO_REUSEADDR");
  }

  int enabled = 1;
  if (setsockopt(fd, SOL_TCP, TCP_NODELAY, &enabled, sizeof(enabled)) < 0) {
    LOG_ERROR("cannot set TCP_NODELAY");
  }

  // TODO: disable nagle? change size of SNDBUFFER and RECVBUFFER?
  // all those things should happen automagically.

  SetFd(fd);
  return true;
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
