#ifndef SERVER_CONNECTION_MANAGER_H
# define SERVER_CONNECTION_MANAGER_H

# include "buffer.h"
# include "prng.h"
# include "password.h"
# include "server-transcoder.h"
# include "protector.h"

# include "connection-key.h"

# include <memory>

class ServerIOChannel;
class ServerAuthenticator;

class ServerConnectedSession {
 public:
  virtual ~ServerConnectedSession() {}

  enum CloseReason {
    Shutdown,  //< client voluntarily closed the connection.
    Truncated, //< data was unexpectedly truncated.
    Timeout,   //< some timer expired, with expected activity not taking place.
    Manager,   //< some error detected by the manager.
    Encoding,  //< some encoding error.
    Decoding,  //< some decryption / decoding error.
    System     //< some error we cannot really identify.
  };

  enum State {
    NeedMoreData,
    InvalidData,
    NeedNewSession,
    Ready
  };

  // Must always return a value, even if the session is not authenticated yet.
  virtual DecodeSessionProtector* GetDecoder() = 0;
  virtual EncodeSessionProtector* GetEncoder() = 0;

  // Determines if the session is ready to accept more data or not.
  virtual ServerConnectedSession::State IsReady(
      const ConnectionKey& key, OutputCursor* cursor) = 0;

  // Called by the transcoder when a new packet is received.
  virtual void HandlePacket(
      const ConnectionKey& key, ServerTranscoder::Connection* connection,
      OutputCursor* data) = 0;
  // Note that after HandleError has been called, all bets are off,
  // connection might have been deleted.
  virtual void HandleError(
      const ConnectionKey& key, ServerTranscoder::Connection* connection,
      const CloseReason error) = 0;

  // Called by the IOChannel or ServerAuthenticator to send a new message
  // it received from the channel.
  virtual InputCursor* Message() = 0;
  virtual bool SendMessage() = 0;

  // Called by the IOChannel or ServerAuthenticator to be notified when data is
  // available for read from a session, or when session is closed.
  typedef function<void (ServerConnectedSession*, OutputCursor*)> read_handler_t;
  typedef function<void (ServerConnectedSession*, CloseReason)> close_handler_t;
  virtual void SetCallbacks(read_handler_t*, close_handler_t*) = 0;
};

class ServerConnectionManager {
 public:
  virtual ~ServerConnectionManager() {}

  //! Checks the data provided by the client (cursor) and key and returns the
  //! ServerConnectedSession object corresponding to this client.
  //!
  //! If no session is found, or the method needs more data from the client,
  //! or it is invalid, the corresponding error is returned.
  //!
  //! GetSession will leave the cursor unchanged unless Ready is returned.
  //! If a new session needs to be created, the caller is responsible for
  //! invoking CreateSession with the proper connection object.
  //!
  //! GetSession is separate from CreateSession to allow transcoders to
  //! lazily create connection objects until after the need has been verified.
  virtual ServerConnectedSession::State GetSession(
      const ConnectionKey& key, OutputCursor* cursor,
      ServerConnectedSession** session) = 0;

  virtual ServerConnectedSession* CreateSession(
      const ConnectionKey& key, OutputCursor* cursor,
      ServerTranscoder::Connection* connection) = 0;

  // Handle an error not tied to a particular session. Note that after
  // HandleError has been called, all bets are off, connection might be gone.
  virtual void HandleError(
      const ConnectionKey& key, ServerTranscoder::Connection* connection,
      const ServerConnectedSession::CloseReason error) = 0;

  // There could be multiple ways to get the traffic from the user:
  // we could setup a proxy server on some port, a socks server, a tun tap
  // device, some funky iptables rule to intercept packets, ... whatever. 
  virtual void RegisterIOChannel(ServerIOChannel* channel) = 0;

  // Again, we could have multiple authentication mechanisms.
  virtual void RegisterAuthenticator(ServerAuthenticator* authenticator) = 0;
};


#endif // SERVER_CONNECTION_MANAGER_H
