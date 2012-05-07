#ifndef CONNECTION_KEY_H
# define CONNECTION_KEY_H

# include "base.h"
# include "hash.h"
# include "macros.h"
# include "errors.h"

# include <string.h>

// Both UDP / TCP transcoders, first thing they'll do is ask us about
// a known connection. They will provide us with:
//   - a Key, uniquely identifying the connection.
//   - some data to read.
class ConnectionKey {
 public:
  explicit ConnectionKey(const void* handler) :
      handler_(handler), size_(0) {}

  static const int kBufferSize = 30;

  void Add(const char* data, uint16_t size) {
    DEBUG_FATAL_UNLESS(size_ + size < kBufferSize && size_ + size >= size_)(
        "can only store kBufferSize bytes in key, %d", kBufferSize);
    memcpy(buffer_ + size_, data, size);
    size_ = static_cast<uint16_t>(size_ + size);
  }

  uint16_t Size() const { return size_; }
  const void* Handler() const { return handler_; }
  const char* Buffer() const { return buffer_; }

 private:
  const void* handler_;
  char buffer_[kBufferSize];
  uint16_t size_;
};

// Define functors that will be automatically picked up by STL
// containers to handle addresses.
FUNCTOR_EQ(
  template<>
  struct equal_to<ConnectionKey> {
    size_t operator()(const ConnectionKey& first, const ConnectionKey& second) const {
      if (first.Handler() != second.Handler())
        return false;
  
      if (first.Size() != second.Size())
        return false;
  
      return !memcmp(first.Buffer(), second.Buffer(), second.Size());
    };
  };
)

FUNCTOR_HASH(
  template<>
  struct hash<ConnectionKey> {
    // TODO(SECURITY): at construction time, we should use a random seed!
    // and maybe a cryptographically strong hash?
    size_t operator()(const ConnectionKey& key) const {
      const void* ptr = key.Handler();
      size_t hash = DefaultHash(
          reinterpret_cast<const char*>(&ptr), sizeof(void*));
      hash = DefaultHash(key.Buffer(), key.Size(), hash);
      LOG_DEBUG("getting hash %d", hash);
      return hash;
    };
  };
)


#endif /* CONNECTION_KEY_H */
