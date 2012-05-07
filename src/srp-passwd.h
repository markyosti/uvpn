#ifndef SRP_PASSWD_H
# define SRP_PASSWD_H

# include "prng.h"
# include "userdb.h"
# include "srp-common.h"

class ScopedPassword;

class SrpSecret : public UserSecret {
 private:
  static const int kSaltLen = 16;

 public:
  SrpSecret(const SecurePrimes&, int defaultindex);
  ~SrpSecret();

  // Creates a secret based on a password supplied by the user.
  // A random seed is generated, the default index is used.
  bool FromPassword(const string& username, const ScopedPassword& pwd);
  // Creates a secret based on a password supplied by the user, using
  // the seed and index supplied.
  bool FromPassword(
      const string& username, const string& seed, int index, const ScopedPassword& pwd);

  // Creates a secret based on a string that was previously generated
  // by ToSecret.
  bool FromSecret(const string& username, const string& secret);

  // Returns a binary encoded representation of the secret.
  // Suitable for storage in an authentication db or password file.
  void ToSecret(string* dump);
  // Returns a human readable description of the secret.
  // Suitable mostly for debugging only.
  void ToAscii(string* dump);

  int index() const { return index_; }
  const BigNumber& x() const { return x_; }
  const BigNumber& v() const { return v_; }
  const string& salt() const { return salt_; }

  static void PadToN(const string& N, string* topad);
  static void CalculateK(
    const BigNumber& N, const BigNumber& g, BigNumber* k);
  static void CalculateU(
    const BigNumber& N, const BigNumber& A, const BigNumber& B, BigNumber* u);

 private:
  const SecurePrimes& primes_;

  int index_;
  string salt_;
  BigNumber x_;
  BigNumber v_;

  BigNumberContext bnctx_;
  DefaultPrng prng_;
};

#endif /* SRP_PASSWD_H */
