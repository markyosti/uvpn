#include "aes-session-protector.h"
#include "buffer.h"
#include "openssl-helpers.h"
#include "password.h"
#include "conversions.h"

#include <openssl/evp.h>


AesSessionKey::AesSessionKey(Prng* prng) : prng_(prng) {
}

AesSessionKey::~AesSessionKey() {
  memset(key_, 0, kKeyLengthInBytes);
  memset(salt_, 0, kKeyLengthInBytes);
}

void AesSessionKey::SendSalt(InputCursor* cursor) {
  LOG_DEBUG();

  // Pseudo code: generate salt, store it into output buffer, apply PBKDF2 on key.
  ScopedPassword::ComputeSalt(prng_, salt_, kKeyLengthInBytes);
  cursor->Add(salt_, kKeyLengthInBytes);
  LOG_DEBUG("salt: %s", ConvertToHex(salt_, kKeyLengthInBytes).c_str());
}

int AesSessionKey::RecvSalt(OutputCursor* cursor) {
  LOG_DEBUG();

  // Pseudo code: read salt from input buffer, apply PBKDF2 on key.
  int result = cursor->Consume(salt_, AesSessionKey::kKeyLengthInBytes);
  if (result < AesSessionKey::kKeyLengthInBytes) {
    LOG_ERROR("no salt in buffer");
    // TODO: handle error.
    return AesSessionKey::kKeyLengthInBytes - result;
  }

  LOG_DEBUG("salt: %s",
	    ConvertToHex(salt_, AesSessionKey::kKeyLengthInBytes).c_str());
  return 0;
}

void AesSessionKey::SetupKey(const ScopedPassword& secret) {
  secret.ComputeKey(salt_, AesSessionKey::kKeyLengthInBytes, key_,
 		    AesSessionKey::kKeyLengthInBytes);
  // TODO(SECURITY): this MUST NOT be logged.
  LOG_DEBUG("key: %s", ConvertToHex(key_, AesSessionKey::kKeyLengthInBytes).c_str());
}

const char* AesSessionKey::GetKey() const {
  return key_;
}

AesSessionEncoder::AesSessionEncoder(Prng* prng, const AesSessionKey& key)
    // TODO(security): CBC, can we have something that provides authentication
    // at the same time instead? crypto++ library has many more interesting
    // modes. Unfortunately, we're using openssl.
    : OpensslEncoder(OpensslProtector::kAES_256_CBC, prng) {
  RUNTIME_FATAL_UNLESS(OpensslProtector::kAES_256_CBC.KeyLength() == AesSessionKey::kKeyLengthInBytes)(
      "forgot to update .h? AesSessionKey::kKeyLengthInBytes %d is != than expected %d",
      AesSessionKey::kKeyLengthInBytes, OpensslProtector::kAES_256_CBC.KeyLength());
  memcpy(key_, key.GetKey(), AesSessionKey::kKeyLengthInBytes);
}


AesSessionEncoder::~AesSessionEncoder() {
}

bool AesSessionEncoder::Start(InputCursor* output, StartOptions options) {
  LOG_DEBUG();

  // Generate and send a random key.
  const int kIVLength = OpensslProtector::kAES_256_CBC.IVLength();
  output->Reserve(kIVLength);

  // TODO: this dance is horrible. I should use something more c++ish, and not need
  // to manage memory this way. (isn't that the whole point of io buffers??)
  char* iv = output->Data();
  prng_->Get(iv, kIVLength);
  output->Increment(kIVLength);

  return OpensslEncoder::Start(key_, iv, options);
}

AesSessionDecoder::AesSessionDecoder(Prng* prng, const AesSessionKey& key)
    // TODO(security): CBC, can we have something that provides authentication
    // at the same time instead? crypto++ library has many more interesting
    // modes. Unfortunately, we're using openssl.
    : OpensslDecoder(OpensslProtector::kAES_256_CBC), prng_(prng) {
  RUNTIME_FATAL_UNLESS(OpensslProtector::kAES_256_CBC.KeyLength() == AesSessionKey::kKeyLengthInBytes)(
      "forgot to update .h? AesSessionKey::kKeyLengthInBytes %d is != than expected %d",
      AesSessionKey::kKeyLengthInBytes, OpensslProtector::kAES_256_CBC.KeyLength());
  memcpy(key_, key.GetKey(), AesSessionKey::kKeyLengthInBytes);
}

AesSessionDecoder::~AesSessionDecoder() {
}

AesSessionDecoder::Result AesSessionDecoder::Start(
    OutputCursor* input, InputCursor* output, StartOptions options) {
  LOG_DEBUG();

  // TODO: I'd like to be able to assume that data read from a buffer doesn't
  // have to be copied out immediately.

  char iv[OpensslProtector::kAES_256_CBC.IVLength()];
  if (input->Consume(iv, sizeof_array(iv)) < sizeof_array(iv))
    return MORE_DATA_REQUIRED;

  return OpensslDecoder::Start(key_, iv, options);
}
