#include "server-crypto-connection-manager.h"
#include "stl-helpers.h"
#include "scramble-session-protector.h"
#include "server-authenticator.h"
#include "server-io-channel.h"

// TODO:
// - lookup SID in hash table, if SID is not there, request new session
//   being created.
// - keep track of where we stand with each packet, eg, is this the beginning
//   of a session? or something else?
// - keep track of latency and similar. 

ServerCryptoConnectionManager::ServerCryptoConnectionManager(
    Prng* prng, Dispatcher* dispatcher)
    : prng_(prng), dispatcher_(dispatcher) {
}

ServerConnectedSession::State ServerCryptoConnectionManager::GetSession(
    const ConnectionKey& key, OutputCursor* cursor, ServerConnectedSession** retval) {
  // ServerCryptoConnectionManager just uses the transcoder connection key to
  // find the session.
  ServerCryptoConnectionManager::Session* session(StlMapGet(sessions_map_, key));
  if (!session)
    return ServerConnectedSession::NeedNewSession;

  *retval = session;
  return ServerConnectedSession::Ready;
}

ServerConnectedSession* ServerCryptoConnectionManager::CreateSession(
    const ConnectionKey& key, OutputCursor* cursor,
    ServerTranscoder::Connection* connection) {
  Session* session(new Session(
      this, connection, authenticator_.get(), channel_.get()));
  sessions_map_[key] = session;
  LOG_DEBUG("creating new session %08x", (unsigned int)session);
  return session;
}

void ServerCryptoConnectionManager::RegisterIOChannel(ServerIOChannel* channel) {
  channel_.reset(channel);
}

void ServerCryptoConnectionManager::RegisterAuthenticator(
    ServerAuthenticator* authenticator) {
  authenticator_.reset(authenticator);
}

ServerCryptoConnectionManager::Session::Session(
    ServerCryptoConnectionManager* parent,
    ServerTranscoder::Connection* connection,
    ServerAuthenticator* authenticator, ServerIOChannel* channel)
    : parent_(parent),
      state_(SessionStateAuthenticationPending),
      connection_(connection),
      authenticator_(authenticator),
      channel_(channel),
      authenticator_read_handler_(bind(
        &ServerCryptoConnectionManager::Session::StartAuthenticator, this,
        placeholders::_1, placeholders::_2)),
      authentication_done_handler_(bind(
        &ServerCryptoConnectionManager::Session::AuthenticationDoneHandler, this,
	placeholders::_1, placeholders::_2, placeholders::_3)),
      read_callback_(&authenticator_read_handler_), 
      close_callback_(NULL) {
  DEBUG_FATAL_UNLESS(channel_)("no channel to use for connections??");
  DEBUG_FATAL_UNLESS(authenticator_)("no authenticator to use for connections??");
}

void ServerCryptoConnectionManager::Session::StartAuthenticator(
    ServerConnectedSession* session, OutputCursor* cursor) {
  authenticator_->StartAuthentication(session, cursor, &authentication_done_handler_);
}

ServerConnectedSession::State ServerCryptoConnectionManager::Session::IsReady(
    const ConnectionKey& key, OutputCursor* cursor) {
  return Ready;
}

void ServerCryptoConnectionManager::Session::AuthenticationDoneHandler(
    ServerAuthenticator::AuthenticationStatus status,
    EncodeSessionProtector* encoder, DecodeSessionProtector* decoder) {
  // TODO: some authenticator want to delay some expensive operations
  //   until next packet is received, any way to do that?
  LOG_DEBUG("encoder %08x, decoder %08x",
	    (unsigned int)encoder, (unsigned int)decoder);

  switch (status) {
    case ServerAuthenticator::SessionAuthenticated:
    case ServerAuthenticator::SessionMaybeAuthenticated:
      SetEncoder(encoder);
      SetDecoder(decoder);
      channel_->HandleConnect(this);
      break;

    case ServerAuthenticator::SessionFailed:
      // TODO(security): we should stop here!
      break;

    case ServerAuthenticator::SessionDenied:
      // TODO(security): even worse! should stop here!
      break;

    default:
      LOG_FATAL("unknown authentication result %d", status);
      break;
  }
}

void ServerCryptoConnectionManager::Session::SetEncoder(
    EncodeSessionProtector* encoder) {
  encoder_.reset(encoder);
}

void ServerCryptoConnectionManager::Session::SetDecoder(
    DecodeSessionProtector* decoder) {
  decoder_.reset(decoder);
}

DecodeSessionProtector* ServerCryptoConnectionManager::Session::GetDecoder() {
  if (!decoder_.get())
    SetDecoder(new ScrambleSessionDecoder());
  return decoder_.get();
}

EncodeSessionProtector* ServerCryptoConnectionManager::Session::GetEncoder() {
  if (!encoder_.get())
    SetEncoder(new ScrambleSessionEncoder(parent_->prng_));
  return encoder_.get();
}

void ServerCryptoConnectionManager::Session::HandlePacket(
    const ConnectionKey& key, ServerTranscoder::Connection* connection,
    OutputCursor* data) {
  (*read_callback_)(this, data);
}

void ServerCryptoConnectionManager::HandleError(
    const ConnectionKey& key, ServerTranscoder::Connection* connection,
    const ServerConnectedSession::CloseReason error) {
  LOG_DEBUG("connection: %08x, reason: %d", (unsigned int)connection, error);

  ServerCryptoConnectionManager::Session* session(StlMapGet(sessions_map_, key));
  DEBUG_FATAL_UNLESS(session)("reporting error for non-existing session");
  session->HandleError(key, connection, error);
}

void ServerCryptoConnectionManager::Session::HandleError(
    const ConnectionKey& key, ServerTranscoder::Connection* connection,
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

void ServerCryptoConnectionManager::Session::Close() {
  connection_->Close();
}

InputCursor* ServerCryptoConnectionManager::Session::Message() {
  return connection_->Message();
}

bool ServerCryptoConnectionManager::Session::SendMessage() {
  return connection_->SendMessage(this, GetEncoder());
}

void ServerCryptoConnectionManager::Session::SetCallbacks(
    read_handler_t* readh, close_handler_t* closeh) {
  read_callback_ = readh;
  close_callback_ = closeh;
}
