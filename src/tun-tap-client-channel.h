#ifndef TUN_TAP_CLIENT_CHANNEL_H
# define TUN_TAP_CLIENT_CHANNEL_H

# include "io-channel-id.h"
# include "client-io-channel.h"
# include "dispatcher.h"
# include "tun-tap-common.h"

# include "client-transcoder.h"
# include "client-connection-manager.h"
# include "packet-queue.h"
# include "interfaces.h"

# include <vector>
# include <memory>

class TunTapClientChannel : public ClientIOChannel {
 public:
  static const int kMaxPacketSize = 4096;

  typedef enum {
    USE_TAP = BIT(0) // if not set, use TUN instead.
  } server_flags_e;

  TunTapClientChannel(Dispatcher* dispatcher, NetworkConfig* config);
  bool Init();
  int GetId() { return IoChannelIdTunTap; }

  void HandleSession(ClientConnectedSession* session);
 
 private:
  class Session {
   public:
    Session(Dispatcher* dispatcher, ClientConnectedSession* session,
            NetworkConfig* config);

   private:
    // Handle configuration packet coming from the server.
    void ServerConfigCallback(ClientConnectedSession* session, OutputCursor* cursor);
    // Handle normal packet coming from the server.
    void ServerPacketCallback(ClientConnectedSession* session, OutputCursor* cursor);
  
    void TunTapReadCallback();
    void TunTapWriteCallback();

    Dispatcher* dispatcher_;
    NetworkConfig* network_config_;

    ClientConnectedSession::read_handler_t server_config_callback_;
    ClientConnectedSession::read_handler_t server_packet_callback_;
  
    Dispatcher::event_handler_t tun_tap_read_callback_;
    Dispatcher::event_handler_t tun_tap_write_callback_;
  
    auto_ptr<EncodeSessionProtector> encoder_;
    auto_ptr<DecodeSessionProtector> decoder_;
    ClientConnectedSession* session_;
  
    TunTapDevice device_;
    vector<pair<string, string> > variables_;
  
    PacketQueue tun_tap_queue_;
  };

  Dispatcher* dispatcher_;
  NetworkConfig* network_config_;
};

#endif /* TUN_TAP_CLIENT_CHANNEL_H */
