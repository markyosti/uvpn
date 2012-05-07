#include "base.h"

#include <string>
#include <memory>

#include "dispatcher.h"

#include "tun-tap-server-channel.h"
#include "socket-transport.h"
#include "srp-server-authenticator.h"
#include "server-udp-transcoder.h"
#include "server-tcp-transcoder.h"
#include "server-simple-connection-manager.h"
#include "interfaces.h"

// So, the client:
//   - creates a dispatcher, transport, and transcoder.
//   - calls authenticate, which starts sending authentication
//     packets by using the scramble session protector.
// The server:
//   - creates a dispatcher, transport, and transcoder.
//   - waits for new authentication requests to arrive.

int main(int argc, char** argv) {
  UdbSecretFile userdb("/root/uvpn.passwd");
  NetworkConfig netconfig;
  
  Dispatcher dispatcher;
  if (!dispatcher.Init()) {
    LOG_FATAL("could not initialize dispatcher");
    return 1;
  }

  SocketTransport socket_api(&dispatcher);

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
}
