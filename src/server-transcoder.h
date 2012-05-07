#ifndef SERVER_TRANSCODER_H
# define SERVER_TRANSCODER_H

# include <string>

# include "base.h"
# include "buffer.h"

class EncodeSessionProtector;
class DecodeSessionProtector;
class ServerConnectedSession;

// Model
//   - Server transcoder invokes a callback every time a new
//     connection is created.
//   - Server transcoder will need to keep some state per connection.
//     (eg, a file descriptor, buffer? something else?)
//   - whatever is handling the connection will need to keep some state
//     as well.
//   - server transcoder will need to invoke some callbacks per connection.
//     In particular, it will need to call:
//       - some HandleRead, when new data is available.
//       - some HandleClose, when connection is broken, for some reason.
//   - additionally, each connection can have a different protector.
class ServerTranscoder {
 public:
  class Connection {
   public:
    virtual ~Connection() {}

    // We want the connection to be closed, before it is deleted.
    virtual void Close() = 0;

    // Data added in the message input cursor is encrypted/compressed/obfuscated
    // before being sent out.
    virtual InputCursor* Message() = 0;

    // Data added in the header input cursor is sent out as is, before anything
    // else. This is generally used for initial handshakes. The endpoint needs
    // to process this data before the transport starts processing the rest of
    // the data.
    virtual InputCursor* Header() = 0;

    // Note that when sending a message, the message can belong to any session.
    // Connection manager is supposed to let us know which session and encoder
    // the message belongs to. If anything, for error reporting.
    virtual bool SendMessage(
        ServerConnectedSession* session, EncodeSessionProtector* encoder) = 0;
  };

  ServerTranscoder() {}
  virtual ~ServerTranscoder() {}
  virtual bool Start() = 0;
};

#endif /* SERVER_TRANSCODER_H */
