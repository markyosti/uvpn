#include "packet-queue.h"

PacketQueue::PacketQueue()
  : to_queue_(NULL) {
}

PacketQueue::~PacketQueue() {
  delete to_queue_;
}

size_t PacketQueue::Pending() {
  return queue_.size();
}

Buffer* PacketQueue::ToSend() {
  DEBUG_FATAL_UNLESS(queue_.size())(
    "no message in queue, but request to send?");
  return queue_.front();
}

void PacketQueue::Sent() {
  queue_.pop();
}

InputCursor* PacketQueue::ToQueue() {
  if (!to_queue_)
    to_queue_ = new Buffer;
  return to_queue_->Input();
}

void PacketQueue::Queued() {
  DEBUG_FATAL_UNLESS(to_queue_)(
    "no message to queue, but request to queue?");

  queue_.push(to_queue_);
  to_queue_ = NULL;
}

DatagramSenderPacketQueue::DatagramSenderPacketQueue()
    : write_handler_(bind(&DatagramSenderPacketQueue::HandleWrite, this)),
      channel_(NULL) {
}

DatagramSenderPacketQueue::~DatagramSenderPacketQueue() {
  DropQueue();
}

void DatagramSenderPacketQueue::DropQueue() {
  while (!queue_.empty()) {
    PacketSlot* slot = queue_.front();
    queue_.pop();
    delete slot;
  }
}

void DatagramSenderPacketQueue::SetChannel(DatagramWriteChannel* channel) {
  // TODO: do something smart with the old channel, if any? drop the queue?
  channel_ = channel;
}

DatagramSenderPacketQueue::PacketSlot* DatagramSenderPacketQueue::GetPacketSlot() {
  return new PacketSlot();
}

DatagramWriteChannel::processing_state_e DatagramSenderPacketQueue::HandleWrite() {
  PacketSlot* slot = queue_.front();
  queue_.pop();

  channel_->Write(slot->buffer.Output(), *(slot->address));
  delete slot;

  if (queue_.empty())
    return DatagramWriteChannel::DONE;
  return DatagramWriteChannel::MORE;
}

void DatagramSenderPacketQueue::QueuePacketSlot(PacketSlot* slot) {
  queue_.push(slot);
  channel_->WantWrite(&write_handler_);
}
