#ifndef CLIENT_SIMPLE_CONNECTION_MANAGER_H
# define CLIENT_SIMPLE_CONNECTION_MANAGER_H

# include "base.h"
# include "buffer.h"
# include "dispatcher.h"
# include "prng.h"
# include "password.h"
# include "client-transcoder.h"
# include "client-authenticator.h"
# include "protector.h"
# include "client-connection-manager.h"

# include <map>
# include <memory>
# include <vector>

class UserChatter;
class Prng;

class ClientSimpleConnectionManager : public ClientConnectionManager {
 public:
  ClientSimpleConnectionManager(Prng* prng, Dispatcher* dispatcher);
  ~ClientSimpleConnectionManager() {}

  ClientConnectedSession::State GetSession(
      const ConnectionKey& key, OutputCursor* cursor,
      ClientConnectedSession** session);

  ClientConnectedSession* CreateSession(
      const ConnectionKey& key, OutputCursor* cursor,
      ClientTranscoder::Connection* connection);

  // Handle an error not tied to a particular session. Note that after
  // HandleError has been called, all bets are off, connection might be gone.
  void HandleError(
      const ConnectionKey& key, ClientTranscoder::Connection* connection,
      const ClientConnectedSession::CloseReason error);

  void RegisterTransport(Transport* transport);
  void RegisterIOChannel(ClientIOChannel* channel);
  void RegisterAuthenticator(ClientAuthenticator* authenticator);
  void RegisterTranscoder(ClientTranscoder* transcoder);

  void AddConnection(const string& destination, UserChatter* chatter);

 private:
  class Session : public ClientConnectedSession {
   public:
    Session(ClientSimpleConnectionManager* parent,
	    UserChatter* chatter,
	    ClientTranscoder::Connection* connection,
	    ClientAuthenticator* authenticator, ClientIOChannel* channel);

    // Must always return a value, even if the session is not authenticated yet.
    virtual DecodeSessionProtector* GetDecoder();
    virtual EncodeSessionProtector* GetEncoder();
  
    // Called by the transcoder when a new packet was received.
    virtual void HandlePacket(
        const ConnectionKey& key, ClientTranscoder::Connection* connection,
	OutputCursor* data);
    virtual void HandleError(
        const ConnectionKey& key, ClientTranscoder::Connection* connection,
        const CloseReason error);
  
    // Called by the IOChannel or ClientAuthenticator to send a new message
    // it received from the channel.
    virtual InputCursor* Message();
    virtual bool SendMessage();
  
    // Called by the IOChannel or ClientAuthenticator to be notified when data is
    // available for read from a session, or when session is closed.
    virtual void SetCallbacks(read_handler_t*, close_handler_t*);

    // Tells the caller if the connection is ready, or if more data is needed.
    ClientConnectedSession::State IsReady(const ConnectionKey& key, OutputCursor* cursor);

   private:
    void StartAuthenticator(ClientConnectedSession* session, OutputCursor* cursor);
    void AuthenticationDoneHandler(
        ClientAuthenticator::AuthenticationStatus status,
	EncodeSessionProtector* encoder, DecodeSessionProtector* decoder);

    void SetEncoder(EncodeSessionProtector* encoder);
    void SetDecoder(DecodeSessionProtector* decoder);

    void Close();

    ClientSimpleConnectionManager* parent_;

    enum SessionState {
      SessionStateAuthenticationPending,
      SessionStateAuthenticationCompleted
    };

    SessionState state_;

    auto_ptr<EncodeSessionProtector> encoder_;
    auto_ptr<DecodeSessionProtector> decoder_;

    auto_ptr<ClientTranscoder::Connection> connection_;

    UserChatter* chatter_;
    ClientAuthenticator* authenticator_;
    ClientIOChannel* channel_;

    ClientAuthenticator::authentication_done_handler_t
        authentication_done_handler_;

    read_handler_t* read_callback_;
    close_handler_t* close_callback_;
  };


  Prng* prng_;
  Dispatcher* dispatcher_;

  typedef unordered_map<ConnectionKey, Session*> SessionsMap;
  SessionsMap sessions_map_;

  auto_ptr<ClientIOChannel> channel_;
  auto_ptr<ClientAuthenticator> authenticator_;
//  map<int, ClientIOChannel*> channels_;
//  map<int, ClientAuthenticator*> authenticators_;

  auto_ptr<Transport> transport_;
  auto_ptr<ClientTranscoder> transcoder_;
};

#endif // CLIENT_SIMPLE_CONNECTION_MANAGER_H
