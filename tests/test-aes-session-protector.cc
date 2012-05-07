#include "gtest.h"

#include "src/aes-session-protector.h"
#include "src/buffer.h"
#include "src/password.h"

TEST(AesSessionProtector, EncodeDecode) {
  DefaultPrng prng;

  ScopedPassword password(STRBUFFER("this is the key"));

  AesSessionKey keysend(&prng);
  AesSessionKey keyrecv(&prng);

  Buffer buffer;
  keysend.SendSalt(buffer.Input());
  keyrecv.RecvSalt(buffer.Output());

  keysend.SetupKey(password);
  keyrecv.SetupKey(password);

  AesSessionEncoder encoder(&prng, keysend);
  AesSessionDecoder decoder(&prng, keyrecv);

  const char data[] = "SHORT DATA";
  int size = sizeof(data);

  Buffer cleartext;
  cleartext.Input()->Add(data, size);

  Buffer encrypted;
  EXPECT_TRUE(encoder.Encode(cleartext.Output(), encrypted.Input()));

  Buffer decrypted;
  EXPECT_EQ(
      AesSessionDecoder::SUCCEEDED,
      decoder.Decode(encrypted.Output(), decrypted.Input()));

  EXPECT_EQ(string(data), string(decrypted.Output()->Data()));
}
