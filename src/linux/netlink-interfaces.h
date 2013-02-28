#ifndef LINUX_NETLINK_INTERFACES_H
# define LINUX_NETLINK_INTERFACES_H

# include "../base.h"
# include "../sockaddr.h"
# include "../ip-addresses.h"

# include <vector>
# include <list>

class OutputCursor;
class InputCursor;
struct nlmsghdr;

class NetlinkSocket {
 public:
  NetlinkSocket();
  ~NetlinkSocket();

  class Message;
  class DumpRoutesMessage;
  class SetRoutesMessage;
  class LinkMessage;
  class IfAddrMessage;

  int GetSeq();
  bool Chat(Message* message);

 private:
  bool Send(const nlmsghdr* header);
  bool Recv(InputCursor* reply);
  bool Init();

  int fd_;
  int seq_;
};

class Interface {
 public:
  Interface(NetlinkSocket* socket, const char* name, unsigned int index);

  bool Enable();
  bool Disable();

  bool AddAddress(const IPAddress& address, short unsigned int cidr);
  bool DelAddress(const IPAddress& address, short unsigned int cidr);

  const string& GetName() const { return name_; }
  unsigned int GetIndex() const { return index_; }

 private:
  const string name_;
  unsigned int index_;

  NetlinkSocket* socket_;
};

struct RoutingEntry {
  RoutingEntry()
      : source(NULL), gateway(NULL),
        interface(0), metric(0) {
  }

  ~RoutingEntry() {
    delete source;
    delete gateway;
  }

  IPAddress* source;
  IPRange destination;
  IPAddress* gateway;

  unsigned int interface;
  int metric;

  NO_COPY(RoutingEntry);
};

class RoutingTable {
 public:
  RoutingTable(NetlinkSocket* socket);

  bool AddRoute(
      const IPAddress& network, short unsigned int cidr,
      const IPAddress* gateway, const IPAddress* source, Interface* interface);
  bool DelRoute(
      const IPAddress& network, short unsigned int cidr,
      const IPAddress* gateway, const IPAddress* source, Interface* interface);

  bool GetRoutes(list<RoutingEntry*>* entries);

 private:
  NetlinkSocket* socket_;
};

class NetlinkNetworking {
 public:
  NetlinkNetworking() : table_(&socket_) {}
  ~NetlinkNetworking() {}

  Interface* GetInterface(const char* name);
  RoutingTable* GetRoutingTable() { return &table_; }

 private:
  NetlinkSocket socket_;
  RoutingTable table_;
};

#endif /* LINUX_NETLINK_INTERFACES_H */
