#include "gtest.h"
#include "src/serializers.h"

TEST(SerializerTest, EncodeString) {
  Buffer buffer;
  EncodeToBuffer("this is a test", buffer.Input());
  string str;
  EXPECT_FALSE(DecodeFromBuffer(buffer.Output(), &str));
  EXPECT_EQ("this is a test", str);
}

TEST(SerializerTest, EncodeDecodePairs) {
  Buffer buffer;

  pair<string, uint32_t> tuple("foo", 15);
  EncodeToBuffer(tuple, buffer.Input());

  pair<string, uint32_t> value;
  EXPECT_FALSE(DecodeFromBuffer(buffer.Output(), &value));
  EXPECT_EQ(tuple.first, value.first);
  EXPECT_EQ(tuple.second, value.second);
}

TEST(SerializerTest, EncodeDecodeVector) {
  Buffer buffer;

  vector<string> input;
  input.push_back("this");
  input.push_back("is");
  input.push_back("foo");
  EncodeToBuffer(input, buffer.Input());

  vector<string> output;
  EXPECT_FALSE(DecodeFromBuffer(buffer.Output(), &output));
  EXPECT_EQ("this", output[0]);
  EXPECT_EQ("is", output[1]);
  EXPECT_EQ("foo", output[2]);
  EXPECT_EQ(3, output.size());
}
