#ifndef PACKET_QUEUE_H
# define PACKET_QUEUE_H

// Basic abstraction to keep a queue of packets / buffers.
# include <queue>
# include "buffer.h"
# include "transport.h"

class Sockaddr;
class SessionProtector;
class Dispatcher;

class DatagramSenderPacketQueue {
 public:
  // TODO: instead of Sockaddr, use a BoundWriteChannel?
  struct PacketSlot {
    PacketSlot() : address(NULL) {}
    Sockaddr* address;
    Buffer buffer;
  };

  // TODO: this has to be limited in size.
  DatagramSenderPacketQueue();
  ~DatagramSenderPacketQueue();
  
  void SetChannel(DatagramWriteChannel*);
  PacketSlot* GetPacketSlot();
  void QueuePacketSlot(PacketSlot*);

 private:
  void DropQueue();
  DatagramWriteChannel::processing_state_e HandleWrite();
  DatagramWriteChannel::event_handler_t write_handler_;

  DatagramWriteChannel* channel_;

  // TODO: this is likely to be costly in the end run. Use a freelist
  // or something equivalent.
  queue<PacketSlot*> queue_;
};

class PacketQueue {
 public:
  PacketQueue();
  virtual ~PacketQueue();

  size_t Pending();

  // Returns a cursor to the current packet to send.
  Buffer* ToSend();
  void Sent();

  // Returns a cursor in the current packet that's being built.
  InputCursor* ToQueue();
  void Queued();

 private:
  queue<Buffer*> queue_;
  Buffer* to_queue_;
};

#endif /* PACKET_QUEUE_H */
