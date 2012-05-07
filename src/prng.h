#ifndef PRNG_H
# define PRNG_H

# include <openssl/rc4.h> 

# include "charset.h"

class Prng;
class Rc4Prng;

typedef Rc4Prng DefaultPrng;

class Prng {
 public:
  virtual int GetInt();
  virtual int GetIntRange(int min, int max);
  virtual long int GetLongInt();

  // Fills a buffer with random data.
  virtual void Get(char* buffer, int length) = 0;
  // Like the method above, but avoids putting \0s.
  virtual void GetZeroTerminated(char* buffer, int length);
};

class Rc4Prng
    : public Prng {
 public:
  static const int kSeedSize = 20;
  static const int kReSeedBytes = 1 << 20;

  Rc4Prng();
  void Get(char* buffer, int length);

 private:
  void Seed();

  unsigned int reseed_;
  RC4_KEY key_;
};

#endif /* PRNG_H */
