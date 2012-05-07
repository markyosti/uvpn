#ifndef CLIENT_CONNECTION_MANAGER_H
# define CLIENT_CONNECTION_MANAGER_H

# include "buffer.h"
# include "prng.h"
# include "password.h"
# include "client-transcoder.h"
# include "protector.h"

# include "connection-key.h"

# include <memory>

class Transport;
class ClientAuthenticator;
class ClientIOChannel;
class ConnectionKey;
class UserChatter;

// The ClientConnectionManager:
//   - opens one or more sessions with an uvpn server,
//     possibly of the same kind.
//
// Model:
//   - we register some Transcoders with the ConnectionManager.
//   - as per configurations, the ConnectionManager calls Connect
//     on some of the Transcoders in order to try to connect.
//   - the Transcoders return a ClientTranscoder::Connection.
//
// Let's say we have a client that wants to connect to multiple
// servers. Do we create a ConnectionManager for each connection?
// or one ConnectionManager in total? Does the client need a
// ConnectedSession, or can that be kept private to the
// ConnectionManager?
//
// Plan:
//   - one ClientConnectionManager per uvpn client instance,
//     keeps track of all channels / transcoders / available.
//   - client calls the Connect() method, possibly with parameters
//     for the ConnectionManager to use.
//   - a ClientConnectedSession is instantiated:
//     - IOChannels calls Message() and SendMessage. The ClientConnectedSession
//       picks the best transcoder to use, and sends messages using that
//       transcoder.
//     - when a message is received by any of the transcoders, the
//       ClientConnectedSession invokes the callback, which most likely
//       ends up calling the same IOChannel.
//
// Can we setup multiple IOChannels at the same time? it should be possible.
// Setup of IOChannels should be hidden by the interface. 

class ClientConnectedSession {
 public:
  virtual ~ClientConnectedSession() {}

  enum CloseReason {
    Shutdown,  // client voluntarily closed the connection.
    Truncated, // data was unexpectedly truncated.
    Timeout,   // some timer expired, with expected activity not taking place.
    Manager,   // some error detected by the manager.
    Encoding,  // some encoding error.
    Decoding,  // some decryption / decoding error.
    System     // some error we cannot really identify.
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
  virtual ClientConnectedSession::State IsReady(
      const ConnectionKey& key, OutputCursor* cursor) = 0;

  // Called by the transcoder when a new packet is received.
  virtual void HandlePacket(
      const ConnectionKey& key, ClientTranscoder::Connection* connection,
      OutputCursor* data) = 0;
  // Note that after HandleError has been called, all bets are off,
  // connection might have been deleted.
  virtual void HandleError(
      const ConnectionKey& key, ClientTranscoder::Connection* connection,
      const CloseReason error) = 0;

  // Called by the IOChannel or ClientAuthenticator to send a new message
  // it received from the channel.
  virtual InputCursor* Message() = 0;
  virtual bool SendMessage() = 0;

  // Called by the IOChannel or ClientAuthenticator to be notified when data is
  // available for read from a session, or when session is closed.
  typedef function<void (ClientConnectedSession*, OutputCursor*)> read_handler_t;
  typedef function<void (ClientConnectedSession*, CloseReason)> close_handler_t;
  virtual void SetCallbacks(read_handler_t*, close_handler_t*) = 0;
};

class ClientConnectionManager {
 public:
  virtual ~ClientConnectionManager() {}

  virtual ClientConnectedSession::State GetSession(
      const ConnectionKey& key, OutputCursor* cursor,
      ClientConnectedSession** session) = 0;

  virtual ClientConnectedSession* CreateSession(
      const ConnectionKey& key, OutputCursor* cursor,
      ClientTranscoder::Connection* connection) = 0;

  // Handle an error not tied to a particular session. Note that after
  // HandleError has been called, all bets are off, connection might be gone.
  virtual void HandleError(
      const ConnectionKey& key, ClientTranscoder::Connection* connection,
      const ClientConnectedSession::CloseReason error) = 0;

  // There can be multiple ways to connect to a remote end: we could have
  // direct connectivity, a socks server, a proxy server, or whatever else
  // is necessary to send packets out. Note that transports might not be all
  // known before a Connect is started. Eg, proxy discovery might take time
  // to complete.
  virtual void RegisterTransport(Transport* transport) = 0;

  // There could be multiple ways to get the traffic from the user:
  // we could setup a proxy server on some port, a socks server, a tun tap
  // device, some funky iptables rule to intercept packets, ... whatever. 
  virtual void RegisterIOChannel(ClientIOChannel* channel) = 0;

  // Again, we could have multiple authentication mechanisms.
  virtual void RegisterAuthenticator(ClientAuthenticator* authenticator) = 0;

  virtual void RegisterTranscoder(ClientTranscoder* transcoder) = 0;

  virtual void Connect(const string& destination, UserChatter* chatter) = 0;
};


#endif // CLIENT_CONNECTION_MANAGER_H
