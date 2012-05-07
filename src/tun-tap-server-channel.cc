#include "tun-tap-server-channel.h"
#include "tun-tap-client-channel.h"

#include "serializers.h"
#include "interfaces.h"

#include <linux/if_tun.h>

TunTapServerChannel::TunTapServerChannel(Dispatcher* dispatcher, NetworkConfig* config)
    : ServerIOChannel(dispatcher),
      dispatcher_(dispatcher),
      network_config_(config) {
  LOG_DEBUG();
}

bool TunTapServerChannel::Init() {
  // TODO: allow user to override manually configured ip range.
  if (!IPManager::FindUsableRange(network_config_->GetRoutingTable(), &valid_range_)) {
    LOG_ERROR("could not allocate an usable ip range");
    return false;
  }

  ip_manager_.reset(new IPManager(valid_range_));

  local_address_ = ip_manager_->AllocateIP();
  if (!local_address_) {
    LOG_ERROR("range is too small, or something weird.");
    return false;
  }
  
  LOG_INFO("tunnels will use %s as endpoint", local_address_->AsString().c_str());
  return true;
}

void TunTapServerChannel::HandleConnect(ServerConnectedSession* session) {
  LOG_DEBUG();
  new Session(dispatcher_, session, local_address_, ip_manager_.get(), network_config_);
}

TunTapServerChannel::Session::Session(
    Dispatcher* dispatcher, ServerConnectedSession* session, IPAddress* server_address,
    IPManager* manager, NetworkConfig* network_config)
    : client_read_callback_(bind(&TunTapServerChannel::Session::ClientReadCallback, this,
	  		    placeholders::_1, placeholders::_2)),
      client_close_callback_(bind(&TunTapServerChannel::Session::ClientCloseCallback, this,
			     placeholders::_1, placeholders::_2)),
      tun_tap_read_callback_(bind(&TunTapServerChannel::Session::TunTapReadCallback, this)),
      tun_tap_write_callback_(bind(&TunTapServerChannel::Session::TunTapWriteCallback, this)),
      session_(session),
      dispatcher_(dispatcher),
      ip_manager_(manager),
      client_address_(NULL) {
  LOG_DEBUG();

  session->SetCallbacks(&client_read_callback_, &client_close_callback_);

  // TODO: TUN or TAP? depends on config...
  uint8_t flags = IFF_TUN;
  if (!device_.Open(IFF_TUN)) {
    LOG_ERROR("unable to open tun tap device");
    // TODO: handle error! What do we do here?
  }

  // TODO: add support for invoking an external configuration script to set
  // the interfaces up, or at least add hooks so the user can setup firewalling or
  // whatever he likes.

  client_address_ = ip_manager_->AllocateIP();
  if (!client_address_) {
    LOG_ERROR("unable to allocate ip address");
    // TODO: handle error! What do we do here?
  }

  // TODO: do we really need to store the interface descriptor in the
  // session? probably not. We should have a better API to manage interfaces,
  // the current one sucks badly.
  interface_ = network_config->GetInterface(device_.Name().c_str());
  if (!interface_->Enable()) {
    LOG_ERROR("unable to enable interface");
    // TODO: handle error!
  }

  if (!interface_->AddAddress(*server_address, server_address->Length())) {
    LOG_ERROR("unable to add address");
    // TODO: handle error!
  }

  if (!network_config->GetRoutingTable()->AddRoute(
          *client_address_, client_address_->Length(), NULL,
	  server_address, interface_)) {
    LOG_ERROR("unable to set routing table correctly");
    // TODO: handle error!
  }

  if (flags == IFF_TUN) {
    flags = 0;
  } else {
    // FIXME: tap support is ... lacking.
    flags = TunTapClientChannel::USE_TAP;
    LOG_FATAL("need better tap support");
  }

  // Send server configuration packet.
  EncodeToBuffer(flags, session->Message());

  vector<pair<string, string> > values;
  values.push_back(make_pair("SERVER_ADDRESS", server_address->AsString()));
  values.push_back(make_pair("CLIENT_ADDRESS", client_address_->AsString()));

  EncodeToBuffer(values, session->Message());
  if (!session->SendMessage()) {
    // TODO: handle errors!
    return;
  }
  
  dispatcher_->AddFd(device_.Fd(), Dispatcher::READ, &tun_tap_read_callback_, NULL);
}

TunTapServerChannel::Session::~Session() {
  if (client_address_)
    ip_manager_->ReturnIP(client_address_);
  delete interface_;
}

void TunTapServerChannel::Session::TunTapWriteCallback() {
  LOG_DEBUG();

  if (tun_tap_queue_.Pending()) {
    if (!device_.Write(tun_tap_queue_.ToSend()->Output())) {
      LOG_PERROR("recv error");
      // TODO: handle errors!
      return;
    }

    tun_tap_queue_.Sent();
  }

  if (!tun_tap_queue_.Pending()) {
    dispatcher_->SetFd(device_.Fd(), Dispatcher::NONE,
		       Dispatcher::WRITE, NULL, NULL);
  }
}

void TunTapServerChannel::Session::TunTapReadCallback() {
  LOG_DEBUG();

  // TODO: 4096 should be enough, but maybe we should (1) have this
  // configurable or (2) figure it out some way. (or fragment, or set
  // MTU when configuring interface, or...).
  if (!device_.Read(session_->Message(), kMaxPacketSize)) {
    LOG_ERROR("read failed");
    // TODO: handle errors.
    return;
  }

  if (!session_->SendMessage()) {
    // TODO: handle errors!
    return;
  }
}

void TunTapServerChannel::Session::ClientReadCallback(
    ServerConnectedSession* session, OutputCursor* cursor) {
  LOG_DEBUG();

  // This notifies us of stuff ready on the udp socket from the client.
  // We need to:
  //   - read it out the cursor. 
  //   - queue it to be written out the tun_tap_device.
  EncodeToBuffer(cursor, tun_tap_queue_.ToQueue());
  tun_tap_queue_.Queued();

  // Be lazy, install the write handler only if we have packets to send.
  if (tun_tap_queue_.Pending() == 1) {
    dispatcher_->SetFd(device_.Fd(), Dispatcher::WRITE, Dispatcher::NONE,
                       NULL, &tun_tap_write_callback_);
  }
}

void TunTapServerChannel::Session::ClientCloseCallback(
    ServerConnectedSession* session, ServerConnectedSession::CloseReason reason) {
  LOG_DEBUG();

  // TODO: there's much more to be done! (reclaim the ip? deconfigure tun/tap device, ...)
  delete this;
}
