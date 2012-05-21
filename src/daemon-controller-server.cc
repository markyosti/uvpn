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

#include "buffer.h"
#include "serializers.h"
#include "daemon-controller-server.h"
#include "ipc-server.h"
#include "ipc/daemon-controller-server.h"
#include "daemon-controller.h"

#include "transport.h"
#include "dispatcher.h"
#include "sockaddr.h"

DaemonControllerServer::DaemonControllerServer()
    : client_connect_handler_(bind(&DaemonControllerServer::HandleConnection, this)) {
}

void DaemonControllerServer::Listen(AcceptingChannel* channel) {
  channel_.reset(channel);
  channel->WantConnection(&client_connect_handler_);
}

AcceptingChannel::processing_state_e
    DaemonControllerServer::HandleConnection() {
  BoundChannel* channel(channel_->AcceptConnection(NULL));
  if (!channel) {
    LOG_PERROR("error while accepting new connection");
    return AcceptingChannel::MORE;
  }
  LOG_DEBUG("accepted new connection");

  Connection* connection = new Connection(this, channel);
  // TODO: track connections somewhere.
  return AcceptingChannel::MORE;
}

void DaemonControllerServer::ProcessClientConnectRequest(
    const string& connect, DaemonControllerServer::Connection* connection) {
}

void DaemonControllerServer::ProcessServerShowClientsRequest(
    DaemonControllerServer::Connection* connection) {
}

void DaemonControllerServer::AddClient(ClientConnectionManager* manager) {
  clients_.push_back(manager);
}

void DaemonControllerServer::AddServer(ServerConnectionManager* manager) {
  servers_.push_back(manager);
}
