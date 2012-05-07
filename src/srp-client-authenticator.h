#ifndef SRP_AUTHENTICATOR_H
# define SRP_AUTHENTICATOR_H

# include "authenticator-id.h"
# include "client-authenticator.h"
# include "srp-client.h"
# include "client-transcoder.h"

class ClientIOChannel;

class SrpClientAuthenticator : public ClientAuthenticator {
 public:
  SrpClientAuthenticator(Prng* prng);

  bool StartAuthentication(
      ClientConnectedSession* connection, OutputCursor* cursor,
      UserChatter* chatter, authentication_done_handler_t* callback);

  int GetId() { return AuthenticatorIdSrp; }

 private:
  class AuthenticationSession {
   public:
    AuthenticationSession(
        Prng* prng, const string& username, ClientConnectedSession* connection,
        OutputCursor* cursor, UserChatter* chatter,
        authentication_done_handler_t* callback);
  
   private:
    BigNumberContext cntx_;
    Prng* prng_;
  
    const string username_;

    SrpClientSession session_;
    UserChatter* chatter_;
  
    ClientConnectedSession::read_handler_t server_hello_callback_;
    ClientConnectedSession::read_handler_t server_public_key_callback_;

    authentication_done_handler_t* authentication_done_callback_;
  
    void HelloCallback(
        ClientConnectedSession* connection, OutputCursor* cursor);
    void PublicKeyCallback(
	ClientConnectedSession* connection, OutputCursor* cursor);
  };

  Prng* prng_;
};


#endif /* SRP_AUTHENTICATOR_H */
