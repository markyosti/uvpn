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

#ifndef STATIC_KEY_H
# define STATIC_KEY_H

//! An arbitrary blob of data usable as a key in an unordered map.
//!
//! StaticKey can hold any amount of data up to SIZE, and provides
//! functors such as the StaticKey can be used in unordered maps.
//!
//! @param SIZE How much data to hold at most.
template<int SIZE>
class StaticKey {
 public:
  typedef StaticKey<SIZE> static_key_t;
  static const uint16_t kBufferSize = SIZE;

  StaticKey() : size_(0) {}

  void Add(const char* data, uint16_t size) {
    DEBUG_FATAL_UNLESS(size_ + size <= kBufferSize && size_ + size >= size_)(
        "can only store kBufferSize bytes in key, %d", kBufferSize);
    memcpy(buffer_ + size_, data, size);
    size_ = static_cast<uint16_t>(size_ + size);
  }

  uint16_t Size() const { return size_; }
  const char* Buffer() const { return buffer_; }

 private:
  char buffer_[kBufferSize];
  uint16_t size_;
};

// Define functors that will be automatically picked up by STL
// containers to handle addresses.
FUNCTOR_EQ(
  template<int SIZE>
  struct equal_to<StaticKey<SIZE> > {
    size_t operator()(const StaticKey<SIZE>& first, const StaticKey<SIZE>& second) const {
      if (first.Size() != second.Size())
        return false;
  
      return !memcmp(first.Buffer(), second.Buffer(), second.Size());
    };
  };
)

FUNCTOR_HASH(
  template<int SIZE>
  struct hash<StaticKey<SIZE> > {
    // TODO(SECURITY): at construction time, we should use a random seed!
    // and maybe a cryptographically strong hash?
    size_t operator()(const StaticKey<SIZE>& key) const {
      size_t hash = DefaultHash(key.Buffer(), key.Size());
      LOG_DEBUG("getting hash %d", hash);
      return hash;
    };
  };
)

#endif /* STATIC_KEY_H */
