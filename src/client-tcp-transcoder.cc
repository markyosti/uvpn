#include "client-tcp-transcoder.h"
#include "client-connection-manager.h"
#include "transport.h"
#include "client-authenticator.h"
#include "serializers.h"

ClientTcpTranscoder::ClientTcpTranscoder() {
}

ClientTranscoder::Connection* ClientTcpTranscoder::Connect(
    Transport* transport, ClientConnectionManager* manager,
    const Sockaddr& address) {
  //  This is asynchronous!
  BoundChannel* channel(transport->StreamConnect(address));
  if (!channel) {
    LOG_ERROR("could not stream connect");
    return NULL;
  }

  return new Connection(channel, manager);
}

ClientTcpTranscoder::Connection::Connection(
    BoundChannel* channel, ClientConnectionManager* manager)
    : key_(this),
      channel_(channel),
      session_(NULL),
      manager_(manager),
      decoder_(NULL),
      read_handler_(bind(&ClientTcpTranscoder::Connection::HandleRead, this)),
      write_handler_(bind(&ClientTcpTranscoder::Connection::HandleWrite, this)) {
  LOG_DEBUG();
  channel_->WantRead(&read_handler_);
}

const ConnectionKey& ClientTcpTranscoder::Connection::GetKey() const {
  return key_;
}

InputCursor* ClientTcpTranscoder::Connection::Message() {
  LOG_DEBUG();
  return from_user_cleartext_.Input();
}

InputCursor* ClientTcpTranscoder::Connection::Header() {
  LOG_DEBUG();
  return from_user_encrypted_.Input();
}

void ClientTcpTranscoder::Connection::Close() {
  channel_->Close();
}

bool ClientTcpTranscoder::Connection::SendMessage(
    ClientConnectedSession* session, EncodeSessionProtector* encoder) {
  LOG_DEBUG("%d bytes are ready to send", from_user_cleartext_.Output()->LeftSize());
  channel_->WantWrite(&write_handler_);

  // Send size of the packet.
  if (encoder) {
    LOG_DEBUG("encoding message");
    if (!encoder->Start(from_user_encrypted_.Input(), SessionProtector::NoPadding)) {
      HandleError(session, ClientConnectedSession::Encoding, "could not start encoder");
      return false;
    }

    Buffer size;
    EncodeToBuffer(static_cast<uint16_t>(from_user_cleartext_.Output()->LeftSize()), size.Input());
    if (!encoder->Continue(size.Output(), from_user_encrypted_.Input())) {
      HandleError(session, ClientConnectedSession::Encoding, "could not encrypt data");
      return false;
    }

    // Pad the buffer now, so we never end up with leftover data.
    encoder->AddPadding(
        from_user_encrypted_.Input(),
        from_user_cleartext_.Output()->LeftSize() + sizeof(uint16_t));

    // Encode packet itself.
    if (!encoder->Continue(
	     from_user_cleartext_.Output(), from_user_encrypted_.Input()) ||
        !encoder->End(from_user_encrypted_.Input())) {
      HandleError(session, ClientConnectedSession::Encoding, "could not add packet");
      return false;
    }
  } else {
    LOG_DEBUG("sending message as is, %08x", (unsigned int)encoder);
    EncodeToBuffer(from_user_cleartext_.Output(), from_user_encrypted_.Input());
  }

  LOG_DEBUG("message ready to send, out buffer now has %d bytes",
	    from_user_encrypted_.Output()->LeftSize());
  return true;
}

BoundChannel::processing_state_e ClientTcpTranscoder::Connection::HandleWrite() {
  LOG_DEBUG("attempting to write %d bytes",
	    from_user_encrypted_.Output()->LeftSize());

  channel_->Write(from_user_encrypted_.Output());
  // Remove write handler, as we don't have anything more to write.
  if (!from_user_encrypted_.Output()->LeftSize())
    return BoundChannel::DONE;
  return BoundChannel::MORE;
}

void ClientTcpTranscoder::Connection::HandleError(
    ClientConnectedSession* session, const ClientConnectedSession::CloseReason error,
    const char* message) {
  LOG_ERROR("closing session: %s", message);
  if (session)
    session->HandleError(key_, this, error);
  else
    manager_->HandleError(key_, this, error);
}

BoundChannel::processing_state_e
    ClientTcpTranscoder::Connection::HandleDecodeError(
        ClientConnectedSession* session, DecodeSessionProtector::Result result,
	const char* message) {
  if (result == DecodeSessionProtector::MORE_DATA_REQUIRED) {
    LOG_DEBUG("decoding error, %s", message);
    return BoundChannel::MORE;
  }
  
  HandleError(session, ClientConnectedSession::Decoding, message);
  return BoundChannel::DONE;
}

// HandleRead is called whenever there's a tcp message - eg, whenever the
// remote server sends us something. We need to: decrypt this data, and send
// it down the stack to the io-channel or authenticator.
// Data is read in server_in_buffer, and decoded in server_out_buffer.
BoundChannel::processing_state_e ClientTcpTranscoder::Connection::HandleRead() {
  LOG_DEBUG("server_in_buffer %d, server_out_buffer %d",
	    from_server_encrypted_.Output()->LeftSize(),
	    from_server_cleartext_.Output()->LeftSize());

  // TODO(security): reserve here is wrong :/ we should fill the buffer
  // and create back pressure on the kernel.
  from_server_encrypted_.Input()->Reserve(kReadSize);
  BoundChannel::io_result_e status(channel_->Read(from_server_encrypted_.Input()));
  if (status != BoundChannel::OK) {
    if (status == BoundChannel::CLOSED)
      HandleError(NULL, ClientConnectedSession::Shutdown, "closed");
    else
      HandleError(NULL, ClientConnectedSession::System, "read failed");
    return BoundChannel::DONE;
  }

  DecodeSessionProtector::Result result;
  while (from_server_encrypted_.Output()->LeftSize()) {
    // This TCP connection has not been bound to a session (yet?).
    if (!session_) {
      ClientConnectedSession::State state(
          manager_->GetSession(key_, from_server_encrypted_.Output(), &session_));
      if (state == ClientConnectedSession::NeedNewSession) {
	session_ = manager_->CreateSession(
	    key_, from_server_encrypted_.Output(), this);
	if (!session_) {
	  HandleError(session_, ClientConnectedSession::Manager, "cannot create session");
          return BoundChannel::DONE;
	}
      }
    
      if (state == ClientConnectedSession::NeedNewSession ||
	  state == ClientConnectedSession::Ready) {
	state = session_->IsReady(key_, from_server_encrypted_.Output());
      }

      if (state != ClientConnectedSession::Ready) {
	if (state == ClientConnectedSession::NeedMoreData) {
	  LOG_DEBUG("session needs more data");
	  return BoundChannel::MORE;
	}

	HandleError(session_, ClientConnectedSession::Manager, "could not determine session");
        return BoundChannel::DONE;
      }
    }

    if (!decoder_) {
      DecodeSessionProtector* decoder(session_->GetDecoder());

      OutputCursor encrypted(*from_server_encrypted_.Output());
      result = decoder->Start(
          &encrypted, from_server_cleartext_.Input(),
	  DecodeSessionProtector::NoPadding);

      if (result != DecodeSessionProtector::SUCCEEDED)
	return HandleDecodeError(
	    session_, result, "could not start decoder");

      from_server_encrypted_.Output()->Increment(
          from_server_encrypted_.Output()->LeftSize() - encrypted.LeftSize());
      decoder_ = decoder;
    }

    if (from_server_cleartext_.Output()->LeftSize() < sizeof(uint16_t)) {
      result = decoder_->Continue(
          from_server_encrypted_.Output(), from_server_cleartext_.Input(),
          sizeof(uint16_t) - from_server_cleartext_.Output()->LeftSize());
      if (result != DecodeSessionProtector::SUCCEEDED)
        return HandleDecodeError(session_, result, "could not decode input size");
    }

    // TODO(optimization): this is freaking slow. Eg, if we don't have enough data,
    // we wait for more data and start over, a client sending one byte at a time could
    // easily DoS the server. Need a better state machine here.
 
    OutputCursor cleartext(*from_server_cleartext_.Output());

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
          from_server_encrypted_.Output(), from_server_cleartext_.Input(),
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

    decoder_->End(from_server_cleartext_.Input());
    cleartext.LimitLeftSize(size);
    session_->HandlePacket(key_, this, &cleartext);

    session_ = NULL;
    decoder_ = NULL;

    // TODO: we should not return BoundChannel::MORE if the buffer is full,
    // to create back pressure on the client and tcp stack.
    from_server_cleartext_.Output()->Increment(
        size + padsize + sizeof(uint16_t));
    LOG_DEBUG("incrementing by %d, %d, %d = %d",
	      size, sizeof(uint16_t), padsize,
	      size + sizeof(uint16_t) + padsize);
  }

  return BoundChannel::MORE;
}
