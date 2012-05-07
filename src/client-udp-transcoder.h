#ifndef SIMPLE_UDP_TRANSCODER_H
# define SIMPLE_UDP_TRANSCODER_H

# include "client-transcoder.h"
# include "packet-queue.h"
# include "transport.h"
# include "client-connection-manager.h"

class Dispatcher;
class SessionProtector;
class BoundChannel;
class Sockaddr;

class ClientUdpTranscoder : public ClientTranscoder {
 public:
  static const int kMaxPacketSize = 8192;

  virtual Connection* Connect(
      Transport* transport, ClientConnectionManager* manager,
      const Sockaddr& address);

 private:
  class Connection : public ClientTranscoder::Connection {
   public:
    Connection(
        BoundChannel* channel, ClientConnectionManager* manager);

    const ConnectionKey& GetKey() const;

    void Close();

    InputCursor* Header();
    InputCursor* Message();
    bool SendMessage(
        ClientConnectedSession* session, EncodeSessionProtector* encoder);

   private:
    BoundChannel::processing_state_e HandleRead();
    BoundChannel::processing_state_e HandleWrite();
    void HandleError(
        ClientConnectedSession* session,
        ClientTranscoder::Connection* connection,
        ClientConnectedSession::CloseReason error, const char* message);

    ConnectionKey key_;

    auto_ptr<BoundChannel> socket_;
    ClientConnectionManager* manager_;
    PacketQueue queue_;

    BoundChannel::event_handler_t server_read_handler_;
    BoundChannel::event_handler_t server_write_handler_;

    Buffer buffer_;
  };
};

#endif /* SIMPLE_UDP_TRANSCODER_H */
