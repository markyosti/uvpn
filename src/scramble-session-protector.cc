#include "scramble-session-protector.h"
#include "buffer.h"
#include "openssl-helpers.h"

#include <openssl/evp.h>

ScrambleSessionEncoder::ScrambleSessionEncoder(Prng* prng) 
    : OpensslEncoder(OpensslProtector::kBF_CBC, prng) {
}

ScrambleSessionEncoder::~ScrambleSessionEncoder() {
}

bool ScrambleSessionEncoder::Start(InputCursor* output, StartOptions options) {
  LOG_DEBUG();

  // Reserve space for a key worth of random data.
  const int kKeyLength = OpensslProtector::kBF_CBC.KeyLength();
  output->Reserve(kKeyLength);

  // Generate the random key.
  char* key = output->Data();
  prng_->Get(key, kKeyLength);
  output->Increment(kKeyLength);

  LOG_DEBUG("key length: %d", OpensslProtector::kBF_CBC.KeyLength());
  LOG_DEBUG("iv length: %d", OpensslProtector::kBF_CBC.IVLength());

  RUNTIME_FATAL_UNLESS(
      OpensslProtector::kBF_CBC.KeyLength() >= OpensslProtector::kBF_CBC.IVLength())
      ("key and iv of different size break assumptions");
  return OpensslEncoder::Start(key, key, options);
}

ScrambleSessionDecoder::ScrambleSessionDecoder()
    : OpensslDecoder(OpensslProtector::kBF_CBC) {
}

ScrambleSessionDecoder::~ScrambleSessionDecoder() {
}

ScrambleSessionDecoder::Result ScrambleSessionDecoder::Start(
    OutputCursor* input, InputCursor* output, StartOptions options) {
  const unsigned int kKeyLength = OpensslProtector::kBF_CBC.KeyLength();
  char key[kKeyLength + 1];
  if (input->Consume(key, kKeyLength) < kKeyLength) {
    LOG_DEBUG("not enough bytes to read key (kKeyLength of %d required)", kKeyLength);
    return MORE_DATA_REQUIRED;
  }
  key[kKeyLength] = '\0';

  LOG_DEBUG("read key of %d bytes", kKeyLength);

  return OpensslDecoder::Start(key, key, options);
}
