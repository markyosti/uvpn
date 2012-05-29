#ifndef SERVER_IO_CHANNEL_H
# define SERVER_IO_CHANNEL_H

# include "dispatcher.h"
# include "protector.h"
# include "server-connection-manager.h"

class ServerIOChannel {
 public:
  ServerIOChannel(Dispatcher* dispatcher) {}
  virtual ~ServerIOChannel() {}

  virtual bool Init() = 0;
  virtual int GetId() = 0;

  virtual void HandleConnect(ServerConnectedSession* session) = 0;
};

#endif /* IO_CHANNEL_H */
