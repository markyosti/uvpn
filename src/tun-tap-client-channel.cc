#include "tun-tap-client-channel.h"

#include "serializers.h"
#include "interfaces.h"
#include <linux/if_tun.h>
#include <errno.h>

TunTapClientChannel::TunTapClientChannel(
    Dispatcher* dispatcher, NetworkConfig* config)
    : ClientIOChannel(dispatcher),
      dispatcher_(dispatcher),
      network_config_(config),
      server_config_callback_(
        bind(&TunTapClientChannel::ServerConfigCallback, this, placeholders::_1, placeholders::_2)),
      server_packet_callback_(
        bind(&TunTapClientChannel::ServerPacketCallback, this, placeholders::_1, placeholders::_2)),
      tun_tap_read_callback_(bind(&TunTapClientChannel::TunTapReadCallback, this)),
      tun_tap_write_callback_(bind(&TunTapClientChannel::TunTapWriteCallback, this)),
      session_(NULL) {
  LOG_DEBUG();
}

void TunTapClientChannel::ServerConfigCallback(
    ClientConnectedSession* session, OutputCursor* cursor) {
  LOG_DEBUG();

  uint8_t flags;
  if (DecodeFromBuffer(cursor, &flags)) {
    LOG_ERROR("could not read flags");
    // TODO: handle error.
    return;
  }

  // The server will send us a list of (key, value) pairs.
  vector<pair<string, string> > values;
  if (DecodeFromBuffer(cursor, &values)) {
    LOG_ERROR("could not read configs");
    // TODO: handle error.
    return;
  }

  auto_ptr<IPAddress> client_address;
  auto_ptr<IPAddress> server_address;
  for (unsigned int i = 0; i < values.size(); ++i) {
    const string& name(values[i].first);
    const string& value(values[i].second);

    if (name == "CLIENT_ADDRESS") {
      client_address.reset(IPAddress::Parse(value));
      continue;
    } 

    if (name == "SERVER_ADDRESS") {
      server_address.reset(IPAddress::Parse(value));
      continue;
    }
  }

  if (!client_address.get()) {
    LOG_ERROR("server did not provide a CLIENT_ADDRESS");
    // TODO: handle errors.
  }

  if (!server_address.get()) {
    LOG_ERROR("server did not provide a SERVER_ADDRESS");
    // TODO: handle errors.
  }

  if (!device_.Open((flags & USE_TAP) ? IFF_TAP : IFF_TUN)) {
    LOG_ERROR("could not open tun/tap device");
    // TODO: error! retry later? really return false? what can the caller do?
    // let the caller reschedule for later? seems just a way to move complexity
    // around and confuse things even further.
    return;
  }

  // Setup interface.
  auto_ptr<Interface> interface(network_config_->GetInterface(device_.Name().c_str()));
  if (!interface.get()) {
    LOG_ERROR("interface %s does not exist??", device_.Name().c_str());
    // TODO: handle errors!
    return;
  }

  if (!interface->Enable()) {
    // TODO: handle errors!
    LOG_ERROR("could not enable interface %s", device_.Name().c_str());
    return;
  }

  if (!interface->AddAddress(*client_address, client_address->Length())) {
    // TODO: handle errors!
    LOG_ERROR("unable to add address");
    return;
  }

  if (!network_config_->GetRoutingTable()->AddRoute(
         *server_address, server_address->Length(), NULL,
	 client_address.get(), interface.get())) {
    // TODO: handle errors!
    LOG_ERROR("unable to add route");
    return;
  }

  // TODO: add default route? we need at least one more route here.
  // TODO: set a close callback handler!
  session->SetCallbacks(&server_packet_callback_, NULL);
  dispatcher_->AddFd(device_.Fd(), Dispatcher::READ, &tun_tap_read_callback_, NULL);

  // TODO: if we don't send any form of ack, the server will never know that
  // we got the config... so, it will start sending packets immediately after.
  // If we get packets in the wrong order, we'll have some mess here.
  // (do we care? will the protocol recover by itself? if yes, this might be moot)
  // (does it even make sense to handle this case? not much we can do with invalid
  // packets until we receive the configuration...)
  // (note that the server is unlikely to *send* us any packets on his own initiative
  // until we start sending packets...)
  // (how do we prove that the config packet is indeed a config packet?)

  // TODO: run process!!! to set networking parameters in the mean time!
  // Start listening for tun tap events.
  return;
}

void TunTapClientChannel::TunTapWriteCallback() {
  LOG_DEBUG();

  if (tun_tap_queue_.Pending()) {
    if (!device_.Write(tun_tap_queue_.ToSend()->Output())) {
      LOG_PERROR("write error: %s", strerror(errno));
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

void TunTapClientChannel::TunTapReadCallback() {
  LOG_DEBUG();

  // TODO: 4096 should be enough, but maybe we should (1) have this
  // configurable or (2) figure it out some way. (or fragment, or set
  // MTU when configuring interface, or...).
  if (!device_.Read(session_->Message(), kMaxPacketSize)) {
    LOG_ERROR("read failed");
    // TODO: handle errors.
    return;
  }

  session_->SendMessage();
}

void TunTapClientChannel::ServerPacketCallback(
    ClientConnectedSession* session, OutputCursor* cursor) {
  LOG_DEBUG();

  // TODO: there's a copy that we could avoid here, if we exported a better
  // interface than the current one.
  // TODO: we need to propagate slowness upstream. What if we have way too
  // many packets queued? right now, this is invisible to the caller.
  EncodeToBuffer(cursor, tun_tap_queue_.ToQueue());
  tun_tap_queue_.Queued();

  // Be lazy, install the write handler only if we have packets to send.
  if (tun_tap_queue_.Pending() == 1) {
    dispatcher_->SetFd(device_.Fd(), Dispatcher::WRITE, Dispatcher::NONE,
                       NULL, &tun_tap_write_callback_);
  }
}

void TunTapClientChannel::Start(ClientConnectedSession* session) {
  LOG_DEBUG();

  session_ = session;

  // Prepare to receive data back from the server.
  // TODO: move this before Open, and add retry logic!
  // TODO: set a close callback handler!
  session->SetCallbacks(&server_config_callback_, NULL);
}
