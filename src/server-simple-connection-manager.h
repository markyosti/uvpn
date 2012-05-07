#ifndef SERVER_SIMPLE_CONNECTION_MANAGER_H
# define SERVER_SIMPLE_CONNECTION_MANAGER_H

# include "server-connection-manager.h"
# include "server-authenticator.h"
# include "dispatcher.h"

class Prng;

class ServerSimpleConnectionManager : public ServerConnectionManager {
 public:
  explicit ServerSimpleConnectionManager(Prng* prng, Dispatcher* dispatcher);
  virtual ~ServerSimpleConnectionManager() {};

  // Given some ciphertext the client sent us, and a key uniquely identifying
  // the stream of packets, tries to identify the session these packets should
  // go to.
  virtual ServerConnectedSession::State GetSession(
      const ConnectionKey& key, OutputCursor* cursor,
      ServerConnectedSession** session);

  // Creates a new session. This should be invoked if GetSession returned
  // NeedNewSession. connection must be != NULL, CreateSession transfers
  // ownership of the connection to the ServerConnectionManager.
  virtual ServerConnectedSession* CreateSession(
      const ConnectionKey& key, OutputCursor* cursor,
      ServerTranscoder::Connection* connection);

  // Reports an error to the ServerConnectionManager. connection can be NULL, if
  // no connection has been determined yet.
  virtual void HandleError(
      const ConnectionKey& key, ServerTranscoder::Connection* connection,
      const ServerConnectedSession::CloseReason error);

  // Note that the ServerSimpleConnectionManager only supports one channel and one
  // authenticator.
  virtual void RegisterIOChannel(ServerIOChannel* channel);
  virtual void RegisterAuthenticator(ServerAuthenticator* authenticator);

 private:
  class Session : public ServerConnectedSession {
   public:
    Session(ServerSimpleConnectionManager* parent,
	    ServerTranscoder::Connection* connection,
	    ServerAuthenticator* authenticator, ServerIOChannel* channel);

    // Must always return a value, even if the session is not authenticated yet.
    virtual DecodeSessionProtector* GetDecoder();
    virtual EncodeSessionProtector* GetEncoder();
  
    // Called by the transcoder when a new packet was received.
    virtual void HandlePacket(
        const ConnectionKey& key, ServerTranscoder::Connection* connection,
	OutputCursor* data);
    virtual void HandleError(
        const ConnectionKey& key, ServerTranscoder::Connection* connection,
        const CloseReason error);
  
    // Called by the IOChannel or ServerAuthenticator to send a new message
    // it received from the channel.
    virtual InputCursor* Message();
    virtual bool SendMessage();
  
    // Called by the IOChannel or ServerAuthenticator to be notified when data is
    // available for read from a session, or when session is closed.
    virtual void SetCallbacks(read_handler_t*, close_handler_t*);

    // Tells the caller if the connection is ready, or if more data is needed.
    ServerConnectedSession::State IsReady(const ConnectionKey& key, OutputCursor* cursor);

   private:
    void StartAuthenticator(ServerConnectedSession* session, OutputCursor* cursor);
    void AuthenticationDoneHandler(
        ServerAuthenticator::AuthenticationStatus status,
	EncodeSessionProtector* encoder, DecodeSessionProtector* decoder);

    void SetEncoder(EncodeSessionProtector* encoder);
    void SetDecoder(DecodeSessionProtector* decoder);

    void Close();

    ServerSimpleConnectionManager* parent_;

    enum SessionState {
      SessionStateAuthenticationPending,
      SessionStateAuthenticationCompleted
    };

    SessionState state_;

    auto_ptr<EncodeSessionProtector> encoder_;
    auto_ptr<DecodeSessionProtector> decoder_;

    auto_ptr<ServerTranscoder::Connection> connection_;

    ServerAuthenticator* authenticator_;
    ServerIOChannel* channel_;

    read_handler_t authenticator_read_handler_;
    ServerAuthenticator::authentication_done_handler_t
        authentication_done_handler_;

    read_handler_t* read_callback_;
    close_handler_t* close_callback_;
  };

  Prng* prng_;
  Dispatcher* dispatcher_;

  typedef unordered_map<ConnectionKey, Session*> SessionsMap;
  SessionsMap sessions_map_;

  auto_ptr<ServerIOChannel> channel_;
  auto_ptr<ServerAuthenticator> authenticator_;
};

#endif /* SERVER_SIMPLE_CONNECTION_MANAGER_H */
