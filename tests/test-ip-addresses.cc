#include "gtest.h"
#include <unordered_set>
#include <iostream>

#include "src/ip-addresses.h"
#include "src/base.h"
#include "src/errors.h"

TEST(IPAddresses, IPv4AddressParse) {
  auto_ptr<IPAddress> address;

  address.reset(IPv4Address::Parse("127.0.0.1"));
  EXPECT_EQ("127.0.0.1", address->AsString());
  EXPECT_EQ(NULL, IPv4Address::Parse("127.0.0.256"));
}

TEST(IPAddresses, IPv4AddressIncrement) {
  auto_ptr<IPAddress> address;

  address.reset(IPv4Address::Parse("127.0.0.1"));
  address->Increment(1);
  EXPECT_EQ("127.0.0.2", address->AsString());

  address.reset(IPv4Address::Parse("127.0.0.1"));
  address->Increment(-1);
  EXPECT_EQ("127.0.0.0", address->AsString());

  address.reset(IPv4Address::Parse("127.0.0.1"));
  address->Increment(256);
  EXPECT_EQ("127.0.1.1", address->AsString());

  address.reset(IPv4Address::Parse("127.0.0.1"));
  address->Increment(65536);
  EXPECT_EQ("127.1.0.1", address->AsString());

  address.reset(IPv4Address::Parse("127.0.0.1"));
  address->Increment(65791);
  EXPECT_EQ("127.1.1.0", address->AsString());
}

TEST(IPAddresses, IPv6AddressParse) {
  auto_ptr<IPAddress> address;

  address.reset(IPv6Address::Parse("::1"));
  EXPECT_EQ("::1", address->AsString());
  EXPECT_EQ(NULL, IPv6Address::Parse("::123456"));
}

TEST(IPAddresses, IPv6AddressIncrement) {
  auto_ptr<IPAddress> address;

  address.reset(IPv6Address::Parse("abcd:1234:5678:8765:4321:dcba:1234:2"));
  address->Increment(1);
  EXPECT_EQ("abcd:1234:5678:8765:4321:dcba:1234:3", address->AsString());

  address.reset(IPv6Address::Parse("abcd:1234:5678:8765:4321:dcba:1234:2"));
  address->Increment(65536);
  EXPECT_EQ("abcd:1234:5678:8765:4321:dcba:1235:2", address->AsString());

  address.reset(IPv6Address::Parse("abcd:1234:5678:8765:4321:dcba:1234:2"));
  address->Increment(65533);
  EXPECT_EQ("abcd:1234:5678:8765:4321:dcba:1234:ffff", address->AsString());

  address.reset(IPv6Address::Parse("abcd:1234:5678:8765:4321:dcba:1234:2"));
  address->Increment(4294967295U);
  EXPECT_EQ("abcd:1234:5678:8765:4321:dcbb:1234:1", address->AsString());
}

TEST(IPAddresses, IPRangeParseOk) {
  IPRange range;

  EXPECT_TRUE(range.Parse("127.0.0.1"));
  EXPECT_EQ("127.0.0.1/32", range.AsString());

  EXPECT_TRUE(range.Parse("127.0.0.0/24"));
  EXPECT_EQ("127.0.0.0/24", range.AsString());

  EXPECT_TRUE(range.Parse("127.0.0.0/16"));
  EXPECT_EQ("127.0.0.0/16", range.AsString());

  EXPECT_TRUE(range.Parse("127.0.0.0/8"));
  EXPECT_EQ("127.0.0.0/8", range.AsString());

  EXPECT_TRUE(range.Parse("0.0.0.0/0"));
  EXPECT_EQ("0.0.0.0/0", range.AsString());

  EXPECT_TRUE(range.Parse("127.0.0.0/0"));
  EXPECT_EQ("0.0.0.0/0", range.AsString());
}

TEST(IPAddresses, IPRangeParseError) {
  IPRange range;

  EXPECT_FALSE(range.Parse("127.0.0.0/33"));
  EXPECT_FALSE(range.Parse("127.0.0.0/-1"));
  EXPECT_FALSE(range.Parse("127.0.0.256"));
}

TEST(IPAddresses, IPv4Truncate) {
  auto_ptr<IPAddress> address;

  address.reset(IPv4Address::Parse("255.255.255.255"));
  address->Truncate(0);
  EXPECT_EQ("0.0.0.0", address->AsString());

  address.reset(IPv4Address::Parse("255.255.255.255"));
  address->Truncate(8);
  EXPECT_EQ("255.0.0.0", address->AsString());

  address.reset(IPv4Address::Parse("255.255.255.255"));
  address->Truncate(16);
  EXPECT_EQ("255.255.0.0", address->AsString());

  address.reset(IPv4Address::Parse("255.255.255.255"));
  address->Truncate(24);
  EXPECT_EQ("255.255.255.0", address->AsString());

  address.reset(IPv4Address::Parse("255.255.255.255"));
  address->Truncate(29);
  EXPECT_EQ("255.255.255.248", address->AsString());

  address.reset(IPv4Address::Parse("255.255.255.255"));
  address->Truncate(17);
  EXPECT_EQ("255.255.128.0", address->AsString());

  address.reset(IPv4Address::Parse("255.255.255.255"));
  address->Truncate(32);
  EXPECT_EQ("255.255.255.255", address->AsString());
}


TEST(IPAddresses, IPv6Truncate) {
  auto_ptr<IPAddress> address;

  address.reset(IPv6Address::Parse("abcd:1234:5678:8765:4321:dcba:1234:2"));
  address->Truncate(0);
  EXPECT_EQ("::", address->AsString());

  address.reset(IPv6Address::Parse("abcd:1234:5678:8765:4321:dcba:1234:2"));
  address->Truncate(8);
  EXPECT_EQ("ab00::", address->AsString());

  address.reset(IPv6Address::Parse("abcd:1234:5678:8765:4321:dcba:1234:2"));
  address->Truncate(24);
  EXPECT_EQ("abcd:1200::", address->AsString());

  address.reset(IPv6Address::Parse("abcd:1234:5678:8765:4321:dcba:1234:abcf"));
  address->Truncate(128);
  EXPECT_EQ("abcd:1234:5678:8765:4321:dcba:1234:abcf", address->AsString());

  address.reset(IPv6Address::Parse("abcd:1234:5678:8765:4321:dcba:1234:abcf"));
  address->Truncate(127);
  EXPECT_EQ("abcd:1234:5678:8765:4321:dcba:1234:abce", address->AsString());
}


TEST(IPRange, GetAddressIndexIndex) {
  IPRange range;

  EXPECT_TRUE(range.Parse("127.0.0.1/16"));
  EXPECT_EQ("127.0.0.0/16", range.AsString());

  auto_ptr<IPAddress> address(range.GetAddressIndex(0));
  EXPECT_EQ("127.0.0.1", address->AsString());

  int index = 100;
  EXPECT_TRUE(range.GetIndexAddress(address.get(), &index));
  EXPECT_EQ(0, index);

  EXPECT_EQ(NULL, range.GetAddressIndex(65536));
  EXPECT_EQ(NULL, range.GetAddressIndex(65535));
  EXPECT_EQ(NULL, range.GetAddressIndex(65534));
  EXPECT_EQ(NULL, range.GetAddressIndex(-1));

  EXPECT_EQ(65534, range.Size());
  address.reset(range.GetAddressIndex(65533));

  EXPECT_EQ("127.0.255.254", address->AsString());
}

TEST(IPRange, Overlapping) {
  IPRange range1;
  EXPECT_TRUE(range1.Parse("127.0.0.1/16"));
  EXPECT_EQ("127.0.0.0/16", range1.AsString());

  IPRange range2;
  EXPECT_TRUE(range2.Parse("127.0.0.1/24"));
  EXPECT_EQ("127.0.0.0/24", range2.AsString());

  IPRange range3;
  EXPECT_TRUE(range3.Parse("127.1.0.1/16"));
  EXPECT_EQ("127.1.0.0/16", range3.AsString());

  EXPECT_TRUE(range1.IsOverlapping(range2));
  EXPECT_TRUE(range2.IsOverlapping(range1));
  EXPECT_FALSE(range2.IsOverlapping(range3));
  EXPECT_FALSE(range1.IsOverlapping(range3));
  EXPECT_FALSE(range3.IsOverlapping(range2));
  EXPECT_FALSE(range3.IsOverlapping(range1));
}

TEST(IPRange, OverlappingRealistic) {
  IPRange range1;
  EXPECT_TRUE(range1.Parse("172.16.0.0/12"));
  EXPECT_EQ("172.16.0.0/12", range1.AsString());

  IPRange range2;
  EXPECT_TRUE(range2.Parse("127.255.255.255/32"));
  EXPECT_EQ("127.255.255.255/32", range2.AsString());

  EXPECT_FALSE(range2.IsOverlapping(range1));
  EXPECT_FALSE(range1.IsOverlapping(range2));
}
