#include "gtest.h"
#include "src/connection-key.h"
#include "src/prng.h"
#include "src/buffer.h"
#include "src/stl-helpers.h"

TEST(ConnectionKey, Hash) {
  ConnectionKey key1(this);
  ConnectionKey key2(NULL);
  ConnectionKey key3(this);
  ConnectionKey key4(this);

  const char data[] = "this is data";
  key1.Add(STRBUFFER(data));
  key2.Add(STRBUFFER(data));
  key3.Add(STRBUFFER(data));

  const char different[] = "this is different";
  key4.Add(STRBUFFER(different));

  unordered_map<ConnectionKey, int> hash;

  hash[key1] = 5;
  EXPECT_TRUE(StlMapHas(hash, key1));
  EXPECT_FALSE(StlMapHas(hash, key2));
  EXPECT_TRUE(StlMapHas(hash, key3));
  EXPECT_FALSE(StlMapHas(hash, key4));

  EXPECT_EQ(5, StlMapGet(hash, key3));
}
