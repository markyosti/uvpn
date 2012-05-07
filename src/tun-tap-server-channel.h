#ifndef TUN_TAP_SERVER_CHANNEL_H
# define TUN_TAP_SERVER_CHANNEL_H

# include "io-channel-id.h"
# include "server-io-channel.h"
# include "ip-manager.h"
# include "tun-tap-common.h"
# include "dispatcher.h"
# include "packet-queue.h"
# include "interfaces.h"
# include "server-connection-manager.h"

class TunTapServerChannel : public ServerIOChannel {
 public:
  TunTapServerChannel(Dispatcher* dispatcher, NetworkConfig* config);
  virtual ~TunTapServerChannel() {}

  bool Init();
  void HandleConnect(ServerConnectedSession* session);
  int GetId() { return IoChannelIdTunTap; }

 private:
  class Session {
   public:
    static const int kMaxPacketSize = 4096;

    Session(Dispatcher* dispatcher, ServerConnectedSession* session,
	    IPAddress* server_address, IPManager* manager, NetworkConfig* config);
    ~Session();

   private:
    void ClientReadCallback(
        ServerConnectedSession* session, OutputCursor* cursor);
    void ClientCloseCallback(
        ServerConnectedSession* session, ServerConnectedSession::CloseReason reason);

    void TunTapReadCallback();
    void TunTapWriteCallback();

    ServerConnectedSession::read_handler_t client_read_callback_;
    ServerConnectedSession::close_handler_t client_close_callback_;

    Dispatcher::event_handler_t tun_tap_read_callback_;
    Dispatcher::event_handler_t tun_tap_write_callback_;

    ServerConnectedSession* session_;
    Dispatcher* dispatcher_;

    PacketQueue tun_tap_queue_;
    TunTapDevice device_;

    IPManager* ip_manager_;
    IPAddress* client_address_;
    Interface* interface_;
  };

  Dispatcher* dispatcher_;
  NetworkConfig* network_config_;

  IPRange valid_range_;

  auto_ptr<IPManager> ip_manager_;
  // TODO: need to free this address from the destructor.
  IPAddress* local_address_;
};

#endif /* TUN_TAP_SERVER_CHANNEL_H */
