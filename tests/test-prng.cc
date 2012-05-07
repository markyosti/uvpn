#include "gtest.h"
#include <unordered_set>
#include <iostream>

#include "src/prng.h"
#include "src/base.h"
#include "src/errors.h"

TEST(DefaultPrng, GetInt) {
  DefaultPrng prng;

  unordered_set<int> previous;

  // TODO: this is a poor man's test. We'd better verify entropy or something alike.
  for (int i = 0; i < 8192; i++) {
    int ran = prng.GetInt();
    LOG_DEBUG("random number %d", ran);

    EXPECT_TRUE(previous.find(ran) == previous.end());
    previous.insert(ran);
  }
}

TEST(DefaultPrng, GetIntRange) {
  DefaultPrng prng;

  for (int i = 0; i < 8192; i++) {
    int ran = prng.GetIntRange(10, 100);

    LOG_DEBUG("random number %d", ran);
    EXPECT_LE(10, ran);
    EXPECT_GE(100, ran);
  }
}

TEST(DefaultPrng, GetZeroTerminated) {
  DefaultPrng prng;

  char buffer[100];
  for (int i = 0; i < 8192; i++) {
    int ran = prng.GetIntRange(1, 100);
    prng.GetZeroTerminated(buffer, ran);
    char* zero = static_cast<char*>(memchr(buffer, '\0', 100));

    EXPECT_EQ(ran - 1, zero - buffer);
  }
}
