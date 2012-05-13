#ifndef SRP_CLIENT_H
# define SRP_CLIENT_H

#include "base.h"
#include "openssl-helpers.h"
#include "srp-passwd.h"

#include <string>

class Buffer;

class SrpClientSession {
 public:
  SrpClientSession(
      Prng* prng, BigNumberContext* context, const string& username);

  // Sends user's name to the server.
  void FillClientHello(InputCursor* clienthello);
  // Gets the prime to use and the salt from the server.
  int ParseServerHello(OutputCursor* serverhello);

  // Based on the prime above and a random number, generates a "public key"
  // which is sent to the server.
  bool FillClientPublicKey(InputCursor* clientkey);
  // Reads the public key calculated by the server, based on a random number,
  // and the secret record of the user.
  int ParseServerPublicKey(OutputCursor* serverkey);

  // Finally, calculates a private key, using the user's password. Note that
  // at this point, if everything went well, the server calculated the *same*
  // private key.
  bool GetPrivateKey(const ScopedPassword& password, ScopedPassword* secret);

 private:
  // RFC 5054 requires at least 256 bits (aka, 32 bytes). 48 = 384 bits, which
  // should be way more than needed.
  static const int kALength = 384;

  const SecurePrimes primes_;

  SrpSecret secret_;
  string username_;
  int index_;
  string salt_;

  BigNumber A_;
  BigNumber B_;
  BigNumber a_;

  Prng* prng_;
  BigNumberContext* bnctx_;
};

#endif /* SRP_CLIENT_H */
