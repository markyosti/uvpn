#ifndef SERVER_CRYPTO_CONNECTION_MANAGER_H
# define SERVER_CRYPTO_CONNECTION_MANAGER_H

# include "server-connection-manager.h"
# include "server-authenticator.h"
# include "dispatcher.h"

// Data structures:
//   - each session is identified by multiple SIDs.
//   - each SID is throw away: once used, we discard it.
//   - packets belonging to a single session can be sent over multiple
//     channels, and replies can be received over multiple channels as
//     well.
// 
// About channels:
//   - some channels are monodirectional -> eg, can send data, but not receive
//     it.
//   - some channels are bidirectional -> can send and receive data.
//     bidirectional channels can be:
//     - reply based -> one end can only send replies to requests
//     - query based -> both ends can send data to each other freely
//
// With IPv4 and NAT, monodirectional channels can generally be used from
// client to server only. With evolution of IPv6, and for hosts with public
// IPv4 addresses, we will be able to freely use monodirectional channles.
//
// When we send a packet, we need to send it over one or more channels. We then
// need to determine which one is the best channel, and keep using it for as
// long as it keeps working reliably.
//
// If some other channels become more reliable, we should use them instead. We
// could also spread the load evenly over multiple channels, if that proves to
// bring advantages.
//
// Channels may have different behaviors depending on data: larger packets may
// be easier to send through a tcp stream rather than a DNS transport.
// We should be wary of introducing packet reordering in addition to the packet
// reordering naturally introduced by the stream.
//
// Algorithm:
//   - client tries several and multiple channels to connect with server.
//   - at startup, it sends all packets through ?all? channels to the server? or at least,
//     scores the possible channels, and sends packets in parallel through the first n channels.
//
//   - the server gets the packets, now it has to send replies. It doesn't know which channels
//     work, and which ones will be best.
//   - uses same algorithm as the client: the inbound channels that are
//     bidirectional are added to the scored list of candidates. As more data is
//     determined about the channel, the score is adjusted.
//   - note: for reply based protocols, we need to have a way to tell the client we
//     need to send more packets, and to control the influx of requests.
//
// Pseudo code:
//   - sort possible channels to use by likelyhood or preference (allow command
//     line options or database for best/preferred methods).
//   - x methods at a time, send the same packets to the server. Each method
//     is identified by an 8 bit channel id.
//
//   - to evaluate each channel, we need to know: packet loss, latency, and
//     corruption. Those signals can change depending on the direction of the
//     channel.
//     - to measure latency, we need a timer to go back from server to client.
//       this timer is going to be biased by the mechanisms used to send the
//       reply back.
//
// What makes a GOOD CHANNEL:
//   - low latency (Lowest Latency Channel - LLC)
//   - low packet loss (Lowest Packet Loss Channel - LPLC)
//   - low corruption rate (Lowest Corruption Channel - LCC)
//   - low reordering (Lowest Reordering Channel - LRC)
//
// RTT:
//   - client wants to know fastest channel C->S
//   - server wants to know fastest channel S->C
//
//   Simple approach:
//   - save time packet is sent, save time packet is received.
//     difference is RTT, RTT includes CS + SC time.
//   - on the server, measure DELTA in receiving packets from
//     channels. Eg, save timestamp of first packet received,
//     express all other timestamps as DELTA from that timestamp.
//     Report DELTA to client. Idea, also consider processing
//     time taken by server? sounds reasonable.
//   - send timings back to the client. Client now knows:
//     - RTT (measured locally), for each request it sent.
//
// Points:
//   - only the receiver can tell which channel is best for the
//     sender to use, in terms of performance.
//
// Each packet sent has:
//   - SID -> global, each packet we send has a different SID coming
//     from the same space. 128 bits.
//     Purpose: identifies the session.
//   - PID -> packet id, global, each packet we send has a unique identifier,
//     which is incremented for each packet.
//     Purpose: de-duping of packets. Same packet sent over multiple channels,
//     packet reordering across channels.
//
//   - CID -> channel packet id, per channel.
//     Purpose: detect packet loss within the channel.
//   - STS -> start time stamp, time the packet was sent.
//     Purpose: on reception, for the same PID, the endpoint
//       can determine which channel delivered the packet faster. See A-LLC.
//
//   - Packets can have:
//     - UPL - user pay load, a packet that needs to be decapsulated and
//             forwarded.
//     - SPL - stats pay load. Statistics about the channel and connections.
//     - RPL - reliability pay load. Fragmentation / ordering / windowing
//             data, so the protocol can be made reliable.
//     - CPL - control pay load. Other control information, like the one
//             used to work in proxy mode.
//
// Each packet received will be associated a:
//   - RTS -> receive time stamp, time the packet was received.
//     Purpose: determine the latency of channels, see A-LLC.
//
// Algorithm to evaluate Lowest Latency Channel (A-LLC):
//   - for each PID, we store the STS and RTS of the first PID received.
//     Clearly, this is the packet that made it faster to us. Let's
//     call this packet 0.
//   - when we receive a packet for the same PID, we need to keep into
//     account:
//     - send time, might have been sent later by remote client.
//     - receive time, clearly that indicates how later we received
//       the packet.
//     - the relation between send time and receive time is unknown.
//   - DELTA LATENCY, or DL, is defined as:
//       DL = (RTS[n] - RTS[0]) - (STS[n] - STS[0])
//     A positive delta latency indicates that the channel picked is
//     slower than the first channel. A negative delta latency indicates
//     that it is faster.
//   - Could an attacker influence the algorithm? YES, by delaying packets.
//     Assuming all channels provide the same security gurantees,
//     by picking the channel that's going faster we're doing the
//     right thing. (delayed or not purposedly, we give the best
//     connection we can to the user), assuming all channels have
//     the same security guarantees.
//
// Algorithm to determine Lowest Corruption Channel (A-LCC):
//   - Use SID to identify session.
//   - Just count packets that arrive corrupted.
//   - Could an attacker influence the algorithm? YES:
//     1 - he is unlikely to be able to generate corrupted
//       packets with a valid SID unless he can obeserve
//       (or control) the communication channel.
//     2 - if he can observe the communication channel, he
//       can race with us in generating valid SIDs with
//       garbage.
//     3 - if he can control the communication channel, he
//       can corrupt the packets.
//     In 1 and 2, attacker is unlikely to be able to 
//     create damage.
//     In 3, if an attacker can control and corrupt the
//     channel, it's a good idea to try different channels. 
//   - What if the SID is corrupted wrong?
//     1 - we will see packet loss on the channel.
//     2 - packet loss on the channel should cause the
//         channel to be unpreffed.
//     3 - when evaluating the channel, we should take that
//         part into account.
//     Problem with corrupted SID is that those packets can't
//     be tied to a channel, so can hardly be tied to a client.
//     Maybe keep that information per end host? eg, per network?
//
// Point:
//   - ability to communicate from client to server and back
//     is affected by the networks being traversed, and by the
//     middle men being used (eg, a specific recursive resolver).
//     We should keep some information about the sender network
//     to score the channel.
//
// Algorithm to find Lowest Packet Loss Channel (A-LPLC) and
//   Lowest Reordering Channel (A-LRC):
//
// Notes:
//   - using multiple channels will introduce reordering.
//   - we have a few problems to address here:
//     - some protocols don't care about reordering, are tolerant
//       to packet loss, and / or don't care about either. Some
//       protocols have built in recovery. To avoid issues like
//       the TCP meltdown, we should let the encapsulated protocol
//       recover from packet loss. Should we also let the
//       encapsulated stream recover from packet reordering by itself?
//       - Reordering streams means that we have to buffer later
//         packets until an earlier packet is received -> artificially
//         introducing latency. Earlier packet may have been lost.
//       - protocol may try to recover by itself in the mean time.
//       - some protocols (including TCP) handle reordering
//         poorly.
//       - decision: don't do reordering by default? measure
//         impact of algorithms on performance. How long of a
//         wait can we tolerate in our own buffers before it
//         affects TCP? Proposal:
//         - we can determine the RTT perceived by the end user,
//           by measuring RTT across channels. We can make
//           an approximate guess at the RTO that TCP will
//           compute.
//         - keep the re-ordering buffer to half that RTO? 
//           This could also help papering over packet loss,
//           at least in those cases where data is sent over
//           multiple channels.
//
//     - presenting a stream that looks as clean as we can
//       to the end application - if we introduce reordering,
//       we should address it.
//
//   - A packet is lost if ... it doesn't make it to the receiver.
//     a packet can't be considered lost if it arrives after
//     the following packet. Could be an effect of packet
//     reordering. Normally, a packet is considered lost if
//     it doesn't make it to the server within the expected
//     timeframe (http://www.ietf.org/rfc/rfc2988.txt, see
//     how RTO is computed for TCP). Notes:
//     - RTO relies on knowing the RTT. RTT is heavily influenced
//       by the Return path time, which changes a lot depending
//       on the return channel picked by the sender. 
//     - we can know if our timers are wrong. Eg, if we receive
//       packets after we've expired them, or way before we expetcted
//       them.
//
//   - SIDs are use once and throw away. Receiver has a
//     small window to determine and expire SIDs. We could
//     consider a packet lost when we have to throw away
//     a SID, but when we throw away a SID, we don't know 
//     which channel the packet was sent through, so we
//     can't blame a specific channel.
//   - Proposals:
//     - on receiver, use the CID described above. When
//       we see a packet with higher CID for a particular
//       channel, we know the previous packet has either
//       been lost, or sent out of order.
//    
// Overall algorithm:
//   - when sending a packet, we need to determine:
//     - how many channels to use
//     - of the possible channels, which ones to use
//     - additionally, we need to pull other channels
//       in from time to time to measure their efficiency.
//
//   - properties of channels are <sender, receiver> generally.
//     At startup, determine the network I'm in. Look up the
//     database to use. If no database found, use default
//     database. Improvement: do longest prefix match,
//     aggregate at some point up in the hirearchy.
// 
//
// Proposed packet header:
//
// struct ClearTextHeader {
//   uint128_t sid;  // Session ID, crypto token.
//   char top_header[];
// };
//
// struct TopHeader {
//   uint8_t flags;         // Flags associated with this packet.
//     HAS_USER_PAYLOAD,    // It has a packet from a user that needs being forwarded.
//     HAS_STATS_PAYLOAD,   // It has statistics about _inbound_ channels.
//     HAS_CONTROL_PAYLOAD, // It has control data, needed by some other module.
//
//   uint8_t channel;  // Identifies the channel from sender perspective.
//
//   uint48_t pid;     // Identifies this packet within the session,
//                     // monotonically incremented for each packet,
//                     // across all channels. Assuming a 10Gb/s network, and 1
//                     // bytes packets, before repeating:
//                     // ((2^48 * 1) / ((10 * 2^30) / 8)) / (60 * 60) =~ 58 hrs.
//                     // As networks improve, we expect pipes to get fatter, allowing
//                     // more pps to be sent through the network. 40 bits would probably
//                     // be enough here for a long time, given a packet size that's
//                     // generally in the order of ~60 bytes.
//
//   uint32_t sts;     // Start time stamp, "time" the transaction was
//                     // started, in us. Using 32 bits allows ~1hr of delta to be
//                     // expressed, which is significantly more than we ever
//                     // expect a packet to be delayed for.
//                     // We have about 5us of latency per km due to speed of light,
//                     // 1ms is ~200km of latency. The purpose of this timestamp is
//                     // to respond both to delays introduced by links (ms) and
//                     // by the software stack and protocol handling (can range from
//                     // us to ms kind of delay).
//                     // Any sort of wireless network can introduce variance in the
//                     // order of 10s of ms for each packet, which will need to be
//                     // taken into account.
//                     // As networks and computers get better, we expect variance in
//                     // latency to go down similarly to latency introduced by the
//                     // softare stack.
//
//
//   uint32_t cid;     // Uniquely identifies this packet within the stream.
//                     // Monotonically inceased within the channel.
//
//
//   char payload[];
// }
//
// struct StatsPayload {
//   struct StatsPayloadEntry {
//     uint8_t channel;
//     uint48_t last_pid;
//     uint8_t loss;
//                                // 24 bits: 1 for packet that is missing, 0 otherwise. 
//   } entry[1];
// };
//
// // Used to hold and pass unspecificed control data. 
// struct ControlPayload {
//   struct ControlPayloadEntry {
//     uint8_t length;    // length == 0 marks end of entries, length includes type.
//                        // When lenght is 0, there is no type afterward.
//     uint8_t type;
//   } entry[1];
// };
//   
//
class Prng;

class ServerCryptoConnectionManager : public ServerConnectionManager {
 public:
  explicit ServerCryptoConnectionManager(Prng* prng, Dispatcher* dispatcher);
  virtual ~ServerCryptoConnectionManager() {};

  // Given some ciphertext the client sent us, and a key uniquely identifying
  // the stream of packets, tries to identify the session these packets should
  // go to.
  virtual ServerConnectedSession::State GetSession(
      const ConnectionKey& key, OutputCursor* cursor,
      ServerConnectedSession** session);

  // Creates a new session. This should be invoked if GetSession returned
  // NeedNewSession. connection must be != NULL, CreateSession transfers
  // ownership of the connection to the ServerConnectionManager.
  virtual ServerConnectedSession* CreateSession(
      const ConnectionKey& key, OutputCursor* cursor,
      ServerTranscoder::Connection* connection);

  // Reports an error to the ServerConnectionManager. connection can be NULL, if
  // no connection has been determined yet.
  virtual void HandleError(
      const ConnectionKey& key, ServerTranscoder::Connection* connection,
      const ServerConnectedSession::CloseReason error);

  // Note that the ServerCryptoConnectionManager only supports one channel and one
  // authenticator.
  virtual void RegisterIOChannel(ServerIOChannel* channel);
  virtual void RegisterAuthenticator(ServerAuthenticator* authenticator);

 private:
  class Session : public ServerConnectedSession {
   public:
    Session(ServerCryptoConnectionManager* parent,
	    ServerTranscoder::Connection* connection,
	    ServerAuthenticator* authenticator, ServerIOChannel* channel);

    // Must always return a value, even if the session is not authenticated yet.
    virtual DecodeSessionProtector* GetDecoder();
    virtual EncodeSessionProtector* GetEncoder();
  
    // Called by the transcoder when a new packet was received.
    virtual void HandlePacket(
        const ConnectionKey& key, ServerTranscoder::Connection* connection,
	OutputCursor* data);
    virtual void HandleError(
        const ConnectionKey& key, ServerTranscoder::Connection* connection,
        const CloseReason error);
  
    // Called by the IOChannel or ServerAuthenticator to send a new message
    // it received from the channel.
    virtual InputCursor* Message();
    virtual bool SendMessage();
  
    // Called by the IOChannel or ServerAuthenticator to be notified when data is
    // available for read from a session, or when session is closed.
    virtual void SetCallbacks(read_handler_t*, close_handler_t*);

    // Tells the caller if the connection is ready, or if more data is needed.
    ServerConnectedSession::State IsReady(const ConnectionKey& key, OutputCursor* cursor);

   private:
    void StartAuthenticator(ServerConnectedSession* session, OutputCursor* cursor);
    void AuthenticationDoneHandler(
        ServerAuthenticator::AuthenticationStatus status,
	EncodeSessionProtector* encoder, DecodeSessionProtector* decoder);

    void SetEncoder(EncodeSessionProtector* encoder);
    void SetDecoder(DecodeSessionProtector* decoder);

    void Close();

    ServerCryptoConnectionManager* parent_;

    enum SessionState {
      SessionStateAuthenticationPending,
      SessionStateAuthenticationCompleted
    };

    SessionState state_;

    auto_ptr<EncodeSessionProtector> encoder_;
    auto_ptr<DecodeSessionProtector> decoder_;

    auto_ptr<ServerTranscoder::Connection> connection_;

    ServerAuthenticator* authenticator_;
    ServerIOChannel* channel_;

    read_handler_t authenticator_read_handler_;
    ServerAuthenticator::authentication_done_handler_t
        authentication_done_handler_;

    read_handler_t* read_callback_;
    close_handler_t* close_callback_;
  };

  Prng* prng_;
  Dispatcher* dispatcher_;

  typedef unordered_map<ConnectionKey, Session*> SessionsMap;
  SessionsMap sessions_map_;

  auto_ptr<ServerIOChannel> channel_;
  auto_ptr<ServerAuthenticator> authenticator_;
};

#endif /* SERVER_CRYPTO_CONNECTION_MANAGER_H */
