#include "srp-server-authenticator.h"
#include "aes-session-protector.h"
#include "conversions.h"

SrpServerAuthenticator::SrpServerAuthenticator(UserDb* udb, Prng* prng)
    : prng_(prng),
      userdb_(udb) {
}

void SrpServerAuthenticator::StartAuthentication(
    ServerConnectedSession* connection, OutputCursor* cursor,
    authentication_done_handler_t* callback) {
  LOG_DEBUG();

  AuthenticationSession* auths = new AuthenticationSession(this, callback);

  // TODO: keep track of all authentication sessions, for cleanup and tracking
  // purposes. Right now, we rely on each session cleaning up by itself.
  return auths->StartAuthentication(connection, cursor);
}

SrpServerAuthenticator::AuthenticationSession::AuthenticationSession(
    SrpServerAuthenticator* parent,
    authentication_done_handler_t* callback)
    : parent_(parent),
      srps_(parent->prng_, &(parent->cntx_)),
      authentication_done_callback_(callback),
      parse_public_key_callback_(bind(
        &SrpServerAuthenticator::AuthenticationSession::ParsePublicKeyCallback, this, placeholders::_1, placeholders::_2)),
      close_callback_(bind(
        &SrpServerAuthenticator::AuthenticationSession::CloseCallback, this, placeholders::_1, placeholders::_2)) {
  LOG_DEBUG("Authentication session created");
}

SrpServerAuthenticator::AuthenticationSession::~AuthenticationSession() {
  LOG_DEBUG();
  // FIXME: delete this?
}

void SrpServerAuthenticator::AuthenticationSession::StartAuthentication(
    ServerConnectedSession* connection, OutputCursor* cursor) {
  LOG_DEBUG();

  if (!srps_.ParseClientHello(cursor, &username_)) {
    LOG_FATAL("failed to parse hello");
    // TODO: handle error
    return;
  }

  string encodedpassword;;
  UserDbSession udbs(parent_->userdb_);
  if (!parent_->userdb_->GetUser(username_, &encodedpassword)) {
    LOG_FATAL("failed to find username %s (%d - %d) in db", username_.c_str(), username_.length(), strlen(username_.c_str()));
    // TODO: handle error!!
    // TODO: timing attack? even if there is no error value, 
    return;
  }

  if (!srps_.InitSession(username_, encodedpassword)) {
    LOG_FATAL("failed to initialize session for %s", username_.c_str());
    // TODO: handle errors!!
    return;
  }

  if (!srps_.FillServerHello(connection->Message())) {
    LOG_FATAL("failed to generate server hello for %s", username_.c_str());
    // TODO: handle errors!!
    return;
  }

  if (!connection->SendMessage()) {
    LOG_DEBUG("send message failed, not setting callbacks, returning.");
    // TODO(protocol): is this the sanest thing we can do?
    return;
  }

  connection->SetCallbacks(&parse_public_key_callback_, &close_callback_);
}

void SrpServerAuthenticator::AuthenticationSession::ParsePublicKeyCallback(
    ServerConnectedSession* connection, OutputCursor* cursor) {
  LOG_DEBUG();

  if (!srps_.ParseClientPublicKey(cursor)) {
    LOG_FATAL("parse public key failed");
    // TODO: handle error
    return;
  }

  if (!srps_.FillServerPublicKey(connection->Message())) {
    LOG_FATAL("fill server public key failed");
    // TODO: handle errors!!
    return;
  }

  ScopedPassword secret;
  srps_.GetPrivateKey(&secret);

  AesSessionKey aeskey(parent_->prng_);
  aeskey.SendSalt(connection->Message());
  if (!connection->SendMessage()) {
    // TODO(protocol): is this the sanest thing we can do?
    LOG_DEBUG("send message failed");
    return;
  }

  // TODO(SECURITY): this MUST happen AFTER client supplied its password, as it's costly.
  // TODO(SECURITY): also, this doesn't seem the best idea. It'd be great if we had something
  // cheap we could use to verify that the client knows the password by doing some costly
  // operation.
  // FIXME: setupkey
  aeskey.SetupKey(secret);

  // TODO(SECURITY,DEBUG): remove this.
  LOG_DEBUG("secret: %s", ConvertToHex(secret.Data(), secret.Used()).c_str());
  AesSessionEncoder* encoder = new AesSessionEncoder(parent_->prng_, aeskey);
  AesSessionDecoder* decoder = new AesSessionDecoder(parent_->prng_, aeskey);

  // TODO(SECURITY): don't initialize the io channel - eg, don't invoke the callback -
  // until first packet. This will increase the cost of a DoS attack.
  (*authentication_done_callback_)(SessionMaybeAuthenticated, encoder, decoder);
}

void SrpServerAuthenticator::AuthenticationSession::CloseCallback(
    ServerConnectedSession* connection, ServerConnectedSession::CloseReason reason) {
  LOG_DEBUG();

  // TODO: log / unregister from parent.
  delete this;
}
