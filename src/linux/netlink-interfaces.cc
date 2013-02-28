#include "netlink-interfaces.h"

#include "../errors.h"
#include "../buffer.h"
#include "../stl-helpers.h"

#include <errno.h>
#include <string.h>

#include <unistd.h>
#include <net/if.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

class NetlinkSocket::Message {
 public:
  virtual ~Message() {}

  virtual nlmsghdr* GetHeader() = 0;
  virtual bool ParseReply(OutputCursor* cursor);

  void AddAttribute(unsigned short attribute, uint32_t value) {
    AddAttribute(attribute, &value, sizeof(value));
  }
  void AddAttribute(unsigned short attribute, const void* data, short unsigned int data_size) {
    nlmsghdr* header(GetHeader());
    rtattr* rta(reinterpret_cast<rtattr*>(((char *)header) + header->nlmsg_len));

    rta->rta_type = attribute;
    rta->rta_len = static_cast<unsigned short int>(RTA_LENGTH(data_size));
    memcpy(RTA_DATA(rta), data, data_size);

    header->nlmsg_len += RTA_ALIGN(RTA_LENGTH(data_size));
  }
};

class NetlinkSocket::LinkMessage : public NetlinkSocket::Message {
 public:
  LinkMessage(uint16_t type, uint16_t flags, int seq,
	      const string& ifname, unsigned ifi_change, unsigned ifi_flags) {
    memset(&data_, 0, sizeof(data_));

    data_.header.nlmsg_len = NLMSG_LENGTH(sizeof(ifinfomsg));
    data_.header.nlmsg_type = type;
    data_.header.nlmsg_flags = flags;
    data_.header.nlmsg_seq = seq;
    data_.header.nlmsg_pid = getpid();

    data_.ifinfo.ifi_family = AF_UNSPEC;
    data_.ifinfo.ifi_change = ifi_change;
    data_.ifinfo.ifi_flags = ifi_flags;

    AddAttribute(IFLA_IFNAME, ifname.c_str(),
                 static_cast<unsigned short int>(ifname.size() + 1));
  }

  nlmsghdr* GetHeader() { return &data_.header; }

 private:
  struct {
    nlmsghdr header;
    ifinfomsg ifinfo;

    // TODO: 128 must be > IFNAMSIZ.
    char buffer[128];
   } data_;
};

class NetlinkSocket::IfAddrMessage : public NetlinkSocket::Message {
 public:
  IfAddrMessage(uint16_t type, uint16_t flags, int seq,
		const IPAddress& address, int cidr, int index) {
    memset(&data_, 0, sizeof(data_));

    data_.header.nlmsg_len = NLMSG_LENGTH(sizeof(ifaddrmsg));
    data_.header.nlmsg_type = type;
    data_.header.nlmsg_flags = flags;
    data_.header.nlmsg_seq = seq;
    data_.header.nlmsg_pid = getpid();

    data_.ifaddr.ifa_family = static_cast<uint8_t>(address.Family());
    data_.ifaddr.ifa_prefixlen = static_cast<uint8_t>(cidr);
    data_.ifaddr.ifa_flags = 0;
    data_.ifaddr.ifa_scope = RT_SCOPE_HOST;
    data_.ifaddr.ifa_index = index;

    AddAttribute(IFA_LOCAL, address.GetRaw(), address.GetRawSize());
  }

  nlmsghdr* GetHeader() { return &data_.header; }

 private:
  struct {
    nlmsghdr header;
    ifaddrmsg ifaddr;
    char buf[128];
  } data_;
};

class NetlinkSocket::DumpRoutesMessage : public NetlinkSocket::Message {
 public:
  DumpRoutesMessage(uint16_t type, uint16_t flags, int seq) {
    memset(&data_, 0, sizeof(data_));

    data_.header.nlmsg_len = NLMSG_LENGTH(sizeof(rtmsg));
    data_.header.nlmsg_type = type;
    data_.header.nlmsg_flags = flags;
    data_.header.nlmsg_seq = seq;
    data_.header.nlmsg_pid = getpid();
  }
  ~DumpRoutesMessage();

  nlmsghdr* GetHeader() { return &data_.header; }
  bool ParseReply(OutputCursor* cursor);

  void GetEntries(list<RoutingEntry*>* entries);

 private:
  IPAddress* ParseIPAddress(int family, rtattr* attr);
  const string& ParseInterface(rtattr* attr);
  list<RoutingEntry*> entries_;

  struct {
    nlmsghdr header;
    rtmsg routing;
  } data_;
};

class NetlinkSocket::SetRoutesMessage : public NetlinkSocket::Message {
 public:
  SetRoutesMessage(
      uint16_t type, uint16_t flags, int seq, const IPAddress& network,
      unsigned short int cidr, const IPAddress* gateway,
      const IPAddress* source, Interface* interface) {
    memset(&data_, 0, sizeof(data_));

    data_.header.nlmsg_len = NLMSG_LENGTH(sizeof(rtmsg));
    data_.header.nlmsg_type = type;
    data_.header.nlmsg_flags = flags;
    data_.header.nlmsg_seq = seq;
    data_.header.nlmsg_pid = getpid();

    data_.routing.rtm_family = static_cast<unsigned char>(network.Family());
    data_.routing.rtm_dst_len = static_cast<unsigned char>(cidr);
    data_.routing.rtm_table = RT_TABLE_MAIN;
    data_.routing.rtm_scope = RT_SCOPE_UNIVERSE;
    data_.routing.rtm_type = RTN_UNICAST;
    data_.routing.rtm_protocol = RTPROT_BOOT;

    AddAttribute(RTA_DST, network.GetRaw(), network.GetRawSize());
    AddAttribute(RTA_OIF, interface->GetIndex());

    if (gateway) {
      DEBUG_FATAL_UNLESS(gateway->Family() == network.Family());
      data_.routing.rtm_family = static_cast<unsigned char>(gateway->Family());
      AddAttribute(RTA_GATEWAY, gateway->GetRaw(), gateway->GetRawSize());
    } else {
      data_.routing.rtm_scope = RT_SCOPE_LINK;
    }

    if (source)
      AddAttribute(RTA_PREFSRC, source->GetRaw(), source->GetRawSize());
  }

  nlmsghdr* GetHeader() { return &data_.header; }

 private:
  struct {
    nlmsghdr header;
    rtmsg routing;
    char buffer[128];
  } data_;
};

IPAddress* NetlinkSocket::DumpRoutesMessage::ParseIPAddress(int family, rtattr* attr) {
  if (family == AF_INET)
    return new IPv4Address(*reinterpret_cast<in_addr*>(RTA_DATA(attr)));
  if (family == AF_INET6)
    return new IPv6Address(*reinterpret_cast<in6_addr*>(RTA_DATA(attr)));

  LOG_ERROR("unknown address family %d", family);
  return NULL;
}

bool NetlinkSocket::Message::ParseReply(OutputCursor* cursor) {
  const nlmsghdr* header(reinterpret_cast<const nlmsghdr*>(cursor->Data()));
  bool status(true);

  while (cursor->ContiguousSize()) {
    if (cursor->LeftSize() > cursor->ContiguousSize() &&
	(cursor->ContiguousSize() < sizeof(nlmsghdr) ||
	 cursor->ContiguousSize() < NLMSG_ALIGN(header->nlmsg_len))) {
      // TODO: merge buffers in a smart way.
      LOG_FATAL("need to merge buffers in a smart way.");
    }

    if (header->nlmsg_type == NLMSG_ERROR) {
      const nlmsgerr* error(reinterpret_cast<const nlmsgerr*>(NLMSG_DATA(header)));
      if ((header->nlmsg_len - sizeof(*header)) < sizeof(nlmsgerr)) {
	LOG_ERROR("error message truncated?");
        status = false;
      } else {
	if (error->error) {
	  LOG_ERROR("netlink socket error %d", error->error);
	  status = false;
	} else {
	  LOG_DEBUG("change ACKED successfully");
	}
      }
    } else {
      LOG_ERROR("unexpected reply type %d", header->nlmsg_type);
      status = false;
    }

    cursor->Increment(NLMSG_ALIGN(header->nlmsg_len));
    header = reinterpret_cast<const nlmsghdr*>(cursor->Data());
  }

  return status;
}

NetlinkSocket::DumpRoutesMessage::~DumpRoutesMessage() {
  StlDeleteElements(&entries_);
}

void NetlinkSocket::DumpRoutesMessage::GetEntries(list<RoutingEntry*>* entries) {
  entries_.swap(*entries);
}

bool NetlinkSocket::DumpRoutesMessage::ParseReply(OutputCursor* cursor) {
  const nlmsghdr* header(reinterpret_cast<const nlmsghdr*>(cursor->Data()));
  while (cursor->ContiguousSize()) {
    if (cursor->LeftSize() > cursor->ContiguousSize() &&
	(cursor->ContiguousSize() < sizeof(nlmsghdr) ||
	 cursor->ContiguousSize() < NLMSG_ALIGN(header->nlmsg_len))) {
      // TODO: merge buffers in a smart way.
      LOG_FATAL("need to merge buffers in a smart way.");
    }

    rtmsg* msg(reinterpret_cast<rtmsg*>(NLMSG_DATA(header)));
    rtattr* attr(RTM_RTA(NLMSG_DATA(header)));
    int size(RTM_PAYLOAD(header));
    RoutingEntry* entry(new RoutingEntry());
    while (RTA_OK(attr, size)) {
      // TODO: filter out non main table? really? (it'd be cool if we supported
      //   updating the non main routing table).
      // TODO: write Parse* function below. Note that we will most likely need
      //   to rewrite IPAddress libraries to support network addresses and such. 
      switch (attr->rta_type) {
	case RTA_DST: {
	  IPAddress* address(ParseIPAddress(msg->rtm_family, attr));
	  if (address && entry->destination.Parse(address, msg->rtm_dst_len)) {
	    LOG_DEBUG("destination %s", entry->destination.AsString().c_str());
	  }
	  break;
	}

	case RTA_PREFSRC:
	  entry->source = ParseIPAddress(msg->rtm_family, attr);
	  break;

	case RTA_GATEWAY:
	  entry->gateway = ParseIPAddress(msg->rtm_family, attr);
	  break;

	case RTA_OIF:
	  entry->interface = *reinterpret_cast<unsigned int*>(RTA_DATA(attr));
	  break;

	case RTA_METRICS:
	  entry->metric = *reinterpret_cast<int*>(RTA_DATA(attr));
	  break;

	default:
	  LOG_DEBUG("DEFAULT");
	  break;
      }

      attr = RTA_NEXT(attr, size);
    }

    if (!entry->destination.IsInitialized() && entry->gateway) {
      if (entry->gateway->Family() == AF_INET)
        entry->destination.Parse("0.0.0.0/0");
      else if(entry->gateway->Family() == AF_INET6)
        entry->destination.Parse("::/0");
      else
        LOG_ERROR("unknown family? weird");
    }

    entries_.push_back(entry);
    cursor->Increment(NLMSG_ALIGN(header->nlmsg_len));
    header = reinterpret_cast<const nlmsghdr*>(cursor->Data());
  }

  return true;
}

RoutingTable::RoutingTable(NetlinkSocket* socket)
    : socket_(socket) {
}

NetlinkSocket::NetlinkSocket()
    : fd_(-1), seq_(0) {
}

bool NetlinkSocket::Chat(Message* message) {
  if (!Init())
    return false;

  if (!Send(message->GetHeader()))
    return false;

  Buffer buffer;
  if (!Recv(buffer.Input()))
    return false;

  return message->ParseReply(buffer.Output());
}


NetlinkSocket::~NetlinkSocket() {
  if (fd_ >= 0)
    close(fd_);
}

bool NetlinkSocket::Init() {
  if (fd_ >= 0)
    return true;

  fd_ = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);
  if (fd_ < 0) {
    LOG_PERROR("socket");
    return false;
  }

  return true;
}

bool RoutingTable::GetRoutes(list<RoutingEntry*>* entries) {
  NetlinkSocket::DumpRoutesMessage message(
      RTM_GETROUTE, NLM_F_REQUEST | NLM_F_DUMP, socket_->GetSeq());

  if (!socket_->Chat(&message))
    return false;

  message.GetEntries(entries);
  return true;
}

bool RoutingTable::AddRoute(
    const IPAddress& network, short unsigned int cidr, const IPAddress* gateway,
    const IPAddress* source, Interface* interface) {
  NetlinkSocket::SetRoutesMessage message(
      RTM_NEWROUTE, NLM_F_ACK | NLM_F_REQUEST | NLM_F_CREATE | NLM_F_REPLACE, socket_->GetSeq(),
      network, cidr, gateway, source, interface);
  return socket_->Chat(&message);
}

bool RoutingTable::DelRoute(
    const IPAddress& network, short unsigned int cidr, const IPAddress* gateway,
    const IPAddress* source, Interface* interface) {
  NetlinkSocket::SetRoutesMessage message(
      RTM_DELROUTE, NLM_F_ACK | NLM_F_REQUEST | NLM_F_CREATE | NLM_F_REPLACE, socket_->GetSeq(),
      network, cidr, gateway, source, interface);
  return socket_->Chat(&message);
}

bool NetlinkSocket::Recv(InputCursor* input) {
  while (1) {
    input->Reserve(4096);

    int read(recv(fd_, input->Data(), input->ContiguousSize(), 0));
    if (read < 0) {
      if (errno != EAGAIN) {
	LOG_PERROR("read");
	return false;
      }

      continue;
    }

    // Note that read here cannot be a negative number.
    if (static_cast<unsigned int>(read) < input->ContiguousSize()) {
      input->Increment(read);
      break;
    }

    input->Increment(read);
  }

  return true;
}

bool NetlinkSocket::Send(const nlmsghdr* header) {
  int nsent = send(fd_, reinterpret_cast<const void*>(header), header->nlmsg_len, 0);
  if (nsent < 0)
    LOG_PERROR("send");

  return nsent;
}

int NetlinkSocket::GetSeq() {
  return ++seq_;
}

Interface::Interface(NetlinkSocket* socket, const char* name, unsigned int index)
    : name_(name), index_(index), socket_(socket) {
}

bool Interface::Enable() {
  NetlinkSocket::LinkMessage message(
      RTM_NEWLINK, NLM_F_REQUEST | NLM_F_ACK, socket_->GetSeq(), name_,
      IFF_UP, IFF_UP);
  return socket_->Chat(&message);
}

bool Interface::Disable() {
  NetlinkSocket::LinkMessage message(
      RTM_NEWLINK, NLM_F_REQUEST | NLM_F_ACK, socket_->GetSeq(), name_,
      IFF_UP, 0);
  return socket_->Chat(&message);
}

bool Interface::AddAddress(const IPAddress& address, short unsigned int cidr) {
  NetlinkSocket::IfAddrMessage message(
      RTM_NEWADDR, NLM_F_REQUEST | NLM_F_CREATE | NLM_F_ACK | NLM_F_EXCL,
      socket_->GetSeq(), address, cidr, index_);
  return socket_->Chat(&message);
}

bool Interface::DelAddress(const IPAddress& address, short unsigned int cidr) {
  NetlinkSocket::IfAddrMessage message(
      RTM_DELADDR, NLM_F_REQUEST | NLM_F_ACK,
      socket_->GetSeq(), address, cidr, index_);
  return socket_->Chat(&message);
}

Interface* NetlinkNetworking::GetInterface(const char* name) {
  unsigned int index(if_nametoindex(name));
  if (!index)
    return NULL;
  return new Interface(&socket_, name, index);
}


