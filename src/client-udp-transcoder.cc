#include "client-udp-transcoder.h"
#include "transport.h"
#include "client-authenticator.h"
#include "serializers.h"

ClientTranscoder::Connection* ClientUdpTranscoder::Connect(
    Transport* transport, ClientConnectionManager* manager,
    const Sockaddr& address) {
  BoundChannel* socket = transport->DatagramConnect(address);
  if (!socket) {
    LOG_ERROR("could not datagram connect");
    return NULL;
  }

  return new Connection(socket, manager);
}

ClientUdpTranscoder::Connection::Connection(
    BoundChannel* socket, ClientConnectionManager* manager)
    : key_(this),
      socket_(socket),
      manager_(manager),
      server_read_handler_(bind(&ClientUdpTranscoder::Connection::HandleRead, this)),
      server_write_handler_(bind(&ClientUdpTranscoder::Connection::HandleWrite, this)) {
  socket_->WantRead(&server_read_handler_);
}

void ClientUdpTranscoder::Connection::HandleError(
    ClientConnectedSession* session,
    ClientTranscoder::Connection* connection,
    ClientConnectedSession::CloseReason error, const char* message) {
  LOG_ERROR("session error: %s", message);
  if (session)
    session->HandleError(key_, connection, error);
  else
    manager_->HandleError(key_, connection, error);
}

const ConnectionKey& ClientUdpTranscoder::Connection::GetKey() const {
  return key_;
}


BoundChannel::processing_state_e ClientUdpTranscoder::Connection::HandleWrite() {
  LOG_DEBUG();

  // TODO: if an input packet is too large, we create large udp packets that
  // end up fragmented in the network. This is bad for PMTU and DF related
  // issues. We should try to keep packets below a certain size as much as
  // possible. To merge packets together, we need some sort of header.
  // We don't want to keep the header in clear, so... we need a cipher that
  // gives us a (small) guaranteed size for headers.
  socket_->Write(queue_.ToSend()->Output());
  queue_.Sent();

  if (!queue_.Pending())
    return DatagramWriteChannel::DONE;
  return DatagramChannel::MORE;
}

BoundChannel::processing_state_e
    ClientUdpTranscoder::Connection::HandleRead() {
  LOG_DEBUG();

  Buffer packet;
  packet.Input()->Reserve(kMaxPacketSize);

  if (socket_->Read(packet.Input()) != BoundChannel::OK) {
    // TODO: handle errors
    LOG_PERROR("udp socket is having troubles receiving data");
    return DatagramChannel::MORE;
  }

  ClientConnectedSession* session;
  ClientConnectedSession::State state(
      manager_->GetSession(key_, packet.Output(), &session));
  if (state == ClientConnectedSession::NeedNewSession) {
    session = manager_->CreateSession(key_, packet.Output(), this);
    if (!session) {
      HandleError(session, this, ClientConnectedSession::Manager,
		  "cannot create session");
      return BoundChannel::MORE;
    }
  }

  if (state == ClientConnectedSession::NeedNewSession ||
      state == ClientConnectedSession::Ready)
    state = session->IsReady(key_, packet.Output());

  if (state != ClientConnectedSession::Ready) {
    HandleError(
        session, this, ClientConnectedSession::Manager,
	"could not determine session");
    return DatagramChannel::MORE;
  }

  DecodeSessionProtector* decoder(session->GetDecoder());
  Buffer decoded;
  DecodeSessionProtector::Result result;
  result = decoder->Decode(packet.Output(), decoded.Input());
  if (result != DecodeSessionProtector::SUCCEEDED) {
    HandleError(session, this, ClientConnectedSession::Truncated,
                "decoding failed - truncated packet?");
    return DatagramChannel::MORE;
  }

  // TODO: same as above, handle partial packets. This means that we might
  // need to keep a queue of incoming packets, and try to decode them in
  // sequence. Unless we keep some headers in the clear, that's going to be
  // hard to handle.
  
  session->HandlePacket(key_, this, decoded.Output());
  return DatagramChannel::MORE;
}

InputCursor* ClientUdpTranscoder::Connection::Message() {
  return buffer_.Input();
}

InputCursor* ClientUdpTranscoder::Connection::Header() {
  return queue_.ToQueue();
}

void ClientUdpTranscoder::Connection::Close() {
  // TODO: close socket.
  // TODO: who deletes this object?
}

bool ClientUdpTranscoder::Connection::SendMessage(
    ClientConnectedSession* session, EncodeSessionProtector* encoder) {
  LOG_DEBUG();

  if (encoder) {
    if (!encoder->Encode(buffer_.Output(), queue_.ToQueue())) {
      LOG_ERROR("encoding failed");
      // TODO: handle errors!
      return false;
    }
  } else {
    EncodeToBuffer(buffer_.Output(), queue_.ToQueue());
  }

  queue_.Queued();
  socket_->WantWrite(&server_write_handler_);
  return true;
}
