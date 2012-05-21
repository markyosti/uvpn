// Copyright (c) 2008,2009,2010,2011 Mark Moreno (kramonerom@gmail.com).
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 
//    1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 
//    2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// 
// THIS SOFTWARE IS PROVIDED BY Mark Moreno ''AS IS'' AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
// EVENT SHALL Mark Moreno OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
// THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// 
// The views and conclusions contained in the software and documentation are
// those of the authors and should not be interpreted as representing official
// policies, either expressed or implied, of Mark Moreno.

#ifndef DAEMON_CONTROLLER_SERVER_H
# define DAEMON_CONTROLLER_SERVER_H

# include "base.h"

# include "ipc-server.h"
# include "ipc/daemon-controller-server.h"
# include "transport.h"

# include <vector>

class Dispatcher;
class Transport;
class ClientConnectionManager;
class ServerConnectionManager; 
class DatagramChannel;

class DaemonControllerServer {
 public:
  DaemonControllerServer();

  void Listen(AcceptingChannel* channel);

  void AddClient(ClientConnectionManager* manager);
  void AddServer(ServerConnectionManager* manager);

 private:
  class Connection : public DaemonControllerServerIpc {
   public:
    Connection(DaemonControllerServer* server, BoundChannel* channel)
        : server_(server), channel_(channel) {}

    void ProcessClientConnectRequest(const string& connect) {
      server_->ProcessClientConnectRequest(connect, this);
    }
  
    void ProcessServerShowClientsRequest() {
      server_->ProcessServerShowClientsRequest(this);
    }

    void ProcessRequestServerStatusRequest(const vector<string>& reply) {}
    void ProcessGetParameterFromUserReply(const vector<string>& reply) {}

   private:
    DaemonControllerServer* server_;
    auto_ptr<BoundChannel> channel_;
  };

  void ProcessClientConnectRequest(const string& connect, Connection* connection);
  void ProcessServerShowClientsRequest(Connection* connection);

  AcceptingChannel::processing_state_e HandleConnection();

  Dispatcher* dispatcher_;
  auto_ptr<AcceptingChannel> channel_; 

  AcceptingChannel::event_handler_t client_connect_handler_;

  vector<ClientConnectionManager*> clients_;
  vector<ServerConnectionManager*> servers_;
};

#endif /* DAEMON_CONTROLLER_SERVER_H */
