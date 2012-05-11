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

#include "daemon-controller-server.h"
#include "daemon-controller-common.h"

#include "transport.h"
#include "dispatcher.h"
#include "sockaddr.h"

DaemonControllerServer::DaemonControllerServer(Dispatcher* dispatcher)
    : dispatcher_(dispatcher) {
}

bool DaemonControllerServer::Init(
    Transport* transport, const char* type, const char* name) {
  LOG_DEBUG();

  LocalSockaddr socket(DaemonControllerUtils::MakeName(type, name));
  channel_.reset(transport->DatagramListenOn(socket));
  if (!channel_.get())
    return false;

  // Pseudo code:
  //   - wait for connectins to arrive.
  //   - when they arrive, run the code.

  return true;
}

void DaemonControllerServer::AddClient(ClientConnectionManager* manager) {
  clients_.push_back(manager);
}

void DaemonControllerServer::AddServer(ServerConnectionManager* manager) {
  servers_.push_back(manager);
}
