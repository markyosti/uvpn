#include "gtest.h"
#include <unordered_set>
#include <iostream>

#include "src/ip-manager.h"
#include "src/interfaces.h"
#include "src/base.h"
#include "src/errors.h"

TEST(IPManager, FindUsableRange) {
  IPRange range;
  NetworkConfig config;

  // TODO: provide a mock for network config, so I can actually write meaningful
  // tests, rather than half assed ones.

  EXPECT_TRUE(IPManager::FindUsableRange(config.GetRoutingTable(), &range));
  LOG_DEBUG("range: %s", range.AsString().c_str());
}


TEST(IPManager, AllocateFreeIP) {
  IPRange range;
  EXPECT_TRUE(range.Parse("10.0.0.0/8"));

  IPManager manager(range);
  auto_ptr<IPAddress> address;

  address.reset(manager.AllocateIP());
  ASSERT_TRUE(address.get());
  EXPECT_EQ("10.0.0.1", address->AsString());
  manager.ReturnIP(address.release());

  address.reset(manager.AllocateIP());
  ASSERT_TRUE(address.get());
  EXPECT_EQ("10.0.0.2", address->AsString());
  manager.ReturnIP(address.release());
}
