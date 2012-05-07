#include "ip-addresses.h"
#include "conversions.h"

void IPv4Address::Increment(uint32_t amount) {
  address_.s_addr = htonl(ntohl(address_.s_addr) + amount);
}

void IPv6Address::Increment(uint32_t amount) {
  for (int i = 3; i >= 0; --i) {
    uint32_t original = ntohl(address_.s6_addr32[i]);
    uint32_t newvalue = ntohl(address_.s6_addr32[i]) + amount;
    address_.s6_addr32[i] = htonl(newvalue);
    if (newvalue <= 0xffffffff && newvalue >= original)
      return;

    // note that offset is at most 32 bits, so there's not much
    // that can be carried over.
    amount = 1;
  }
}

void IPv6Address::Truncate(uint32_t length) {
  if (length == 128)
    return;

  uint32_t offset(length / 32);
  uint32_t mask(~(0xffffffff >> (length % 32)));
  address_.s6_addr32[offset] &= htonl(mask);
  memset(address_.s6_addr32 + offset + 1, 0, (4 - offset - 1) * 4);
}
  
bool IPv6Address::Difference(
    const IPAddress* address, int* difference) const {
  if (address->Family() != Family())
    return false;

  // TODO: this is all very ugly, we should really use something better than
  // plain integers. 12 is 96 bits in bytes.
  const IPv6Address* other(dynamic_cast<const IPv6Address*>(address));
  if (!memcmp(other->address_.s6_addr32, address_.s6_addr32, 12))
    return false;

  uint32_t addr1 = ntohl(address_.s6_addr32[3]);
  uint32_t addr2 = ntohl(other->address_.s6_addr32[3]);

  *difference = addr2 - addr1;
  if ((addr2 > addr1 && *difference < 0) ||
      (addr2 < addr1 && *difference > 0)) 
    return false;

  return true;
}

void IPv4Address::Truncate(uint32_t length) {
  if (length == 32)
    return;

  uint32_t mask(~(0xffffffff >> length));
  address_.s_addr &= htonl(mask);
}

bool IPv4Address::Difference(
    const IPAddress* address, int* difference) const {
  if (address->Family() != Family())
    return false;

  const IPv4Address* other(dynamic_cast<const IPv4Address*>(address));

  uint32_t addr1 = ntohl(address_.s_addr);
  uint32_t addr2 = ntohl(other->address_.s_addr);

  // Detect underflows / overflows.
  *difference = addr2 - addr1;
  if ((addr2 > addr1 && *difference < 0) ||
      (addr2 < addr1 && *difference > 0)) {
    LOG_ERROR("difference overflow");
    return false;
  }

  return true;
}

IPAddress* IPv4Address::Clone() const {
  return new IPv4Address(address_);
}

IPAddress* IPv6Address::Clone() const {
  return new IPv6Address(address_);
}

bool IPRange::IsOverlapping(const IPRange& other) const {
  if (!address_ || !other.address_ || other.address_->Family() != address_->Family())
    return false;

  int delta;
  // FIXME: this is actually not granted, could easily be overlapping outside
  // the range an int can cover.
  if (!address_->Difference(other.address_, &delta)) {
    LOG_ERROR("delta between %s and %s is too high",
	      other.address_->AsString().c_str(), address_->AsString().c_str());
    return false;
  }

  LOG_ERROR("delta between %s and %s is %d, sizes %d, %d",
      other.address_->AsString().c_str(), address_->AsString().c_str(), delta, size_, other.size_);

  if (delta >= 0 && delta <= size_)
    return true;
  if (delta < 0 && (0 - delta) <= other.size_)
    return true;

  return false;
}

bool IPRange::IsIncluded(const IPAddress* address) const {
  int delta;
  if (!address_->Difference(address, &delta))
    return false;

  if (delta < 0 || delta >= size_)
    return false;
  return true;
}

IPAddress* IPAddress::Parse(const string& str) {
  IPAddress* address;
  address = IPv4Address::Parse(str);
  if (address)
    return address;

  return IPv6Address::Parse(str);
}

IPAddress* IPAddress::Parse(const sockaddr* address, socklen_t socklen) {
  if (socklen < sizeof(*address))
    return NULL;

  if (address->sa_family == AF_INET) {
    if (socklen < sizeof(sockaddr_in))
      return NULL;
    return new IPv4Address(reinterpret_cast<const sockaddr_in*>(address)->sin_addr);
  }
  if (address->sa_family == AF_INET6) {
    if (socklen < sizeof(sockaddr_in6))
      return NULL;
    return new IPv6Address(reinterpret_cast<const sockaddr_in6*>(address)->sin6_addr);
  }

  return NULL;
}

bool IPRange::Parse(IPAddress* address, int cidr) {
  if (address_)
    delete address_;

  // LOG_DEBUG("setting address to %08x", address);

  address_ = address;
  if (cidr < 0 || cidr > address->Length())
    return false;

  // Note that first and last address of every network are used.
  int bits;
  if ((address->Length() - cidr) > 32) {
    // TODO: fix this, use something more than a simple int.
    LOG_ERROR("large network, clamping to 32 bits due to implementor lazyness");
    bits = 32;
  } else {
    bits = (address->Length() - cidr);
  }

  address_->Truncate(cidr);

  cidr_ = cidr;
  size_ = (1 << bits);

  // first and last address of a range are really not usable.
  if (size_ >= 2)
    size_ -= 2;
  else
    size_ = 0;

  return true;
}

int IPRange::Size() const {
  return size_;
}

IPAddress* IPRange::GetAddressIndex(int index) const {
  if (index < 0 || index >= size_)
    return NULL;

  IPAddress* address(address_->Clone());
  address->Increment(index + 1);
  return address;
}

bool IPRange::Parse(const string& range) {
  string::size_type slash(range.find('/'));

  string address;
  if (slash != string::npos) {
    address = range.substr(0, slash);
  } else {
    address = range;
  }

  auto_ptr<IPAddress> ip(IPAddress::Parse(address));
  if (!ip.get()) {
    LOG_DEBUG("invalid ip %s", address.c_str());
    return false;
  }

  int cidr(ip->Length());
  if (slash != string::npos) {
    const string& value(range.substr(slash + 1));
    if (value.empty()) {
      LOG_DEBUG("empty cidr %s", range.c_str());
      return false;
    }
    
    char* endptr;
    cidr = strtol(value.c_str(), &endptr, 10);
    if (*endptr) {
      LOG_DEBUG("invalid cidr %s in %s", value.c_str(), range.c_str());
      return false;
    }
  }
  
  return Parse(ip.release(), cidr);
}

string IPRange::AsString() const {
  if (!address_)
    return "not-initialized";

  string range(address_->AsString());
  range.append("/");
  range.append(ToString(cidr_));
  return range;
}

IPRange::IPRange()
    : address_(NULL), cidr_(0), size_(0) {
}

IPRange::~IPRange() {
  // LOG_DEBUG("freeing %08x", address_);
  delete address_;
}

bool IPRange::GetIndexAddress(IPAddress* address, int* index) const {
  int difference;
  if (!address_->Difference(address, &difference))
    return false;

  if (difference <= 0 || difference >= Size())
    return false;

  *index = difference - 1;
  return true;
}

void IPRange::Swap(IPRange* range) {
  uint32_t tmp_cidr(cidr_);
  uint32_t tmp_size(size_);
  IPAddress* tmp_address(address_);

  // LOG_DEBUG("address was %08x, range->address_ %08x",
  //	    address_, range->address_);

  cidr_ = range->cidr_;
  size_ = range->size_;
  address_ = range->address_;

  range->cidr_ = tmp_cidr;
  range->size_ = tmp_size;
  range->address_ = tmp_address;

  // LOG_DEBUG("address is now %08x, range->address_ %08x", 
  //	    address_, range->address_);
}

bool IPRange::IsInitialized() const {
  return address_;
}

IPAddress* IPRange::GetAddress() const {
  return address_;
}

int IPRange::GetCidr() const {
  return cidr_;
}
