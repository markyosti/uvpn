#ifndef CLIENT_TRANSCODER_H
# define CLIENT_TRANSCODER_H

# include <string>

# include "base.h"
# include "buffer.h"
# include "connection-key.h"

class Transport;
class Sockaddr;
class EncodeSessionProtector;
class DecodeSessionProtector;
class ClientConnectedSession;
class ClientConnectionManager;

// Model:
//   - a transcoder is created.
//   - a transcoder is bound to a single remote end.
//   - a tate machine tells the transcoder how to send traffix.
// How to use aClientTranscoder:
//   - first, you get a buffer, with Message().
//   - then, you call SendMessage(), to send it out.
// Maybe we should create a Message abstraction? Let's
// keep it simple for now.
class ClientTranscoder {
 public:
  class Connection {
   public:
    virtual ~Connection() {}

    // Returns the key of the connection.
    virtual const ConnectionKey& GetKey() const = 0;

    // We want the connection to be closed, before it is deleted.
    virtual void Close() = 0;

    // Invoked by the IOChannel when a message is received from the client.
    // (eg, new packet read from tun/tap device, new request from proxy)
    // The packcet is in cleartext.
    virtual InputCursor* Message() = 0;
    virtual bool SendMessage(
        ClientConnectedSession* session, EncodeSessionProtector* encoder) = 0;
  };

  ClientTranscoder() {}
  virtual ~ClientTranscoder() {}

  virtual Connection* Connect(
      Transport* transport, ClientConnectionManager* manager,
      const Sockaddr& address) = 0;
};

#endif /* CLIENT_TRANSCODER_H */
