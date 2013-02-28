#ifndef IP_ADDRESSES_H
# define IP_ADDRESSES_H

# include "sockaddr.h"
# include <memory>

class IPAddress {
 public:
  virtual ~IPAddress() {}

  static IPAddress* Parse(const string& string);
  static IPAddress* Parse(const sockaddr* sockaddr, socklen_t socklen);

  virtual sa_family_t Family() const = 0;

  virtual const void* GetRaw() const = 0;
  virtual short unsigned int GetRawSize() const = 0;

  virtual Sockaddr* GetSockaddr(uint16_t port) const = 0;
  virtual string AsString() const = 0;
  virtual int Length() const = 0;

  virtual void Truncate(uint32_t length) = 0;
  virtual void Increment(uint32_t offset) = 0;

  virtual IPAddress* Clone() const = 0;
  virtual bool Difference(const IPAddress* address, int* difference) const = 0;
};

template <int FAMILY, int MAXLEN, typename SOCKADDR, typename ADDRESS>
class InetAddress : public IPAddress {
 public:
  InetAddress(const ADDRESS& address)
      : address_(address) {}
  virtual ~InetAddress() {}

  sa_family_t Family() const { return FAMILY; }
  Sockaddr* GetSockaddr(uint16_t port) const {
    return new SOCKADDR(address_, port);
  }

  const void* GetRaw() const { return &address_; }
  short unsigned int GetRawSize() const { return sizeof(ADDRESS); }

  virtual string AsString() const {
    char buffer[MAXLEN + 1];
    inet_ntop(FAMILY, &address_, buffer, MAXLEN);
    return string(buffer);
  }
  
  virtual int Length() const {
    return sizeof(ADDRESS) * 8;
  }

 protected:
  ADDRESS address_;
};

class IPv4Address : public InetAddress<AF_INET, INET_ADDRSTRLEN, IPv4Sockaddr, in_addr> {
 public:
  IPv4Address(const in_addr& addr)
      : InetAddress<AF_INET, INET_ADDRSTRLEN, IPv4Sockaddr, in_addr>(addr) {}
  virtual ~IPv4Address() {}

  virtual void Increment(uint32_t amount);
  virtual void Truncate(uint32_t length);

  virtual IPAddress* Clone() const;
  virtual bool Difference(const IPAddress* address, int* difference) const;

  static IPAddress* Parse(const string& address) {
    in_addr parsed;
    if (inet_pton(AF_INET, address.c_str(), &parsed) <= 0)
      return NULL;

    return new IPv4Address(parsed);
  }
};

class IPv6Address : public InetAddress<AF_INET6, INET6_ADDRSTRLEN, IPv6Sockaddr, in6_addr> {
 public:
  IPv6Address(const in6_addr& addr)
      : InetAddress<AF_INET6, INET6_ADDRSTRLEN, IPv6Sockaddr, in6_addr>(addr) {}
  virtual ~IPv6Address() {}

  virtual void Increment(uint32_t amount);
  virtual void Truncate(uint32_t length);

  virtual IPAddress* Clone() const;
  virtual bool Difference(const IPAddress* address, int* difference) const;

  static IPAddress* Parse(const string& address) {
    in6_addr parsed;
    if (inet_pton(AF_INET6, address.c_str(), &parsed) <= 0)
      return NULL;

    return new IPv6Address(parsed);
  }
};

class IPRange {
 public:
  IPRange();
  ~IPRange();

  bool Parse(const string& range);
  bool Parse(IPAddress* address, int cidr);

  // Returns true if the specified address is included in this range.
  bool IsIncluded(const IPAddress* address) const;
  bool IsOverlapping(const IPRange& range) const;
  bool IsInitialized() const;

  IPAddress* GetAddress() const;
  int GetCidr() const;

  // Returns the indexth address in this range.
  IPAddress* GetAddressIndex(int index) const;
  bool GetIndexAddress(IPAddress* address, int* index) const;

  // Returns the number of elements in this range. (eg, maximum value for index above).
  int Size() const;
  string AsString() const;
  void Swap(IPRange* range);

 private:
  IPAddress* address_;
  uint32_t cidr_;
  uint32_t size_;

  NO_COPY(IPRange);
};

#endif /* IP_ADDRESSES_H */
