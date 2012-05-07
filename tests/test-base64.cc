#include "gtest.h"
#include "src/base64.h"

TEST(Base64Test, Base64Encode) {
  string input;
  string output;

  input.assign("this is a test");
  Base64Encode(input, &output);
  EXPECT_EQ("dGhpcyBpcyBhIHRlc3Q=", output);
  output.clear();

  input.assign("this is a tes");
  Base64Encode(input, &output);
  EXPECT_EQ("dGhpcyBpcyBhIHRlcw==", output);
  output.clear();

  input.assign("this is a te");
  Base64Encode(input, &output);
  EXPECT_EQ("dGhpcyBpcyBhIHRl", output);
  output.clear();

  input.assign("");
  Base64Encode(input, &output);
  EXPECT_EQ("", output);
  output.clear();
}

TEST(Base64Test, Base64DecodeCleanly) {
  string input;
  string output;

  input.assign("dGhpcyBpcyBhIHRlc3Q=");
  EXPECT_TRUE(Base64Decode(input, &output));
  EXPECT_EQ("this is a test", output);
  output.clear();

  input.assign("dGhpcyBpcyBhIHRlcw==");
  EXPECT_TRUE(Base64Decode(input, &output));
  EXPECT_EQ("this is a tes", output);
  output.clear();

  input.assign("dGhpcyBpcyBhIHRl");
  EXPECT_TRUE(Base64Decode(input, &output));
  EXPECT_EQ("this is a te", output);
  output.clear();

  input.assign("");
  EXPECT_TRUE(Base64Decode(input, &output));
  EXPECT_EQ("", output);
  output.clear();
}

TEST(Base64Test, Base64DecodeErrors) {
  string input;
  string output;

  // input is wrong length (and one less =).
  input.assign("dGhpcyBpcyBhIHRlcw=");
  EXPECT_FALSE(Base64Decode(input, &output));

  // ! is not a valid base64 character.
  input.assign("dGhpcyBpc!BhIHRlcw==");
  EXPECT_FALSE(Base64Decode(input, &output));

  // 3 equals??
  input.assign("dGhpcyBpc!BhIHRlc===");
  EXPECT_FALSE(Base64Decode(input, &output));
}
