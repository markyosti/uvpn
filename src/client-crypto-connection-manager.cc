#include "client-crypto-connection-manager.h"
#include "stl-helpers.h"
#include "scramble-session-protector.h"
#include "client-authenticator.h"
#include "client-io-channel.h"
#include "dispatcher.h"
#include "sockaddr.h"

// Logic, currently:
//  - GetSession - checks only the key to see if session is new or not.
//  - if session is not in table, it returns session->IsReady();
ClientCryptoConnectionManager::ClientCryptoConnectionManager(
    Prng* prng, Dispatcher* dispatcher)
    : prng_(prng), dispatcher_(dispatcher) {
}

ClientConnectedSession::State ClientCryptoConnectionManager::GetSession(
    const ConnectionKey& key, OutputCursor* cursor, ClientConnectedSession** retval) {
  // ClientCryptoConnectionManager just uses the transcoder connection key to
  // find the session.
  ClientCryptoConnectionManager::Session* session(StlMapGet(sessions_map_, key));
  if (!session) {
    LOG_DEBUG("client that needs new session??? all sessions should be known");
    return ClientConnectedSession::NeedNewSession;
  }

  *retval = session;
  return ClientConnectedSession::Ready;
}

ClientConnectedSession* ClientCryptoConnectionManager::CreateSession(
    const ConnectionKey& key, OutputCursor* cursor,
    ClientTranscoder::Connection* connection) {
  // If we get here, it means that the client is getting back packets
  // belonging to a session it doesn't know / understand. This is
  // not supported right now.
  return NULL;
}

void ClientCryptoConnectionManager::AddConnection(
    const string& destination, UserChatter* chatter) {
  // Pseudo code:
  //   - pick which transcoders we want to use.
  //     Call Connect() for each transcoder.
  //     Register the session key for each transcoder.
  //
  //   - start authenticator. authenticator will invoke SendMessage.
  //   - when SendMessage is invoked, we need to pick the transcoders
  //     on which we'll send the message.
  //
  //   - when message is received, need to pass it to the handler.
  //     If multiple transcoders are being used, need to de-dupe messages.
  
  // Find the address of the destination.
  // TODO(kramonerom): add support for dns resolution.
  auto_ptr<Sockaddr> sockaddr(Sockaddr::Parse(destination, 1029));
  if (!sockaddr.get()) {
    LOG_ERROR("could not convert %s to address", destination.c_str());
    return;
  }

  // Create a transcoder. In this case, we'll just open a tcp connection
  // (or udp, depending on what kind of transcoder we were given to begin
  // with).
  ClientTranscoder::Connection* connection = transcoder_->Connect(
      transport_.get(), this, *(sockaddr.get()));
  if (!connection) {
    // TODO: handle errors! what do we do here?
  }

  // Create a new authentication session.
  Session* session(new Session(
      this, chatter, connection, authenticator_.get(), channel_.get()));
  sessions_map_[connection->GetKey()] = session;

  LOG_DEBUG("created new session %08x", (unsigned int)session);
  return;
}

void ClientCryptoConnectionManager::RegisterTransport(Transport* transport) {
  // TODO: there should be code to autodetect proxies, for example, and try to
  // use them, rather than just pick a random transport.
  transport_.reset(transport);
}

void ClientCryptoConnectionManager::RegisterIOChannel(ClientIOChannel* channel) {
  channel_.reset(channel);
}

void ClientCryptoConnectionManager::RegisterTranscoder(ClientTranscoder* transcoder) {
  transcoder_.reset(transcoder);
}

void ClientCryptoConnectionManager::RegisterAuthenticator(
    ClientAuthenticator* authenticator) {
  authenticator_.reset(authenticator);
}

ClientCryptoConnectionManager::Session::Session(
    ClientCryptoConnectionManager* parent,
    UserChatter* chatter,
    ClientTranscoder::Connection* connection,
    ClientAuthenticator* authenticator, ClientIOChannel* channel)
    : parent_(parent),
      state_(SessionStateAuthenticationPending),
      connection_(connection),
      chatter_(chatter),
      authenticator_(authenticator),
      channel_(channel),
      authentication_done_handler_(bind(
        &ClientCryptoConnectionManager::Session::AuthenticationDoneHandler, this,
	placeholders::_1, placeholders::_2, placeholders::_3)),
      read_callback_(NULL),
      close_callback_(NULL) {
  DEBUG_FATAL_UNLESS(channel_)("no channel to use for connections??");
  DEBUG_FATAL_UNLESS(authenticator_)("no authenticator to use for connections??");

  authenticator_->StartAuthentication(this, NULL, chatter_, &authentication_done_handler_);
}

ClientConnectedSession::State ClientCryptoConnectionManager::Session::IsReady(
    const ConnectionKey& key, OutputCursor* cursor) {
  return Ready;
}

void ClientCryptoConnectionManager::Session::AuthenticationDoneHandler(
    ClientAuthenticator::AuthenticationStatus status,
    EncodeSessionProtector* encoder, DecodeSessionProtector* decoder) {
  // TODO: some authenticators want to delay some expensive operations
  //   until next packet is received, any way to do that?
  LOG_DEBUG("encoder %08x, decoder %08x",
	    (unsigned int)encoder, (unsigned int)decoder);

  switch (status) {
    case ClientAuthenticator::SessionAuthenticated:
    case ClientAuthenticator::SessionMaybeAuthenticated:
      SetEncoder(encoder);
      SetDecoder(decoder);
      channel_->Start(this);
      break;

    case ClientAuthenticator::SessionFailed:
      // TODO(security): we should stop here!
      break;

    case ClientAuthenticator::SessionDenied:
      // TODO(security): even worse! should stop here!
      break;

    default:
      LOG_FATAL("unknown authentication result %d", status);
      break;
  }
}

void ClientCryptoConnectionManager::Session::SetEncoder(
    EncodeSessionProtector* encoder) {
  encoder_.reset(encoder);
}

void ClientCryptoConnectionManager::Session::SetDecoder(
    DecodeSessionProtector* decoder) {
  decoder_.reset(decoder);
}

DecodeSessionProtector* ClientCryptoConnectionManager::Session::GetDecoder() {
  if (!decoder_.get())
    SetDecoder(new ScrambleSessionDecoder());
  return decoder_.get();
}

EncodeSessionProtector* ClientCryptoConnectionManager::Session::GetEncoder() {
  if (!encoder_.get())
    SetEncoder(new ScrambleSessionEncoder(parent_->prng_));
  return encoder_.get();
}

void ClientCryptoConnectionManager::Session::HandlePacket(
    const ConnectionKey& key, ClientTranscoder::Connection* connection,
    OutputCursor* data) {
  (*read_callback_)(this, data);
}

void ClientCryptoConnectionManager::HandleError(
    const ConnectionKey& key, ClientTranscoder::Connection* connection,
    const ClientConnectedSession::CloseReason error) {
  LOG_DEBUG("connection: %08x, reason: %d", (unsigned int)connection, error);

  ClientCryptoConnectionManager::Session* session(StlMapGet(sessions_map_, key));
  DEBUG_FATAL_UNLESS(session)("reporting error for non-existing session");
  session->HandleError(key, connection, error);
}

void ClientCryptoConnectionManager::Session::HandleError(
    const ConnectionKey& key, ClientTranscoder::Connection* connection,
    const CloseReason error) {
  LOG_DEBUG("connection: %08x, reason: %d", (unsigned int)connection, error);

  SessionsMap* map(&parent_->sessions_map_);
  SessionsMap::iterator it(map->find(key));
  if (it != map->end()) {
    LOG_DEBUG("deleting session now");
    Session* session(it->second);
    map->erase(it);

    DEBUG_FATAL_UNLESS(session == this)(
        "how did we end up having one session deleting a different session?");

    session->Close();
    parent_->dispatcher_->DeleteLater(session);
  }
}

void ClientCryptoConnectionManager::Session::Close() {
  connection_->Close();
}

InputCursor* ClientCryptoConnectionManager::Session::Message() {
  return connection_->Message();
}

bool ClientCryptoConnectionManager::Session::SendMessage() {
  return connection_->SendMessage(this, GetEncoder());
}

void ClientCryptoConnectionManager::Session::SetCallbacks(
    read_handler_t* readh, close_handler_t* closeh) {
  read_callback_ = readh;
  close_callback_ = closeh;
}
