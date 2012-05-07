#ifndef SERVER_UDP_TRANSCODER_H
# define SERVER_UDP_TRANSCODER_H

# include "server-transcoder.h"
# include "transport.h"
# include "packet-queue.h"
# include "sockaddr.h"
# include "server-connection-manager.h"

# include <memory>

class Dispatcher;
class ClientIOChannel;

class ServerUdpTranscoder : public ServerTranscoder {
 public:
  static const int kMaxPacketSize = 8192;
  ServerUdpTranscoder(
      Dispatcher* dispatcher, Transport* transport, const Sockaddr& address,
      ServerConnectionManager* manager);

  bool Start();

 private:
  class Connection : public ServerTranscoder::Connection {
   public:
    Connection(DatagramSenderPacketQueue* queue, Sockaddr* address);

    void Close();
 
    InputCursor* Message();
    InputCursor* Header();

    bool SendMessage(
        ServerConnectedSession* session, EncodeSessionProtector* encoder);

   private:
    DatagramSenderPacketQueue* queue_;
    Sockaddr* address_;

    Buffer buffer_;
    auto_ptr<DatagramSenderPacketQueue::PacketSlot> slot_;
  };

  DatagramChannel::processing_state_e HandleRead();
  void HandleError(
      ServerConnectedSession* session, const ConnectionKey& key,
      ServerTranscoder::Connection* connection,
      ServerConnectedSession::CloseReason error, const char* msg);

  Dispatcher* dispatcher_;
  Transport* transport_;

  ServerConnectionManager* manager_;

  DatagramChannel::event_handler_t client_connect_handler_;

  DatagramSenderPacketQueue queue_;

  const Sockaddr& address_;
  auto_ptr<DatagramChannel> socket_;
};

#endif /* SERVER_UDP_TRANSCODER_H */
