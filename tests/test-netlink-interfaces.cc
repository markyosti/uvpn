#include "gtest.h"
#include <unordered_set>
#include <iostream>

#include "src/interfaces.h"
#include "src/base.h"
#include "src/errors.h"

TEST(RoutingTable, GetRoutes) {
  NetlinkNetworking networking;
  RoutingTable* table(networking.GetRoutingTable());
  list<RoutingEntry*> entries;

  table->GetRoutes(&entries);

  for (list<RoutingEntry*>::iterator it(entries.begin());
       it != entries.end(); ++it) {
    RoutingEntry* entry(*it);
    LOG_DEBUG("src=%s, dst=%s, gw=%s, int=%d, med=%d",
	      STR_OR_NULL(entry->source, AsString().c_str()),
	      entry->destination.AsString().c_str(),
	      STR_OR_NULL(entry->gateway, AsString().c_str()),
	      entry->interface, entry->metric);
  }
}
