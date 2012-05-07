#ifndef SERVER_AUTHENTICATOR_H
# define SERVER_AUTHENTICATOR_H

# include "base.h"
# include "server-connection-manager.h"

class ServerAuthenticator {
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

  virtual ~ServerAuthenticator() {}
  virtual void StartAuthentication(
      ServerConnectedSession* connection, OutputCursor* cursor,
      authentication_done_handler_t* callback) = 0;

  virtual int GetId() = 0;
};

#endif /* SERVER_AUTHENTICATOR_H */
