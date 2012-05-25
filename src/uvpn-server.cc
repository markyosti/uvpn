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

#include "base.h"

#include <string>
#include <memory>

#include "uvpn-server.h"
#include "dispatcher.h"

#include "tun-tap-server-channel.h"
#include "socket-transport.h"
#include "srp-server-authenticator.h"
#include "server-udp-transcoder.h"
#include "server-tcp-transcoder.h"
#include "server-simple-connection-manager.h"
#include "interfaces.h"
#include "daemon-controller.h"
#include "daemon-controller-server.h"

UvpnServer::UvpnServer(ConfigParser* parser)
    : type_(
          parser, Option::Default, "type", "t", "server",
          "Each uvpn runs multiple processes to provide vpn "
          "connections. Tipically, you will have a 'client' process and "
          "a 'server' process. By using this option, you can start uvpn "
          "processes in some custom role. You don't normally need to "
          "change it, but if you do, remember that you also need to "
          "pass it to uvpn-ctl."),
      name_(
          parser, Option::Default, "name", "n", "default",
          "If you run multiple uvpn 'server's or multiple uvpn 'client's "
          "(see the --type option), you need each server or each client "
          "to have a different name. With this option, you can specify "
          "the name of the uvpn instance to launch. You don't normally need "
          "to specify this option, but if you do, remember that you also need "
          "to pass it to uvpn-ctl.") {
}

int UvpnServer::Run() {
  Dispatcher dispatcher;
  if (!dispatcher.Init()) {
    LOG_FATAL("could not initialize dispatcher");
    return 1;
  }
  SocketTransport socket_api(&dispatcher);

  // Initialize controller, so uvpn-ctl works.
  AcceptingChannel* channel = DaemonController::Listen(
      &socket_api, "server", "default");
  if (!channel) {
    LOG_FATAL("could not initialize controller");
    return 2;
  }
  DaemonControllerServer controller;
  controller.Listen(channel);

  UdbSecretFile userdb("/root/uvpn.passwd");
  NetworkConfig netconfig;

  // Initialize IO channels. Server IO channels expect packets / requests
  // from the users, interpret them, and forward them.
  TunTapServerChannel io_tuntap(&dispatcher, &netconfig);
  if (!io_tuntap.Init()) { // TODO: move this somewhere else?
    LOG_FATAL("could not initialize tun-tap-server-channel");
    return 1;
  }

  //ProxyChannel io_proxy(&dispatcher, &socket_api);
  //SocksChannel io_socks(&dispatcher, &socket_api);

  // Initialize ConnectionManager.
  DefaultPrng prng;
  ServerSimpleConnectionManager manager(&prng, &dispatcher);
  controller.AddServer(&manager);

  // Initialize authenticators.
  SrpServerAuthenticator auth_srp(&userdb, &prng);
  manager.RegisterIOChannel(&io_tuntap);
  //manager.RegisterIOChannel(&io_proxy);
  manager.RegisterAuthenticator(&auth_srp);

  // Initialize transcoders.
  auto_ptr<Sockaddr> listen(Sockaddr::Parse("0.0.0.0", 1029));
  ServerUdpTranscoder t_udp(
      &dispatcher, &socket_api, *listen, &manager);
  ServerTcpTranscoder t_tcp(&socket_api, *listen, &manager);

  // TODO: both Start() can return errors. We should probably let the
  // manager handle starting the transcoders.
  t_udp.Start();
  t_tcp.Start();
  dispatcher.Start();

  return 0;
}
