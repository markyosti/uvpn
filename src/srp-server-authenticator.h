#ifndef SRP_SERVER_AUTHENTICATOR_H
# define SRP_SERVER_AUTHENTICATOR_H

# include "authenticator-id.h"
# include "server-authenticator.h"
# include "srp-server.h"
# include "scramble-session-protector.h"
# include "server-transcoder.h"

class SrpServerAuthenticator : public ServerAuthenticator {
 public:
  SrpServerAuthenticator(UserDb* userdb, Prng* prng);

  void StartAuthentication(
      ServerConnectedSession* connection, OutputCursor* cursor,
      authentication_done_handler_t* callback);

  int GetId() { return AuthenticatorIdSrp; }

 private:
  class AuthenticationSession {
   public:
    AuthenticationSession(
        SrpServerAuthenticator* parent, 
        authentication_done_handler_t* callback);
    ~AuthenticationSession();

    void StartAuthentication(ServerConnectedSession*, OutputCursor*);

   private:
    void ParsePublicKeyCallback(ServerConnectedSession*, OutputCursor*);
    void CloseCallback(ServerConnectedSession*, ServerConnectedSession::CloseReason);

    SrpServerAuthenticator* parent_;
    SrpServerSession srps_;
    string username_;

    authentication_done_handler_t* authentication_done_callback_;

    ServerConnectedSession::read_handler_t parse_hello_callback_;
    ServerConnectedSession::read_handler_t parse_public_key_callback_;
    ServerConnectedSession::close_handler_t close_callback_;
  };

  void ConnectCallback(EncodeSessionProtector* encoder,
		       DecodeSessionProtector* decoder,
		       ServerConnectedSession* session);

  Prng* prng_;
  BigNumberContext cntx_;
  UserDb* userdb_;
};

#endif /* SRP_SERVER_AUTHENTICATOR_H */
