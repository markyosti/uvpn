#ifndef SOCKET_TRANSPORT_H
# define SOCKET_TRANSPORT_H

# include "transport.h"
# include "dispatcher.h"
# include "macros.h"

class SocketTransport
    : public Transport {
 public:
  SocketTransport(Dispatcher* dispatcher);
  ~SocketTransport();

  virtual BoundChannel* DatagramConnect(const Sockaddr& address);
  virtual DatagramChannel* DatagramListenOn(const Sockaddr& address);

  virtual BoundChannel* StreamConnect(const Sockaddr& address);
  virtual AcceptingChannel* StreamListenOn(const Sockaddr& address);

 private:
  class Socket
      : virtual public ListeningChannel,
        virtual public ConnectingChannel,
        virtual public ReadChannel,
        virtual public WriteChannel,
        virtual public AcceptingChannel {
   public:
    Socket(Dispatcher* dispatcher, int type);
    virtual ~Socket();

    void WantRead(const BaseChannel::event_handler_t* callback);
    void WantWrite(const BaseChannel::event_handler_t* callback);
    void WantConnection(const BaseChannel::event_handler_t* callback) {
      WantRead(callback);
    }

    void Close();

    virtual bool ListenOn(const Sockaddr& address);
    virtual bool ConnectTo(const Sockaddr& address);

    BoundChannel* AcceptConnection(Sockaddr** address);

   protected:
    io_result_e Write(OutputCursor* buffer, const Sockaddr* remote);
    io_result_e Read(InputCursor* buffer, Sockaddr** remote);

    void SetFd(int fd);
    int fd_;
    int type_;

   private:
    void HandleRead();
    void HandleWrite();

    Dispatcher* dispatcher_;

    Dispatcher::event_handler_t read_handler_;
    Dispatcher::event_handler_t write_handler_;

    const DatagramChannel::event_handler_t* read_callback_;
    const DatagramChannel::event_handler_t* write_callback_;
    
    int pending_writes_;
    int pending_reads_;
  };

  class DatagramSocket : virtual public DatagramChannel, public Socket {
   public:
    DatagramSocket(Dispatcher* dispatcher) : Socket(dispatcher, SOCK_DGRAM) {}
    virtual ~DatagramSocket() {}

    virtual bool ListenOn(const Sockaddr& address);

    virtual io_result_e Write(OutputCursor* buffer, const Sockaddr& remote) {
      return Socket::Write(buffer, &remote);
    }

    virtual io_result_e Read(InputCursor* buffer, Sockaddr** remote) {
      return Socket::Read(buffer, remote);
    }
  };

  class BoundSocket : public Socket, virtual public BoundChannel {
   public:
    BoundSocket(Dispatcher* dispatcher, int type)
	: Socket(dispatcher, type) {
    }

    virtual io_result_e Read(InputCursor* buffer) {
      return Socket::Read(buffer, NULL);
    }

    virtual io_result_e Write(OutputCursor* buffer) {
      return Socket::Write(buffer, NULL);
    }
  };

  Dispatcher* dispatcher_;

  NO_COPY(SocketTransport);
};

#endif /* SOCKET_TRANSPORT_H */
