#include "server-udp-transcoder.h"
#include "server-connection-manager.h"
#include "server-authenticator.h"
#include "serializers.h"

ServerUdpTranscoder::ServerUdpTranscoder(
    Dispatcher* dispatcher, Transport* transport, const Sockaddr& address,
    ServerConnectionManager* manager)
    : dispatcher_(dispatcher),
      transport_(transport),
      manager_(manager),
      client_connect_handler_(bind(&ServerUdpTranscoder::HandleRead, this)),
      address_(address) {
}

void ServerUdpTranscoder::Start() {
  LOG_DEBUG("server-udp-transcoder, %s", address_.AsString().c_str());

  socket_.reset(transport_->DatagramListenOn(address_));
  queue_.SetChannel(socket_.get());
  socket_->WantRead(&client_connect_handler_);
}

void ServerUdpTranscoder::HandleError(
    ServerConnectedSession* session, const ConnectionKey& key,
    ServerTranscoder::Connection* connection,
    ServerConnectedSession::CloseReason error, const char* message) {
  LOG_ERROR("session error: %s", message);
  if (session)
    session->HandleError(key, connection, error);
  else
    manager_->HandleError(key, connection, error);
}

DatagramChannel::processing_state_e ServerUdpTranscoder::HandleRead() {
  LOG_DEBUG();

  // Notes: HandleRead here will receive packets for "new connections" and for
  // "existing connections". Read PROTOCOL file for more details.

  // TODO(security): buffer could grow indefinetely, we need bounds and a way to push
  // pressure back to the kernel and to the sender.
  Buffer packet;
  packet.Input()->Reserve(kMaxPacketSize);

  Sockaddr* address;
  if (socket_->Read(packet.Input(), &address) != DatagramChannel::OK) {
    // TODO: handle errors
    LOG_PERROR("udp socket is having troubles receiving data");
    return DatagramChannel::MORE;
  }

  ConnectionKey key(this);
  key.Add(reinterpret_cast<const char*>(address->Data()),
	  static_cast<uint16_t>(address->Size()));

  ServerConnectedSession* session;
  Connection* connection(NULL);
  // FIXME: this seems wrong, if the session is not new, but existing,
  // we end up using connection set to NULL down below, especially
  // in HandlePacket.
  ServerConnectedSession::State state(
      manager_->GetSession(key, packet.Output(), &session));
  if (state == ServerConnectedSession::NeedNewSession) {
    connection = new Connection(&queue_, address);
    session = manager_->CreateSession(key, packet.Output(), connection);
    if (!session) {
      HandleError(session, key, connection, ServerConnectedSession::Manager,
		  "cannot create session");
      return BoundChannel::MORE;
    }
  }

  if (state == ServerConnectedSession::NeedNewSession ||
      state == ServerConnectedSession::Ready)
    state = session->IsReady(key, packet.Output());

  if (state != ServerConnectedSession::Ready) {
    HandleError(
        session, key, connection, ServerConnectedSession::Manager,
	"could not determine session");
    return DatagramChannel::MORE;
  }

  DecodeSessionProtector* decoder(session->GetDecoder());
  Buffer decoded;
  DecodeSessionProtector::Result result;
  result = decoder->Decode(packet.Output(), decoded.Input());
  if (result != DecodeSessionProtector::SUCCEEDED) {
    HandleError(session, key, connection, ServerConnectedSession::Decoding,
	        "decoding failed - truncated packet?");
    return DatagramChannel::MORE;
  }

  session->HandlePacket(key, connection, decoded.Output());
  return DatagramChannel::MORE;
}

ServerUdpTranscoder::Connection::Connection(DatagramSenderPacketQueue* queue, Sockaddr* address)
    : queue_(queue), address_(address), slot_(queue_->GetPacketSlot()) {
}

InputCursor* ServerUdpTranscoder::Connection::Message() {
  return buffer_.Input();
}

InputCursor* ServerUdpTranscoder::Connection::Header() {
  return slot_->buffer.Input();
}

void ServerUdpTranscoder::Connection::Close() {
  // NOOP, nothing to do? seems so.
  // TODO: who deletes this object?
}

bool ServerUdpTranscoder::Connection::SendMessage(
    ServerConnectedSession* session, EncodeSessionProtector* encoder) {
  if (encoder) {
    if (!encoder->Encode(buffer_.Output(), slot_->buffer.Input())) {
      // TODO: handle errors
      return false;
    }
  } else {
    EncodeToBuffer(buffer_.Output(), slot_->buffer.Input());
  }

  slot_->address = address_;
  queue_->QueuePacketSlot(slot_.release());
  slot_.reset(queue_->GetPacketSlot());
  return true;
}
