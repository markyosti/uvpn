#ifndef SERVER_TCP_TRANSCODER_H
# define SERVER_TCP_TRANSCODER_H

# include "connection-key.h"
# include "server-connection-manager.h"
# include "server-transcoder.h"
# include "packet-queue.h"
# include "transport.h"
# include "sockaddr.h"

# include <memory>

class ClientIOChannel;
class ServerConnectionManager;

class ServerTcpTranscoder : public ServerTranscoder {
 public:
  static const int kMaxPacketSize = 8192;
  ServerTcpTranscoder(
      Transport* transport, const Sockaddr& address,
      ServerConnectionManager* manager);

  bool Start();

 private:
  class Connection : public ServerTranscoder::Connection {
   public:
    Connection(BoundChannel* channel, ServerConnectionManager* manager);

    void Close();
 
    InputCursor* Message();
    InputCursor* Header();

    bool SendMessage(
        ServerConnectedSession* session, EncodeSessionProtector* encoder);

   private:
    void HandleError(ServerConnectedSession* session,
		     ServerConnectedSession::CloseReason error, const char* message);
    BoundChannel::processing_state_e HandleDecodeError(
        ServerConnectedSession* session, DecodeSessionProtector::Result result,
	const char* message);

    const static int kReadSize = 4096;

    BoundChannel::processing_state_e HandleWrite();
    BoundChannel::processing_state_e HandleRead();

    ConnectionKey key_;

    auto_ptr<BoundChannel> channel_;
    ServerConnectedSession* session_;
    ServerConnectionManager* manager_;
    DecodeSessionProtector* decoder_;

    const BoundChannel::event_handler_t read_handler_;
    const BoundChannel::event_handler_t write_handler_;

    // Encrypted data read from the tcp socket, coming from remote uvpn client.
    Buffer from_client_encrypted_;
    // Cleartext data read from the tcp socket, ready to be sent to the tunnel. 
    Buffer from_client_cleartext_;

    // Cleartext data read from the tunnel, coming from the user, that needs to
    // be sent to remote uvpn client.
    Buffer from_tunnel_cleartext_;
    // Encrypted data read from the tunnel, ready to be sent to remote uvpn client.
    Buffer from_tunnel_encrypted_;
  };

  AcceptingChannel::processing_state_e HandleConnection();

  Transport* transport_;
  ServerConnectionManager* manager_;

  // Invoked by transport every time there is a new connection.
  AcceptingChannel::event_handler_t client_connect_handler_;

  const Sockaddr& address_;
  auto_ptr<AcceptingChannel> socket_;
};

#endif /* SERVER_TCP_TRANSCODER_H */
