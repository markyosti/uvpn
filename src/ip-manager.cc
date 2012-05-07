#include "ip-manager.h"
#include "netlink-interfaces.h"
#include "stl-helpers.h"

bool IPManager::IsRangeOverlapping(const IPRange& range, const list<RoutingEntry*>& entries) {
  LOG_DEBUG("trying range %s", range.AsString().c_str());

  for (list<RoutingEntry*>::const_iterator it(entries.begin());
       it != entries.end(); ++it) {
    RoutingEntry* entry(*it);
    if (entry->destination.IsOverlapping(range)) {
      LOG_DEBUG("overlaps with %s", entry->destination.AsString().c_str());
      return true;
    }
  }

  return false;
}

bool IPManager::FindUsableRange(RoutingTable* table, IPRange* result) {
  list<RoutingEntry*> entries;
  AUTO_DELETE_ELEMENTS(entries);

  if (!table->GetRoutes(&entries)) {
    // TODO: error handling.
    LOG_ERROR("failed reading routing table");
    return false;
  }

  IPRange range_192;
  range_192.Parse("192.168.0.0/16");

  IPRange range_172;
  range_172.Parse("172.16.0.0/12");

  IPRange range_10;
  range_10.Parse("10.0.0.0/8");

  IPRange* attempts[] = {
    &range_10, &range_172, &range_192
  };

  for (unsigned int i = 0; i < sizeof_array(attempts); ++i) {
    IPRange* range(attempts[i]);
    if (!IsRangeOverlapping(*range, entries)) {
      range->Swap(result);
      return true;
    }
  }

  return false;
}

IPManager::IPManager(const IPRange& range)
    : range_(range), index_(0) {
}

bool IPManager::FindUsableIndex() {
  for (int i = 0; i < range_.Size(); ++i) {
    if (!used_[index_])
      return true;
    IncrementIndex();
  }

  return false;
}

void IPManager::IncrementIndex() {
  index_ = (index_ + 1) % range_.Size();
}

IPAddress* IPManager::AllocateIP() {
  if (used_.size() <= index_)
    used_.resize(index_ + 1, 0);
  if (!FindUsableIndex())
    return NULL;

  used_[index_] = true;
  IPAddress* address(range_.GetAddressIndex(index_));
  IncrementIndex();

  return address;
}

bool IPManager::ReturnIP(IPAddress* address) {
  int index;
  if (!range_.GetIndexAddress(address, &index))
    return false;

  if (used_.size() <= index || !used_[index])
    return false;

  used_[index] = false;
  delete address;
  return true;
}
