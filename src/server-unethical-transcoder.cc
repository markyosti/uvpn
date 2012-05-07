#include "server-unethical-transcoder.h"

#include "server-tcp-transcoder.h"
#include "server-udp-transcoder.h"

// Why does a transcoder need a protector? because it needs to encode/decode
// encrypted data, like the length of the packet.

ServerUnethicalTranscoder::ServerUnethicalTranscoder(
    Dispatcher* dispatcher, Transport* transport, const Sockaddr& address,
    ServerTranscoder::protector_generator_t* generator)
    : dispatcher_(dispatcher),
      transport_(transport),
      application_generator_(generator),
      transcoder_connect_handler_(bind(
          &ServerUnethicalTranscoder::ConnectCallback, this,
	  placeholders::_1, placeholders::_2, placeholders::_3)) {
}

void ServerUnethicalTranscoder::GenerateProtector(
    EncodeSessionProtector** encoder, DecodeSessionProtector** decoder) {
  // Short story:
  //   - we need to provide a protector generator to other ServerTranscoders.
  //   - those transcoders will generate a new set of protectors before
  //     invoking the connect callback.
  //   - the connect callback is usually an authenticator, that adds a layer
  //     of encryption on the channel. It needs an existing encoder / decoder
  //     to be able to decode the existing data.
  //   - the unethical transcoder register its own callback, as a new connection
  //     could be a real new connection, or part of an existing one.
  //     So, no need to generate a protector here.
  *encoder = NULL;
  *decoder = NULL;
}

void ServerUnethicalTranscoder::Start(connect_handler_t* handler) {
  // Notes:
  //   - handler is the authentication handler.
  //   - needs to be called with the current encoder, decoder.
  //
  // Pseudo code:
  //   - depending on config, start all the transcoders that we need.
  //   - provide a callback
  auto_ptr<Sockaddr> listen(Sockaddr::Parse("0.0.0.0", 1029));

  ServerUdpTranscoder* udp = new ServerUdpTranscoder(
      dispatcher_, transport_, *listen, application_generator_);
  udp->Start(&transcoder_connect_handler_);
  transcoders_.push_back(udp);

  ServerTcpTranscoder* tcp = new ServerTcpTranscoder(
      dispatcher_, transport_, *listen, application_generator_);
  tcp->Start(&transcoder_connect_handler_);
  transcoders_.push_back(tcp);

  // Other transcoders:
  //   - tcp over port 80
  //   - http over port 80
  //   - tcp over port 443
  //   - real tls over port 443
  //   - udp over port 53
  //   - dns over port 53
  //   - sctp? other crazy protocols?
  //   - ack tcp packets?
  //   - other raw protocol?

  // Other things to do here:
  //   - is there a proxy? can we detect one?
  //   - tcp halts with ~3% packet loss. Can we avoid this
  //     when using udp / sctp / packet oriented transcoders?
  //     fountain codes, reed solomon, ...
}

void ServerUnethicalTranscoder::ConnectCallback(
    EncodeSessionProtector* encoder, DecodeSessionProtector* decoder,
    ServerTranscoderSession* session) {
  // Notes:
  //   - udp calls the ConnectCallback every time it sees a packet with
  //     unknown src ip / port tuple. This is because we don't want to
  //     re-authenticate for every packet.
  //   - tcp calls the ConnectCallback every time there is a new connection.
  //   - tcp / udp call the ReadHandler with the decoded packet. But they
  //     call the encoder / decoder correctly.
  //   - the authenticator callback calls the channel connector if connection
  //     is completed.
  // Pseudo code:
  //   - register a read handler.
  // Caller:
  //   - invokes generator, creates an encoder and decoder. Currently, uvpn-server
  //     provides a generator that creates a scramble session encoder / decoder.
  //   - invokes this callback, providing encoder, decoder, and session, which
  //     it just created, representing the connection with the client.
  //     The usual callback is the SRP authenticator callback is invoked, which:
  //       - creates a new authentication session.
  //       - calls SetProtector on the session (using the generators supplied).
  //       - calls SetCallbacks on the session (which register the callback to handle packets).

  // Normally, an authenticator would register a ConnectCallback that performs
  // authentication. With the unethical transcoder, a new physical connection
  // (eg, a new tcp stream, or a new udp stream) can be part of an existing
  // connection, and we won't know until we get some data out of the connection.
  // Also, the physical media needs to encrypt and decrypt data. So we (1) need
  // to provide an universal decoder / encoder, and (2) we need to provide some
  // sort of smart callback.

  // Now, as soon as the underlying transcoder tries to interpret newly read
  // data, it will need to decode it (remember: metadata is also encrypted).
  session->SetProtector(&connection_dispatcher_encoder_,
			&connection_dispatcher_decoder_);
}

ServerUnethicalTranscoder::ConnectionDispatcherDecoder::ConnectionDispatcherDecoder() {
}

ServerUnethicalTranscoder::ConnectionDispatcherDecoder::~ConnectionDispatcherDecoder() {
}

ServerUnethicalTranscoder::ConnectionDispatcherDecoder::Result
ServerUnethicalTranscoder::ConnectionDispatcherDecoder::Start(
    OutputCursor* input, InputCursor* output, StartOptions options) {
  SessionProtector::Result result;
  ServerConnection* connection = server_connection_state_.GetConnection(input, &result);
  if (result != SessionProtector::SUCCEEDED)
    return result;

  // IDEA:
  //   - read 8 bytes of connection identifier.
  //   - now if the connection exists, we're fine.

  if (!connection) {
    // is this the start of a new connection? perform authentication.
    // is this invalid?
  }



  // Pseudo code:
  //   - find the connection
  //     - is this a known connection?
  //       - associate the real encoder / decoder.
  //     - is this the start of a new connection?
  //       - perform authentication.
  //     - is this invalid?
  //     
  return CORRUPTED_DATA;
}

ServerUnethicalTranscoder::ConnectionDispatcherDecoder::Result
ServerUnethicalTranscoder::ConnectionDispatcherDecoder::Continue(
    OutputCursor* input, InputCursor* output, uint32_t until) {
  return CORRUPTED_DATA;
}

ServerUnethicalTranscoder::ConnectionDispatcherDecoder::Result
ServerUnethicalTranscoder::ConnectionDispatcherDecoder::End(
    OutputCursor* input, InputCursor* output) {
  return CORRUPTED_DATA;
}

ServerUnethicalTranscoder::ConnectionDispatcherDecoder::Result
ServerUnethicalTranscoder::ConnectionDispatcherDecoder::RemovePadding(
    OutputCursor* output, int datasize, uint8_t* padsize) {
  return CORRUPTED_DATA;
}

# if 0
if (!connection_manager_->GetConnection(packet.Output(), &cid, &connection)) {
    // * if packet.HasPayload():
    // *   this is NSK, requesting a server key.
    // * else:
    // *   this is WSK, performing authentication.
    //
    // Idea:
    //   * pass packet to the ServerAuthenticator, by
    //     simply invoking the ConnectCallback?
    //
    //   * ConnectCallback, should determine what to do
    //     with the packet. (the if above).
    //
    //   * Outcome is:
    //     * we send the key back to the user (NSK.1)
    //       nothing is done to the connection table.
    //     * we have an entry to add to the connection
    //       table. The ConnectCallback should provide us
    //       with some sort of CidTracker. Or at least with
    //       a sensible ClientConnectionState object.

    // Pseudo code:
    //   - prepare ServerTranscoderSession
    //   - prepare ClientConnectionState
    //   - call connection_manager_.AddConnection for the next X cids.
    if (client_connect_callback_)
      // FIXME!
      (*client_connect_callback_)(NULL, NULL);

    ClientConnectionState* connection = new ClientConnectionState();
    connection_manager_->AddConnection(NULL, connection);
  } else {
    // This is an existing connection.
    // Pseudo code:
    //   - decode packet, at least the part we care about.
    //   - is this a fragment? feed it to the reassembler.
    //   - do we have all fragments? invoke the read callback.  
    //   - is it invalid? ignore it. Otherwise, consume the CID.

    Buffer decoded;
    ClientConnectionState::consume_status_e status;
    status = connection->ConsumePacket(packet.Output(), decoded.Input());
    if (status == ClientConnectionState::INVALID) {
      // TODO: handle error
      return DatagramChannel::MORE;
    }
  }
#endif

