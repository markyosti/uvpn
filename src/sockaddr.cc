#include "sockaddr.h"
#include "conversions.h"

Sockaddr* Sockaddr::Parse(const string& address, uint16_t port) {
  Sockaddr* retval;
  if ((retval = IPv4Sockaddr::Parse(address, port)) != NULL)
    return retval;
  if ((retval = IPv6Sockaddr::Parse(address, port)) != NULL)
    return retval;

  return NULL;
}

Sockaddr* Sockaddr::Parse(
    const sockaddr& sockaddr, const socklen_t size) {
  Sockaddr* retval;
  if ((retval = IPv4Sockaddr::Parse(sockaddr, size)) != NULL)
    return retval;
  if ((retval = IPv6Sockaddr::Parse(sockaddr, size)) != NULL)
    return retval;

  return NULL;
}

IPv4Sockaddr* IPv4Sockaddr::Parse(
    const sockaddr& sockaddr, const socklen_t size) {
  if (sockaddr.sa_family != AF_INET || size != sizeof(sockaddr_in))
    return NULL;
  return new IPv4Sockaddr(sockaddr, size);
}

IPv6Sockaddr* IPv6Sockaddr::Parse(
    const sockaddr& sockaddr, const socklen_t size) {
  if (sockaddr.sa_family != AF_INET6 || size != sizeof(sockaddr_in6))
    return NULL;
  return new IPv6Sockaddr(sockaddr, size);
}

IPv4Sockaddr* IPv4Sockaddr::Parse(const string& address, uint16_t port) {
  string ip(address);

  string::size_type colon = address.rfind(':');
  if (colon != string::npos) {
    if (!FromString(address.substr(colon + 1), &port))
      return NULL;
    ip.assign(address.substr(0, colon));
  }

  in_addr inaddr;
  if (inet_pton(AF_INET, ip.c_str(), &inaddr) <= 0)
    return NULL;

  return new IPv4Sockaddr(inaddr, port);
}

IPv6Sockaddr* IPv6Sockaddr::Parse(const string& address, uint16_t port) {
  string ip;

  if (address.size() >= 1 && address[0] == '[') {
    string::size_type closed = address.rfind(']');
    if (closed != string::npos) {
      ip.assign(address.substr(1, closed - 1));

      if (closed != (address.length() - 1) &&
	  address[closed + 1] == ':') {
	if (!FromString(address.substr(closed + 2), &port))
	  return NULL;
      }
    }
  } else {
    ip.assign(address);
  }

  LOG_DEBUG("ADDRESS %s", ip.c_str());

  in6_addr inaddr;
  if (inet_pton(AF_INET6, ip.c_str(), &inaddr) <= 0)
    return NULL;

  return new IPv6Sockaddr(inaddr, port);
}
