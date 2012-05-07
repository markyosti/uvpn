#include "gtest.h"
#include "src/password.h"
#include "src/prng.h"
#include "src/buffer.h"
#include <openssl/evp.h>
#include <time.h>

TEST(Pbkdf, ComputeKeyAndSalt) {
  DefaultPrng prng;
  Buffer buffer;

  ScopedPassword password(STRBUFFER("this-is-my-ultra-secret-password"));

  LOG_DEBUG("test setup at %d", time(NULL));
  char salt[16];
  password.ComputeSalt(&prng, salt, 16);

  char computedkey[17];
  computedkey[16] = '\0';
  password.ComputeKey(salt, 16, computedkey, 16);
  LOG_DEBUG("calculated salt with internal function at %d", time(NULL));

  char comparekey[17];
  comparekey[16] = '\0';
  PKCS5_PBKDF2_HMAC_SHA1(password.Data(), password.Used(),
			 reinterpret_cast<unsigned char*>(salt), 16,
			 ScopedPassword::kPbkdfRounds,
			 16, reinterpret_cast<unsigned char*>(comparekey));
  LOG_DEBUG("calculated salt with openssl at %d", time(NULL));

  EXPECT_FALSE(memcmp(comparekey, computedkey, 17));
}
