#ifndef SOCKADDR_H
# define SOCKADDR_H

# include "base.h"
# include "hash.h"
# include "errors.h"
# include "conversions.h"

# include <string>
# include <string.h>
# include <sys/socket.h>
# include <stdio.h>

# include <arpa/inet.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <netinet/ip.h>
# include <netinet/ip6.h>

class Sockaddr {
 public:
  static Sockaddr* Parse(const string& address, uint16_t default_port);
  static Sockaddr* Parse(
      const sockaddr& sockaddr, const socklen_t size);

  virtual sa_family_t Family() const = 0;

  virtual const sockaddr* Data() const = 0;
  virtual size_t Size() const = 0;

  virtual const string AsString() const = 0;
};

template <typename SOCKADDR, typename ADDRESS>
class InetSockaddr : public Sockaddr {
 public:
  InetSockaddr() {}
  InetSockaddr(const sockaddr& sockaddr, const socklen_t size) {
    memcpy(&address_, &sockaddr, size);
  }

  virtual const ADDRESS& Host() const = 0;
  virtual uint16_t Port() const = 0;

  const sockaddr* Data() const {
    return reinterpret_cast<const sockaddr*>(&address_);
  }

  size_t Size() const {
    return sizeof(SOCKADDR);
  }

 protected:
  SOCKADDR address_;
};

class IPv4Sockaddr : public InetSockaddr<sockaddr_in, in_addr> {
 public:
  IPv4Sockaddr() { address_.sin_family = AF_INET; }
  IPv4Sockaddr(in_addr address, uint16_t port) {
    address_.sin_family = AF_INET;
    address_.sin_port = htons(port);
    address_.sin_addr = address;
  }
  IPv4Sockaddr(const sockaddr& sockaddr, const socklen_t socklen)
      : InetSockaddr<sockaddr_in, in_addr>(sockaddr, socklen) {
  }

  static IPv4Sockaddr* Parse(const string& host, uint16_t port);
  static IPv4Sockaddr* Parse(const sockaddr& sockaddr, const socklen_t size);

  virtual sa_family_t Family() const { return address_.sin_family; }
  const in_addr& Host() const { return address_.sin_addr; }
  uint16_t Port() const { return ntohs(address_.sin_port); }

  const string AsString() const {
    char address[INET_ADDRSTRLEN + 1];
    inet_ntop(Family(), &Host(), address, Size());

    string buffer(address);
    buffer.append(":");
    buffer.append(ToString(Port()));
    return buffer;
  }
};

class IPv6Sockaddr : public InetSockaddr<sockaddr_in6, in6_addr> {
 public:
  IPv6Sockaddr() { address_.sin6_family = AF_INET6; }
  IPv6Sockaddr(in6_addr address, uint16_t port) {
    address_.sin6_family = AF_INET6;
    LOG_ERROR("port is %d", port);
    address_.sin6_port = htons(port);
    address_.sin6_addr = address;
  }

  IPv6Sockaddr(const sockaddr& sockaddr, const socklen_t socklen)
      : InetSockaddr<sockaddr_in6, in6_addr>(sockaddr, socklen) {
  }

  static IPv6Sockaddr* Parse(const string& host, uint16_t port);
  static IPv6Sockaddr* Parse(const sockaddr& sockaddr, const socklen_t size);

  virtual sa_family_t Family() const { return address_.sin6_family; }
  const in6_addr& Host() const { return address_.sin6_addr; }
  uint16_t Port() const { return ntohs(address_.sin6_port); }

  const string AsString() const {
    char address[INET6_ADDRSTRLEN + 1];
    inet_ntop(Family(), &Host(), address, Size());

    string buffer("[");
    buffer.append(address);
    buffer.append("]");
    buffer.append(":");
    buffer.append(ToString(Port()));

    return buffer;
  }
};


// Define functors that will be automatically picked up by STL
// containers to handle addresses.
FUNCTOR_EQ(
  template<>
  struct equal_to<const Sockaddr*> {
    size_t operator()(const Sockaddr* first, const Sockaddr* second) const {
      LOG_DEBUG("comparing addresses %s to %s",
  	      first->AsString().c_str(), second->AsString().c_str());
  
      if (first->Family() != second->Family())
        return false;
  
      if (first->Size() != second->Size())
        return false;
  
      return !memcmp(first->Data(), second->Data(), second->Size());
    };
  };
)

FUNCTOR_HASH(
  template<>
  struct hash<const Sockaddr*> {
    // TODO(SECURITY): at construction time, we should use a random seed!
    // and maybe a cryptographically strong hash?
    size_t operator()(const Sockaddr* address) const {
      LOG_DEBUG("hashing address %s", address->AsString().c_str());
  
      size_t hash = DefaultHash(
          reinterpret_cast<const char*>(address->Data()),
          address->Size());
  
      return hash;
    };
  };
)

#endif /* SOCKADDR_H */
