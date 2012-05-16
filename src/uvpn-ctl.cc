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

#include "uvpn-ctl.h"

#include "daemon-controller.h"
#include "ipc-client.h"
#include "ipc/daemon-controller-client.h"

UvpnCtl::UvpnCtl(ConfigParser* parser)
    : type_(
          parser, Option::Default, "type", "t", "client",
          "Each yovpn instance runs multiple processes to handle vpn "
          "connections. Tipically, you will have a 'client' process and "
          "possibly a 'server' process, but it depends on how you configured "
          "yovpn to start."),
      name_(
          parser, Option::Default, "name", "n", "default",
          "On a single machine you can have multiple instances of yovpn "
          "running, each one with its own set of processes, and each one "
          "with its own name. With this option, you can specify which "
          "yovpn instance you want to control. "
          "If you did not change the default name and only run one yovpn, "
          "you don't need to change this option."),
      cmd_server_(
          parser, Command::Default, "server",
          "Inspect or change configuration of a server."),
      cmd_server_show_clients_(
          &cmd_server_, Command::Default, "show-clients",
          "List the current clients connected to the server.",
          bind(&UvpnCtl::CommandServerShowClients, this)),
      cmd_server_show_users_(
          &cmd_server_, Command::Default, "show-users",
          "List the current users connected to the server."),
      cmd_server_save_state_(
          &cmd_server_, Command::Default, "save-state",
          "Save the state of the server, restore it at startup."),

      cmd_client_(
          parser, Command::Default, "client",
          "Inspect or change configuration of a client."),
      cmd_client_connect_(
          &cmd_client_, Command::Default, "connect",
          "Connect to a specific server.",
          bind(&UvpnCtl::CommandClientConnect, this)),
      cmd_client_disconnect_(
          &cmd_client_, Command::Default, "disconnect",
          "Connect to a specific server."),
      cmd_client_save_state_(
          &cmd_client_, Command::Default, "save-state",
          "Save the state of the client, restore it at startup."),

      opt_server_(
          NULL, Option::Default, "server", "s", "",
          "Name of the server to connect/disconnect to."),

      transport_(&dispatcher_),
      client_(&dispatcher_) {
  opt_server_.AddTo(&cmd_client_connect_);
  opt_server_.AddTo(&cmd_client_disconnect_);
}

void UvpnCtl::CommandServerShowClients() {
  LOG_DEBUG();
}

void UvpnCtl::CommandClientConnect() {
  LOG_DEBUG();
  client_.SendRequestClientConnect(opt_server_.Get());
}

bool UvpnCtl::Init() {
  if (!dispatcher_.Init())
    return false;

  DaemonController controller;
  BoundChannel* channel = controller.Connect(
      &transport_, type_.Get().c_str(), name_.Get().c_str());
  if (!channel)
    return false;

  client_.Connect(channel);

  dispatcher_.Start();
  return true;
}
