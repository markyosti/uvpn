#include "base.h"

#include <string>

#include "dispatcher.h"
#include "interfaces.h"

#include "tun-tap-client-channel.h"
#include "socket-transport.h"
#include "srp-client-authenticator.h"
#include "client-simple-connection-manager.h"
#include "terminal-user-chatter.h"
#include "daemon-controller.h"
#include "daemon-controller-server.h"
#include "uvpn-client.h"

#include "client-udp-transcoder.h"
#include "client-tcp-transcoder.h"

UvpnClient::UvpnClient(ConfigParser* parser)
    : type_(
          parser, Option::Default, "type", "t", "client",
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

void UvpnClient::Run() {
  const string& server("192.168.0.2");

  // Dispatcher: listens on fds, dispatches events.
  Dispatcher dispatcher;
  if (!dispatcher.Init()) {
    LOG_FATAL("could not initialize dispatcher");
    return;
  }

  // Allows to open connections using the socket api.
  SocketTransport socket_api(&dispatcher);
  //SocksTransport socks_api(&dispatcher);
  //ProxyTransport proxy_api(&dispatcher);
  
  // Initialize controller, so uvpn-ctl works.
  AcceptingChannel* channel = DaemonController::Listen(
      &socket_api, "client", "default");
  if (!channel) {
    LOG_FATAL("could not initialize controller");
    return;
  }
  DaemonControllerServer controller;
  controller.Listen(channel);

  NetworkConfig config;

  // Initialize IO channels. Client IO channels wait for packets / requests
  // from the user, and forward them to the server.
  TunTapClientChannel io_tuntap(&dispatcher, &config);
  //ProxyChannel io_proxy(&dispatcher, &socket_api);
  //ProxyChannel io_socks(&dispatcher, &socket_api);

  DefaultPrng prng;

  ClientUdpTranscoder t_udp;
  ClientTcpTranscoder t_tcp;

  // Takes care of performing authentication.
  // We could have multiple, different, authenticator modules.
  SrpClientAuthenticator a_srp(&prng);

  ClientSimpleConnectionManager manager(&prng, &dispatcher);
  controller.AddClient(&manager);

  manager.RegisterTransport(&socket_api);
  //manager.RegisterTransport(&socks_api);
  //manager.RegisterTransport(&proxy_api);

  manager.RegisterIOChannel(&io_tuntap);
  //manager.RegisterIOChannel(&io_proxy);
  //manager.RegisterIOChannel(&io_socks);
 
  //manager.RegisterTranscoder(&t_tcp);
  manager.RegisterTranscoder(&t_udp);

  manager.RegisterAuthenticator(&a_srp);

  // Initialize user chatter.
  TerminalUserChatter chatter;

  // Connect, and perform authentication.
  manager.AddConnection(server, &chatter);
  dispatcher.Start();
}
