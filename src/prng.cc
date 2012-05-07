#include "prng.h"
#include "errors.h"

#include <string.h> // memset

#include <openssl/rand.h> 
#include <openssl/rc4.h> 
#include <openssl/err.h> 

int Prng::GetInt() {
  int retval;

  Get(reinterpret_cast<char *>(&retval), sizeof(retval));
  return retval;
}

long int Prng::GetLongInt() {
  long int retval;

  Get(reinterpret_cast<char *>(&retval), sizeof(retval));
  return retval;
}

int Prng::GetIntRange(int min, int max) {
  return (static_cast<unsigned int>(GetInt()) % ((max - min) + 1)) + min;
}

void Prng::GetZeroTerminated(char* buffer, int length) {
  DEBUG_FATAL_UNLESS(length)("requesting a 0 length random string?");

  Get(buffer, length - 1);
  for (int i = 0; i < length - 1; i++) {
    if (buffer[i])
      continue;

    buffer[i] = GetIntRange(1, 255);
  }
  buffer[length - 1] = '\0';
}

Rc4Prng::Rc4Prng() : reseed_(0) {
}

void Rc4Prng::Get(char* buffer, int length) {
  if (reseed_ <= 0)
    Seed();

  RC4(&key_, length, reinterpret_cast<unsigned char*>(buffer),
      reinterpret_cast<unsigned char*>(buffer));
  reseed_ -= length;
}

void Rc4Prng::Seed() {
  unsigned char buffer[kSeedSize];
  int i;

  if (RAND_bytes(buffer, kSeedSize) <= 0) {
    LOG_FATAL("Couldn't obtain random bytes (error %ld)", ERR_get_error()); 
  }
  RC4_set_key(&key_, kSeedSize, buffer); 

  // Discard first 256 bytes of RC4 output, for various reasons.
  for(i = 0; i <= 256; i += kSeedSize)
    RC4(&key_, kSeedSize, buffer, buffer);
  memset(buffer, 0, kSeedSize);

  reseed_ = kReSeedBytes;
}
