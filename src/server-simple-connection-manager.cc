#include "server-simple-connection-manager.h"
#include "stl-helpers.h"
#include "scramble-session-protector.h"
#include "server-authenticator.h"
#include "server-io-channel.h"

// Logic, currently:
//  - GetSession - checks only the key to see if session is new or not.
//  - if session is not in table, it returns session->IsReady();
ServerSimpleConnectionManager::ServerSimpleConnectionManager(
    Prng* prng, Dispatcher* dispatcher)
    : prng_(prng), dispatcher_(dispatcher) {
}

ServerConnectedSession::State ServerSimpleConnectionManager::GetSession(
    const ConnectionKey& key, OutputCursor* cursor, ServerConnectedSession** retval) {
  // ServerSimpleConnectionManager just uses the transcoder connection key to
  // find the session.
  ServerSimpleConnectionManager::Session* session(StlMapGet(sessions_map_, key));
  if (!session)
    return ServerConnectedSession::NeedNewSession;

  *retval = session;
  return ServerConnectedSession::Ready;
}

ServerConnectedSession* ServerSimpleConnectionManager::CreateSession(
    const ConnectionKey& key, OutputCursor* cursor,
    ServerTranscoder::Connection* connection) {
  Session* session(new Session(
      this, connection, authenticator_.get(), channel_.get()));
  sessions_map_[key] = session;
  LOG_DEBUG("creating new session %08x", (unsigned int)session);
  return session;
}

void ServerSimpleConnectionManager::RegisterIOChannel(ServerIOChannel* channel) {
  channel_.reset(channel);
}

void ServerSimpleConnectionManager::RegisterAuthenticator(
    ServerAuthenticator* authenticator) {
  authenticator_.reset(authenticator);
}

ServerSimpleConnectionManager::Session::Session(
    ServerSimpleConnectionManager* parent,
    ServerTranscoder::Connection* connection,
    ServerAuthenticator* authenticator, ServerIOChannel* channel)
    : parent_(parent),
      state_(SessionStateAuthenticationPending),
      connection_(connection),
      authenticator_(authenticator),
      channel_(channel),
      authenticator_read_handler_(bind(
        &ServerSimpleConnectionManager::Session::StartAuthenticator, this,
        placeholders::_1, placeholders::_2)),
      authentication_done_handler_(bind(
        &ServerSimpleConnectionManager::Session::AuthenticationDoneHandler, this,
	placeholders::_1, placeholders::_2, placeholders::_3)),
      read_callback_(&authenticator_read_handler_), 
      close_callback_(NULL) {
  DEBUG_FATAL_UNLESS(channel_)("no channel to use for connections??");
  DEBUG_FATAL_UNLESS(authenticator_)("no authenticator to use for connections??");
}

void ServerSimpleConnectionManager::Session::StartAuthenticator(
    ServerConnectedSession* session, OutputCursor* cursor) {
  authenticator_->StartAuthentication(session, cursor, &authentication_done_handler_);
}

ServerConnectedSession::State ServerSimpleConnectionManager::Session::IsReady(
    const ConnectionKey& key, OutputCursor* cursor) {
  return Ready;
}

void ServerSimpleConnectionManager::Session::AuthenticationDoneHandler(
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

void ServerSimpleConnectionManager::Session::SetEncoder(
    EncodeSessionProtector* encoder) {
  encoder_.reset(encoder);
}

void ServerSimpleConnectionManager::Session::SetDecoder(
    DecodeSessionProtector* decoder) {
  decoder_.reset(decoder);
}

DecodeSessionProtector* ServerSimpleConnectionManager::Session::GetDecoder() {
  if (!decoder_.get())
    SetDecoder(new ScrambleSessionDecoder());
  return decoder_.get();
}

EncodeSessionProtector* ServerSimpleConnectionManager::Session::GetEncoder() {
  if (!encoder_.get())
    SetEncoder(new ScrambleSessionEncoder(parent_->prng_));
  return encoder_.get();
}

void ServerSimpleConnectionManager::Session::HandlePacket(
    const ConnectionKey& key, ServerTranscoder::Connection* connection,
    OutputCursor* data) {
  (*read_callback_)(this, data);
}

void ServerSimpleConnectionManager::HandleError(
    const ConnectionKey& key, ServerTranscoder::Connection* connection,
    const ServerConnectedSession::CloseReason error) {
  LOG_DEBUG("connection: %08x, reason: %d", (unsigned int)connection, error);

  ServerSimpleConnectionManager::Session* session(StlMapGet(sessions_map_, key));
  DEBUG_FATAL_UNLESS(session)("reporting error for non-existing session");
  session->HandleError(key, connection, error);
}

void ServerSimpleConnectionManager::Session::HandleError(
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

void ServerSimpleConnectionManager::Session::Close() {
  connection_->Close();
}

InputCursor* ServerSimpleConnectionManager::Session::Message() {
  return connection_->Message();
}

bool ServerSimpleConnectionManager::Session::SendMessage() {
  return connection_->SendMessage(this, GetEncoder());
}

void ServerSimpleConnectionManager::Session::SetCallbacks(
    read_handler_t* readh, close_handler_t* closeh) {
  read_callback_ = readh;
  close_callback_ = closeh;
}
