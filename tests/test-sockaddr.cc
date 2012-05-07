#include "gtest.h"
#include "src/sockaddr.h"
#include <memory>

TEST(SockaddrTest, ParseAsString4) {
  auto_ptr<Sockaddr> addr(Sockaddr::Parse("127.0.0.1", 143));
  EXPECT_EQ("127.0.0.1:143", addr->AsString());
}

TEST(SockaddrTest, ParseAsString4WithPort) {
  auto_ptr<Sockaddr> addr(Sockaddr::Parse("127.0.0.1:80", 143));
  EXPECT_EQ("127.0.0.1:80", addr->AsString());
}

TEST(SockaddrTest, ParseAsString6) {
  auto_ptr<Sockaddr> addr(Sockaddr::Parse("::1", 143));
  EXPECT_EQ("[::1]:143", addr->AsString());
}

TEST(SockaddrTest, ParseAsString6WithSquares) {
  auto_ptr<Sockaddr> addr(Sockaddr::Parse("[::1]", 143));
  EXPECT_EQ("[::1]:143", addr->AsString());
}

TEST(SockaddrTest, ParseAsString6WithSquaresAndPort) {
  auto_ptr<Sockaddr> addr(Sockaddr::Parse("[::1]:80", 143));
  EXPECT_EQ("[::1]:80", addr->AsString());
}
