#ifndef IP_MANAGER_H
# define IP_MANAGER_H

# include "ip-addresses.h"
# include <vector>
# include <list>

class RoutingTable;
class RoutingEntry;

class IPManager {
 public:
  IPManager(const IPRange& range);
  ~IPManager() {};

  static bool FindUsableRange(RoutingTable* table, IPRange* range);

  IPAddress* AllocateIP();
  bool ReturnIP(IPAddress* address);

 private:
  bool FindUsableIndex();
  void IncrementIndex();
  static bool IsRangeOverlapping(
      const IPRange& range, const list<RoutingEntry*>& etnries);

  const IPRange& range_;
  unsigned int index_;
  vector<bool> used_;
};

#endif /* IP_MANAGER_H */
