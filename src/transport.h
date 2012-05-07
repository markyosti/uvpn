#ifndef TRANSPORT_H
# define TRANSPORT_H

# include "base.h"
# include "buffer.h"
# include "sockaddr.h"

# include <string>
# include <memory>

// IDEA:
//   - Socket: contains some stuff that represents the connection itself.
//
// Can a single handler be shared among multiple sockets? Yes, it should be sharable.
// If it can be shared, where does it keep socket data? it should be passed some data
// along with each socket.
//
class Channel;
class Sockaddr;

// Interface definitions.

class BaseChannel {
 public:
  virtual ~BaseChannel() {}
  virtual void Close() = 0;

  enum processing_state_e {
    DONE,   // This callback is done reading / writing to the channel.
    MORE    // This callback needs to read / write more data.
  };

  enum io_result_e {
    OK,     // Operation completed successfully.
    CLOSED, // File descriptor seems to be closed now.
    ERROR   // There was an error completing the operation.
  };

  typedef function<processing_state_e ()> event_handler_t;
};

class WriteChannel : virtual public BaseChannel {
 public:
  virtual void WantWrite(const event_handler_t* callback) = 0;
};

class ReadChannel : virtual public BaseChannel {
 public:
  virtual void WantRead(const event_handler_t* callback) = 0;
};

class BoundWriteChannel : virtual public WriteChannel {
 public:
  // Tries to write buffer->ContiguousSize() bytes in one go, or as many
  // bytes as possible, depending on the underlying implementation.
  virtual io_result_e Write(OutputCursor* buffer) = 0;
};

class BoundReadChannel : virtual public ReadChannel {
 public:
  // Note that when reading, the maximum size of the packet is determined
  // by buffer->ContiguousSize(). Call Reserve before invoking this function.
  virtual io_result_e Read(InputCursor* buffer) = 0;
};

class BoundChannel : 
    virtual public BaseChannel,
    virtual public BoundReadChannel,
    virtual public BoundWriteChannel {
};

class AcceptingChannel : virtual public BaseChannel {
 public:
  virtual void WantConnection(const event_handler_t* handler) = 0;
  virtual BoundChannel* AcceptConnection(Sockaddr** address) = 0;
};

class DatagramReadChannel : virtual public ReadChannel {
 public:
  // Note that when reading, the maximum size of the packet is determined
  // by buffer->ContiguousSize(). Call Reserve before invoking this function.
  virtual io_result_e Read(InputCursor* buffer, Sockaddr** remote) = 0;
};

class DatagramWriteChannel : virtual public WriteChannel {
 public:
  // Sockaddr must be filled with the address of the remote end.
  virtual io_result_e Write(OutputCursor* buffer, const Sockaddr& remote) = 0;
};

class DatagramChannel
    : virtual public BaseChannel,
      virtual public DatagramWriteChannel,
      virtual public DatagramReadChannel {
};

class Transport {
 public:
  virtual BoundChannel* DatagramConnect(const Sockaddr& address) = 0;
  virtual DatagramChannel* DatagramListenOn(const Sockaddr& address) = 0;

  virtual AcceptingChannel* StreamListenOn(const Sockaddr& address) = 0;
  virtual BoundChannel* StreamConnect(const Sockaddr& address) = 0;
};

// Utility classes / functions.

// Used to turn a DatagramChannel into a BoundChannel.
class BoundDatagramWriteChannel : public BoundWriteChannel {
 public:
  // Takes ownership of the Sockaddr pointer!
  BoundDatagramWriteChannel(Sockaddr* remote, DatagramChannel* channel)
      : remote_(remote), channel_(channel) {}
  virtual ~BoundDatagramWriteChannel() {}
  void Close() {}

  io_result_e Write(OutputCursor* buffer) { return channel_->Write(buffer, *remote_); }

  void WantWrite(const event_handler_t* callback) {
    return channel_->WantWrite(callback);
  }

 private:
  auto_ptr<Sockaddr> remote_;
  DatagramChannel* channel_;
};

#endif /* TRANSPORT_H */
