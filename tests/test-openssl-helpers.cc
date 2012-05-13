#include "gtest.h"
#include "src/openssl-helpers.h"
#include "src/buffer.h"

TEST(OpenSSLHelpersTest, ShaDigest) {
  Digest digest(Digest::kSHA1);

  digest.Update("this is a test\n");
  digest.Update("really, this is a test\n");

  EXPECT_EQ("45a3317dbe98fd5872d625d2812a6eaf6e8d93ce", digest.ToString());
}

TEST(BigNumberTest, ValidConversions) {
  BigNumber bn;
  bn.SetFromInt(3);
  string exported;

  bn.ExportAsAscii(&exported);
  EXPECT_EQ("3", exported);
  bn.ExportAsHex(&exported);
  EXPECT_EQ("03", exported);

  string binary;
  bn.ExportAsBinary(&binary);
  BigNumber check;
  check.SetFromBinary(binary);
  check.ExportAsAscii(&exported);

  EXPECT_EQ("3", exported);

  EXPECT_TRUE(bn.SetFromAscii("4"));
  bn.ExportAsAscii(&exported);
  EXPECT_EQ("4", exported);

  EXPECT_TRUE(bn.SetFromHex("0A"));
  bn.ExportAsAscii(&exported);
  EXPECT_EQ("10", exported);
}

TEST(BigNumberTest, InvalidConversions) {
  BigNumber bn;
  EXPECT_FALSE(bn.SetFromAscii("CIAO"));
  EXPECT_FALSE(bn.SetFromHex("0Z"));
}

TEST(BigNumberTest, Buffer) {
  BigNumber bn;
  bn.SetFromInt(3);

  Buffer buffer;
  EncodeToBuffer(bn, buffer.Input());

  BigNumber check;
  EXPECT_EQ(0, DecodeFromBuffer(buffer.Output(), &check));

  string exported;
  check.ExportAsAscii(&exported);

  EXPECT_EQ("3", exported);
}
