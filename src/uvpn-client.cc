#include "base.h"

#include <string>

#include "dispatcher.h"
#include "interfaces.h"

#include "tun-tap-client-channel.h"
#include "socket-transport.h"
#include "srp-client-authenticator.h"
#include "client-simple-connection-manager.h"
#include "terminal-user-chatter.h"

#include "client-udp-transcoder.h"
#include "client-tcp-transcoder.h"

int main(int argc, char** argv) {
  const string& server("192.168.0.2");

  NetworkConfig config;

  // Dispatcher: listens on fds, dispatches events.
  Dispatcher dispatcher;
  if (!dispatcher.Init()) {
    LOG_FATAL("could not initialize dispatcher");
    return 1;
  }

  // Allows to open connections using the socket api.
  SocketTransport socket_api(&dispatcher);
  //SocksTransport socks_api(&dispatcher);
  //ProxyTransport proxy_api(&dispatcher);

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
  manager.Connect(server, &chatter);
  dispatcher.Start();
}
