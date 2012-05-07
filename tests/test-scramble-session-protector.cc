#include "gtest.h"

#include "src/scramble-session-protector.h"
#include "src/buffer.h"

TEST(ScrambleSessionProtectorTest, EncodeDecode) {
  DefaultPrng prng;
  ScrambleSessionEncoder encoder(&prng);

  const char data[] = "This is a real test. Too bad this string is sooo short.";
  int size = sizeof(data);

  Buffer cleartext;
  cleartext.Input()->Add(data, size);
  Buffer encrypted;
  EXPECT_TRUE(encoder.Encode(cleartext.Output(), encrypted.Input()));

  ScrambleSessionDecoder decoder;
  Buffer decrypted;
  EXPECT_EQ(
      ScrambleSessionDecoder::SUCCEEDED,
      decoder.Decode(encrypted.Output(), decrypted.Input()));

  EXPECT_EQ(string(data), string(decrypted.Output()->Data()));
}

TEST(ScrambleSessionProtector, LongerEncodeDecode) {
  DefaultPrng prng;
  ScrambleSessionEncoder encoder(&prng);

  char data[] = "this is a dumb string";
  int size = sizeof(data);

  Buffer cleartext;
  for (int i = 0; i < 8192; i++)
    cleartext.Input()->Add(data, size - 1);

  Buffer encrypted;
  OutputCursor clearcursor(*cleartext.Output());
  EXPECT_TRUE(encoder.Encode(&clearcursor, encrypted.Input()));

  Buffer decrypted;
  ScrambleSessionDecoder decoder;
  EXPECT_EQ(
      ScrambleSessionDecoder::SUCCEEDED,
      decoder.Decode(encrypted.Output(), decrypted.Input()));

  OutputCursor decryptcursor(*decrypted.Output());

  EXPECT_EQ(8192 * (size - 1), cleartext.Output()->LeftSize());
  EXPECT_EQ(8192 * (size - 1), decryptcursor.LeftSize());

  string clear, decrypt;
  cleartext.Output()->GetString(&clear);
  decryptcursor.GetString(&decrypt);

  EXPECT_EQ(clear, decrypt);
}
