#ifndef CONNECTION_KEY_H
# define CONNECTION_KEY_H

# include "base.h"
# include "hash.h"
# include "macros.h"
# include "errors.h"
# include "static-key.h"

# include <string.h>

//! Allows to uniquely identify different kind of connections.
//! 
//! ConnectionManagers need to keep track of all the existing connections.
//! Connections can come from any transcoder, but keys used by the
//! ConnectionManager need to be globally unique. To ensure uniqueness of the
//! ConnectionKey, a parameter handler is requested.
//! 
//! @param handler Pointer to the object that generated the connection. This
//!     ensures that whatever is filled in the key is globally unique. Note
//!     that if there is one object per connection (eg, a Session object), the
//!     address of this object may be enough to uniquely identify the
//!     connection. 
class ConnectionKey : public DynamicKey<20> {
 public:
  explicit ConnectionKey(const void* handler) : handler_(handler) {}
  const void* Handler() const { return handler_; }

 private:
  const void* handler_;
};

// Define functors that will be automatically picked up by STL
// containers to handle addresses.
FUNCTOR_EQ(
  template<>
  struct equal_to<ConnectionKey> {
    size_t operator()(const ConnectionKey& first, const ConnectionKey& second) const {
      if (first.Handler() != second.Handler())
        return false;

      return equal_to<ConnectionKey::static_key_t>()(first, second);
    };
  };
)

FUNCTOR_HASH(
  template<>
  struct hash<ConnectionKey> {
    size_t operator()(const ConnectionKey& key) const {
      size_t value = hash<ConnectionKey::static_key_t>()(key);
      const void* ptr = key.Handler();
      value = DefaultHash(reinterpret_cast<const char*>(&ptr), sizeof(ptr), value);
      LOG_DEBUG("getting hash %d", value);
      return value;
    };
  };
)

#endif /* CONNECTION_KEY_H */
