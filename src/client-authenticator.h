#ifndef CLIENT_AUTHENTICATOR_H
# define CLIENT_AUTHENTICATOR_H

# include "base.h"
# include "client-connection-manager.h"

class UserChatter;

class ClientAuthenticator {
 public:
  enum AuthenticationStatus {
    // Authentication was not completed yet.
    SessionAuthenticationPending,
    // Session was succesfully authenticated.
    SessionAuthenticated,   
    // Handshake succeeded, but due to how the authenticator works,
    // it doesn't know if authentication succeeded for real until first
    // exchange of packets.
    SessionMaybeAuthenticated,
    // Handshake succeeded, but user is not who he claims to be.
    SessionDenied,          
    // Handshake failed, some protocol level error.
    SessionFailed
  };

  typedef function<void (
      AuthenticationStatus, EncodeSessionProtector*,
      DecodeSessionProtector*)> authentication_done_handler_t;

  virtual ~ClientAuthenticator() {}
  virtual bool StartAuthentication(
      ClientConnectedSession* connection, OutputCursor* cursor,
      UserChatter* chatter, authentication_done_handler_t* callback) = 0;

  virtual int GetId() = 0;
};

#endif /* CLIENT_AUTHENTICATOR_H */
