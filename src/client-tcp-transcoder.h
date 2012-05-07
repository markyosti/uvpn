#ifndef CLIENT_TCP_TRANSCODER_H
# define CLIENT_TCP_TRANSCODER_H

# include "connection-key.h"
# include "client-connection-manager.h"
# include "client-transcoder.h"
# include "packet-queue.h"
# include "transport.h"

class EncodeSessionProtector;
class DecodeSessionProtector;
class BoundChannel;
class Sockaddr;

class ClientTcpTranscoder : public ClientTranscoder {
 public:
  ClientTcpTranscoder();

  virtual Connection* Connect(
      Transport* transport, ClientConnectionManager* manager,
      const Sockaddr& address);

 private:
  class Connection : public ClientTranscoder::Connection {
   public:
    Connection(BoundChannel* channel, ClientConnectionManager* manager);

    const ConnectionKey& GetKey() const;

    void Close();

    InputCursor* Header();
    InputCursor* Message();
    bool SendMessage(
        ClientConnectedSession* session, EncodeSessionProtector* encoder);

   private:
    void HandleError(ClientConnectedSession* session,
		     const ClientConnectedSession::CloseReason error, const char* message);
    BoundChannel::processing_state_e HandleDecodeError(
        ClientConnectedSession* session, DecodeSessionProtector::Result result,
	const char* message);

    const static int kReadSize = 4096;

    BoundChannel::processing_state_e HandleWrite();
    BoundChannel::processing_state_e HandleRead();

    ConnectionKey key_;

    auto_ptr<BoundChannel> channel_;
    ClientConnectedSession* session_;
    ClientConnectionManager* manager_;
    DecodeSessionProtector* decoder_;

    const BoundChannel::event_handler_t read_handler_;
    const BoundChannel::event_handler_t write_handler_;

    Buffer from_user_cleartext_;
    Buffer from_user_encrypted_;
  
    Buffer from_server_encrypted_;
    Buffer from_server_cleartext_;
  };
};

#endif /* CLIENT_TCP_TRANSCODER_H */
