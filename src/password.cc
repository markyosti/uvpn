#include "openssl-helpers.h"
#include "prng.h"
#include "buffer.h"
#include "password.h"

// TODO: this is included only for htonl. Seems silly, we should remove
// this dependency and have some more c++ish functions to do the same.
#include <arpa/inet.h>

void ScopedPassword::ComputeSalt(Prng* prng, char* salt, int saltlen) {
  prng->Get(salt, saltlen);
  return;
}

void ScopedPassword::ComputeKey(
    const char* salt, int saltlen, char* key, int keylen) const {
  Hmac::Context hctxt;

  char hash[Hmac::kSHA1.Length()];
  for (unsigned long i = 1; keylen > 0; i++) {
    int cplen = min(Hmac::kSHA1.Length(), keylen);
    uint32_t count = htonl(i);

    Hmac hmac(&hctxt, Data(), Used(), Hmac::kSHA1);
    hmac.Update(salt, saltlen);
    hmac.Update(reinterpret_cast<char*>(&count), sizeof(uint32_t));
    hmac.Get(hash);
    memcpy(key, hash, cplen);

    for(int j = 1; j < kPbkdfRounds; j++) {
      Hmac pmac(&hctxt, Data(), Used(), Hmac::kSHA1);
      pmac.Update(hash, Hmac::kSHA1.Length());
      pmac.Get(hash);

      for(int k = 0; k < cplen; k++)
        key[k] ^= hash[k];
    }
    keylen -= cplen;
    key += cplen;
  }
}
