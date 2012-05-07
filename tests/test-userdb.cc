#include "gtest.h"
#include "src/userdb.h"

TEST(UserDb, FullRoundTest) {
  unlink("/tmp/test.db");

  UdbSecretFile testdb("/tmp/test.db");
  EXPECT_TRUE(testdb.Open());
  EXPECT_TRUE(testdb.Close());

  EXPECT_TRUE(testdb.Open());

  string secret;
  EXPECT_FALSE(testdb.GetUser("foo", &secret));
  EXPECT_TRUE(testdb.AddUser("foo", "bar"));
  EXPECT_TRUE(testdb.GetUser("foo", &secret));
  EXPECT_EQ("bar", secret);
  EXPECT_FALSE(testdb.SetUser("baz", "brr"));
  EXPECT_TRUE(testdb.SetUser("foo", "bur!"));
  EXPECT_TRUE(testdb.GetUser("foo", &secret));
  EXPECT_EQ("bur!", secret);

  EXPECT_TRUE(testdb.Close());
  EXPECT_TRUE(testdb.Open());
  EXPECT_FALSE(testdb.GetUser("foo", &secret));
  EXPECT_TRUE(testdb.AddUser("foo", "bar"));
  EXPECT_TRUE(testdb.Commit());
  EXPECT_TRUE(testdb.Close());

  EXPECT_TRUE(testdb.Open());
  EXPECT_TRUE(testdb.GetUser("foo", &secret));
  EXPECT_EQ("bar", secret);
  EXPECT_FALSE(testdb.DelUser("brrr"));
  EXPECT_TRUE(testdb.DelUser("foo"));
  EXPECT_FALSE(testdb.GetUser("foo", &secret));
  EXPECT_TRUE(testdb.Close());

  EXPECT_TRUE(testdb.Open());
  EXPECT_TRUE(testdb.GetUser("foo", &secret));
  EXPECT_TRUE(testdb.DelUser("foo"));
  EXPECT_TRUE(testdb.Commit());
  EXPECT_TRUE(testdb.Close());

  EXPECT_TRUE(testdb.Open());
  EXPECT_FALSE(testdb.GetUser("foo", &secret));
  EXPECT_TRUE(testdb.AddUser("pippo", "pass1"));
  EXPECT_TRUE(testdb.AddUser("pluto", "pass2"));
  EXPECT_TRUE(testdb.AddUser("topolino", "pass3"));
  EXPECT_TRUE(testdb.Commit());
  EXPECT_TRUE(testdb.Close());

  EXPECT_TRUE(testdb.Open());
  EXPECT_TRUE(testdb.GetUser("pippo", &secret));
  EXPECT_EQ("pass1", secret);
  EXPECT_TRUE(testdb.GetUser("pluto", &secret));
  EXPECT_EQ("pass2", secret);
  EXPECT_TRUE(testdb.GetUser("topolino", &secret));
  EXPECT_EQ("pass3", secret);
}

