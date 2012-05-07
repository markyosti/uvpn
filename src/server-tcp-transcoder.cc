#include "server-tcp-transcoder.h"
#include "connection-key.h"
#include "server-authenticator.h"
#include "serializers.h"

ServerTcpTranscoder::ServerTcpTranscoder(
    Transport* transport, const Sockaddr& address,
    ServerConnectionManager* manager)
    : transport_(transport),
      manager_(manager),
      client_connect_handler_(bind(&ServerTcpTranscoder::HandleConnection, this)),
      address_(address) {
}

bool ServerTcpTranscoder::Start() {
  LOG_DEBUG("server-tcp-transcoder, %s", address_.AsString().c_str());

  socket_.reset(transport_->StreamListenOn(address_));
  if (!socket_.get()) {
    LOG_ERROR("could not listen on streaming socket");
    return false;
  }

  socket_->WantConnection(&client_connect_handler_);
  return true;
}

AcceptingChannel::processing_state_e ServerTcpTranscoder::HandleConnection() {
  BoundChannel* channel(socket_->AcceptConnection(NULL));
  if (!channel) {
    LOG_PERROR("error while accepting new connection");
    return AcceptingChannel::MORE;
  }
  LOG_DEBUG("accepted new connection");

  Connection* connection = new Connection(channel, manager_);
  // TODO: track connections somewhere.
  return AcceptingChannel::MORE;
}

ServerTcpTranscoder::Connection::Connection(
    BoundChannel* channel, ServerConnectionManager* manager)
    : key_(this),
      channel_(channel),
      session_(NULL),
      manager_(manager),
      decoder_(NULL),
      read_handler_(bind(&ServerTcpTranscoder::Connection::HandleRead, this)),
      write_handler_(bind(&ServerTcpTranscoder::Connection::HandleWrite, this)) {
  LOG_DEBUG();
  channel_->WantRead(&read_handler_);
}

InputCursor* ServerTcpTranscoder::Connection::Message() {
  LOG_DEBUG();
  return from_tunnel_cleartext_.Input();
}

InputCursor* ServerTcpTranscoder::Connection::Header() {
  LOG_DEBUG();
  return from_tunnel_encrypted_.Input();
}

void ServerTcpTranscoder::Connection::Close() {
  channel_->Close();
}

bool ServerTcpTranscoder::Connection::SendMessage(
    ServerConnectedSession* session, EncodeSessionProtector* encoder) {
  LOG_DEBUG("%d bytes are ready to send", from_tunnel_cleartext_.Output()->LeftSize());
  channel_->WantWrite(&write_handler_);

  // Send size of the packet.
  if (encoder) {
    LOG_DEBUG("encoding message");
    if (!encoder->Start(from_tunnel_encrypted_.Input(), SessionProtector::NoPadding)) {
      HandleError(session, ServerConnectedSession::Encoding, "could not start encoder");
      return false;
    }

    Buffer size;
    EncodeToBuffer(static_cast<uint16_t>(from_tunnel_cleartext_.Output()->LeftSize()), size.Input());
    if (!encoder->Continue(size.Output(), from_tunnel_encrypted_.Input())) {
      HandleError(session, ServerConnectedSession::Encoding, "could not encrypt data");
      return false;
    }

    // Pad the buffer now, so we never end up with leftover data.
    encoder->AddPadding(
        from_tunnel_encrypted_.Input(),
        from_tunnel_cleartext_.Output()->LeftSize() + sizeof(uint16_t));

    // Encode packet itself.
    if (!encoder->Continue(
	     from_tunnel_cleartext_.Output(), from_tunnel_encrypted_.Input()) ||
        !encoder->End(from_tunnel_encrypted_.Input())) {
      HandleError(session, ServerConnectedSession::Encoding, "could not add packet");
      return false;
    }
  } else {
    LOG_DEBUG("sending message as is, %08x", (unsigned int)encoder);
    EncodeToBuffer(from_tunnel_cleartext_.Output(), from_tunnel_encrypted_.Input());
  }

  LOG_DEBUG("message ready to send, out buffer now has %d bytes",
	    from_tunnel_encrypted_.Output()->LeftSize());
  return true;
}

BoundChannel::processing_state_e ServerTcpTranscoder::Connection::HandleWrite() {
  LOG_DEBUG("attempting to write %d bytes",
	    from_tunnel_encrypted_.Output()->LeftSize());

  channel_->Write(from_tunnel_encrypted_.Output());
  // Remove write handler, as we don't have anything more to write.
  if (!from_tunnel_encrypted_.Output()->LeftSize())
    return BoundChannel::DONE;
  return BoundChannel::MORE;
}

void ServerTcpTranscoder::Connection::HandleError(
    ServerConnectedSession* session, const ServerConnectedSession::CloseReason error,
    const char* message) {
  LOG_ERROR("closing session: %s", message);
  if (session)
    session->HandleError(key_, this, error);
  else
    manager_->HandleError(key_, this, error);
}

BoundChannel::processing_state_e
    ServerTcpTranscoder::Connection::HandleDecodeError(
        ServerConnectedSession* session, DecodeSessionProtector::Result result,
	const char* message) {
  if (result == DecodeSessionProtector::MORE_DATA_REQUIRED) {
    LOG_DEBUG("decoding error, %s", message);
    return BoundChannel::MORE;
  }
  
  HandleError(session, ServerConnectedSession::Decoding, message);
  return BoundChannel::DONE;
}

BoundChannel::processing_state_e ServerTcpTranscoder::Connection::HandleRead() {
  LOG_DEBUG("client_in_buffer %d, client_out_buffer %d",
	    from_client_encrypted_.Output()->LeftSize(),
	    from_client_cleartext_.Output()->LeftSize());

  // TODO(security): buffer could grow indefinetely, we need bounds and a way to push
  // pressure back to the kernel and to the sender.
  from_client_encrypted_.Input()->Reserve(kReadSize);
  BoundChannel::io_result_e status(channel_->Read(from_client_encrypted_.Input()));
  if (status != BoundChannel::OK) {
    if (status == BoundChannel::CLOSED)
      HandleError(NULL, ServerConnectedSession::Shutdown, "closed");
    else
      HandleError(NULL, ServerConnectedSession::System, "read failed");
    return BoundChannel::DONE;
  }

  DecodeSessionProtector::Result result;
  while (from_client_encrypted_.Output()->LeftSize()) {
    // This TCP connection has not been bound to a session (yet?).
    if (!session_) {
      ServerConnectedSession::State state(
          manager_->GetSession(key_, from_client_encrypted_.Output(), &session_));
      if (state == ServerConnectedSession::NeedNewSession) {
	session_ = manager_->CreateSession(
	    key_, from_client_encrypted_.Output(), this);
	if (!session_) {
	  HandleError(session_, ServerConnectedSession::Manager, "cannot create session");
          return BoundChannel::DONE;
	}
      }
    
      if (state == ServerConnectedSession::NeedNewSession ||
	  state == ServerConnectedSession::Ready) {
	state = session_->IsReady(key_, from_client_encrypted_.Output());
      }

      if (state != ServerConnectedSession::Ready) {
	if (state == ServerConnectedSession::NeedMoreData) {
	  LOG_DEBUG("session needs more data");
	  return BoundChannel::MORE;
	}

	HandleError(session_, ServerConnectedSession::Manager, "could not determine session");
        return BoundChannel::DONE;
      }
    }

    if (!decoder_) {
      DecodeSessionProtector* decoder(session_->GetDecoder());

      OutputCursor encrypted(*from_client_encrypted_.Output());
      result = decoder->Start(
          &encrypted, from_client_cleartext_.Input(),
	  DecodeSessionProtector::NoPadding);

      if (result != DecodeSessionProtector::SUCCEEDED)
	return HandleDecodeError(
	    session_, result, "could not start decoder");

      from_client_encrypted_.Output()->Increment(
          from_client_encrypted_.Output()->LeftSize() - encrypted.LeftSize());
      decoder_ = decoder;
    }

    if (from_client_cleartext_.Output()->LeftSize() < sizeof(uint16_t)) {
      result = decoder_->Continue(
          from_client_encrypted_.Output(), from_client_cleartext_.Input(),
          sizeof(uint16_t) - from_client_cleartext_.Output()->LeftSize());
      if (result != DecodeSessionProtector::SUCCEEDED)
        return HandleDecodeError(session_, result, "could not decode input size");
    }

    // TODO(optimization): this is freaking slow. Eg, if we don't have enough data,
    // we wait for more data and start over, a client sending one byte at a time could
    // easily DoS the server. Need a better state machine here.
 
    OutputCursor cleartext(*from_client_cleartext_.Output());

    // Read the size of the packet.
    uint16_t size;
    if (!DecodeFromBuffer(&cleartext, &size)) {
      LOG_DEBUG("not enough data to decode input size");
      return BoundChannel::MORE;
    }

    LOG_DEBUG("packet size %d, available in cleartext %d, to decode %d",
	      size, cleartext.LeftSize(), size - cleartext.LeftSize());
    if (cleartext.LeftSize() < size) {
      result = decoder_->Continue(
          from_client_encrypted_.Output(), from_client_cleartext_.Input(),
          size - cleartext.LeftSize());
      if (result != DecodeSessionProtector::SUCCEEDED)
        return HandleDecodeError(session_, result, "could not decode data");

      LOG_DEBUG("left size %d", cleartext.LeftSize());
      if (cleartext.LeftSize() < size) {
        LOG_DEBUG("not all data received, need %d, have %d",
  		  size, cleartext.LeftSize());
        return BoundChannel::MORE;
      }
    }

    uint8_t padsize;
    result = decoder_->RemovePadding(&cleartext, size + sizeof(uint16_t), &padsize);
    if (result != DecodeSessionProtector::SUCCEEDED)
      return HandleDecodeError(session_, result, "could not remove padding");

    decoder_->End(from_client_cleartext_.Input());
    cleartext.LimitLeftSize(size);
    session_->HandlePacket(key_, this, &cleartext);

    session_ = NULL;
    decoder_ = NULL;

    // TODO: we should not return BoundChannel::MORE if the buffer is full,
    // to create back pressure on the client and tcp stack.
    from_client_cleartext_.Output()->Increment(
        size + padsize + sizeof(uint16_t));
    LOG_DEBUG("incrementing by %d, %d, %d = %d",
	      size, sizeof(uint16_t), padsize,
	      size + sizeof(uint16_t) + padsize);
  }

  return BoundChannel::MORE;
}
