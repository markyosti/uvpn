#include "srp-client-authenticator.h"

#include <memory>

#include "aes-session-protector.h"
#include "user-chatter.h"
#include "conversions.h"
#include "client-transcoder.h"
#include "terminal.h"
#include "client-io-channel.h"

SrpClientAuthenticator::SrpClientAuthenticator(Prng* prng)
  : prng_(prng) {
}

bool SrpClientAuthenticator::StartAuthentication(
    ClientConnectedSession* connection, OutputCursor* cursor,
    UserChatter* chatter, authentication_done_handler_t* callback) {
  StaticBuffer username;
  if (!chatter->Get(UserChatter::kUserName, &username)) {
    // TODO: handle errors!
    LOG_ERROR("could not get username");
    return false;
  }

  new AuthenticationSession(
      prng_, username.AsString(), connection, cursor, chatter, callback);
  // TODO: track sessions in some useful way!
 
  return true;
}

SrpClientAuthenticator::AuthenticationSession::AuthenticationSession(
    Prng* prng, const string& username, ClientConnectedSession* connection,
    OutputCursor* cursor, UserChatter* chatter,
    authentication_done_handler_t* callback)
    : prng_(prng),
      session_(prng_, &cntx_, username),
      chatter_(chatter),
      server_hello_callback_(bind(
          &SrpClientAuthenticator::AuthenticationSession::HelloCallback, this, placeholders::_1, placeholders::_2)),
      server_public_key_callback_(bind(
          &SrpClientAuthenticator::AuthenticationSession::PublicKeyCallback, this, placeholders::_1, placeholders::_2)),
      authentication_done_callback_(callback) {
  session_.FillClientHello(connection->Message());

  // TODO: instead of using a scrambler, maybe send a hash of the username?
  // yes, it would be "vulnerable" to rainbow attacks, but strong enough
  // to protect against casual evasedropping. In any case, sensibly stronger
  // than the scrambler protector.
  connection->SetCallbacks(&server_hello_callback_, NULL);
  connection->SendMessage();
}

void SrpClientAuthenticator::AuthenticationSession::HelloCallback(
    ClientConnectedSession* connection, OutputCursor* cursor) {
  LOG_DEBUG("parsing hello");

  OutputCursor parsed(*cursor);
  int missing = session_.ParseServerHello(&parsed);
  if (missing < 0) {
    // TODO: handle errors.
    LOG_FATAL("parse server hello failed");
    return;
  }

  if (missing > 0) {
    // TODO: handle need more data.
    LOG_ERROR("need more data");
    return;
  }

  session_.FillClientPublicKey(connection->Message());
  // TODO: do something if connection is closed!
  connection->SetCallbacks(&server_public_key_callback_, NULL);
  connection->SendMessage();
}

void SrpClientAuthenticator::AuthenticationSession::PublicKeyCallback(
    ClientConnectedSession* connection, OutputCursor* cursor) {
  LOG_DEBUG("parsing public key");

  int result = session_.ParseServerPublicKey(cursor);
  if (result < 0) {
    // TODO: handle errors.
    LOG_FATAL("invalid server public key");
    return;
  }
  if (result > 0) {
    // TODO: handle need more data.
    LOG_ERROR("need more data");
    return;
  }

  // TODO: either get the password asynchronously, or ... don't block here anyway.
  ScopedPassword password;
  if (!chatter_->Get(UserChatter::kPassword, &password)) {
    // TODO: handle errors in a useful way.
    LOG_FATAL("could not read password");
    return;
  }

  // TODO(SECURITY,DEBUG): remove this.
  LOG_DEBUG("password: %.*s", password.Used(), password.Data());

  ScopedPassword key;
  if (!session_.GetPrivateKey(password, &key)) {
    LOG_FATAL("could not get private key");
    // TODO: handle errors.
    return;
  }

  // TODO(SECURITY,DEBUG): remove this.
  LOG_DEBUG("secret: %s", ConvertToHex(key.Data(), key.Used()).c_str());

  AesSessionKey aeskey(prng_);
  if (!aeskey.RecvSalt(cursor)) {
    LOG_FATAL("could not setup key");
    // TODO: handle errors.
    return;
  }

  aeskey.SetupKey(key);
  AesSessionEncoder* encoder(new AesSessionEncoder(prng_, aeskey));
  AesSessionDecoder* decoder(new AesSessionDecoder(prng_, aeskey));

  (*authentication_done_callback_)(SessionMaybeAuthenticated, encoder, decoder);
}
