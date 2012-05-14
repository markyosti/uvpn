// Copyright (c) 2008,2009,2010,2011 Mark Moreno (kramonerom@gmail.com).
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 
//    1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 
//    2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// 
// THIS SOFTWARE IS PROVIDED BY Mark Moreno ''AS IS'' AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
// EVENT SHALL Mark Moreno OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
// THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// 
// The views and conclusions contained in the software and documentation are
// those of the authors and should not be interpreted as representing official
// policies, either expressed or implied, of Mark Moreno.

#include "ipc-client.h"

IpcClientInterface::IpcClientInterface(BoundChannel* channel)
    : channel_(channel),
      read_handler_(bind(&IpcClientInterface::HandleRead, this)),
      write_handler_(bind(&IpcClientInterface::HandleWrite, this)) {
}

void IpcClientInterface::Start() {
  channel_->WantRead(&read_handler_);

  // TODO: Really? not sure we should do this at Start.
  read_buffer_.Input()->Reserve(8192);
}

BoundChannel::processing_state_e IpcClientInterface::HandleRead() {
  BoundChannel::io_result_e status(channel_->Read(read_buffer_.Input()));
  if (status != BoundChannel::OK) {
    // TODO: handle errors!
  }

  // Parse as many requests received as we can. We might want to add code
  // here so we avoid starving other components.
  while (1) {
    LOG_DEBUG("processing input");

    OutputCursor parsing(*read_buffer_.Output());
    int result = Dispatch(&parsing);
    if (result < 0) {
      LOG_DEBUG("dispatch returned < 0, %d", result);
      // TODO: drop connection? really, there's not that much we can do here.
    }
    if (result > 0) {
      LOG_DEBUG("dispatch returned > 0, %d", result);
      read_buffer_.Input()->Reserve(result);
      break;
    }

    read_buffer_.Output()->Increment(
        read_buffer_.Output()->LeftSize() - parsing.LeftSize());
  }

  return BoundChannel::MORE;
}

BoundChannel::processing_state_e IpcClientInterface::HandleWrite() {
  channel_->Write(write_buffer_.Output());
  if (write_buffer_.Output()->LeftSize())
    return BoundChannel::MORE;
  return BoundChannel::DONE;
}

void IpcClientInterface::Send() {
  channel_->WantWrite(&write_handler_);
}
