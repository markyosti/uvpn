#ifndef SRP_COMMON_H
# define SRP_COMMON_H

# include "openssl-helpers.h"

class SecurePrimes {
 public:
  struct Prime {
    const char* name;
    int generator;
    const char* value;
  };

  SecurePrimes();
  ~SecurePrimes();

  bool ValidIndex(int index) const;
  const BigNumber& GetPrime(int index) const;
  const BigNumber& GetGenerator(int index) const;

  const Prime& GetDescriptor(int index) const;

 private:
  static const Prime kValidPrimes[];

  // The following are treated like a cache. Marked as "mutable" to allow for
  // lazy initialization.
  mutable BigNumber** generators_;
  mutable BigNumber** primes_;
};

#endif /* SRP_COMMON_H */
