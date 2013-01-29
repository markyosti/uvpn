#ifndef CLIENT_IO_CHANNEL_H
# define CLIENT_IO_CHANNEL_H

# include "protector.h"
# include "dispatcher.h"
# include "client-connection-manager.h"

// Notes: this has to handle both the client side and the server side.
// For a tun/tap device, it's fairly obvious how it will work. It's not
// so obvious for a socks or http proxy.

class ClientIOChannel {
 public:
  ClientIOChannel(Dispatcher* dispatcher) {}
  virtual ~ClientIOChannel() {}

  virtual int GetId() = 0;
  virtual bool Init() = 0;

  virtual void HandleSession(ClientConnectedSession* session) = 0;
};

#endif /* CLIENT_IO_CHANNEL_H */
