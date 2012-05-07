#ifndef SRP_SERVER_H
# define SRP_SERVER_H

# include <string>
# include "base.h"
# include "openssl-helpers.h"
# include "srp-common.h"
# include "srp-passwd.h"
# include "prng.h"

// How to use it:
//
// SrpServer server;
// while (...) {
//   Buffer buffer;
//   RecvFromClient(&buffer);
//
//   string username;
//   SrpServerSession* session = server.Get(buffer, &username);
//   if (!session) {
//     [...]
//     LOG_ERROR("error");
//     continue;
//   }
//   
//   string usersecret = GetUserSecret(username);
//
//   Buffer initreply;
//   session.GetInitReply(usersecret, &reply);
//   SendToClient(reply);
//   
//   Buffer authreply;
//   RecvFromClient(&authreply);
//   session.SetAuthMessage(&authreply);
//   [...]
//   session.Encrypt(...);
//   session.Decrypt(...);
// }

class InputCursor;
class OutputCursor;

class SrpServerSession {
 public:
  SrpServerSession(Prng* prng, BigNumberContext* context);

  static bool ParseClientHello(OutputCursor* hellomessage, string* username);
  bool InitSession(const string& username, const string& secret);

  bool FillServerHello(InputCursor* serverhello);
  bool ParseClientPublicKey(OutputCursor* clientkey);

  bool FillServerPublicKey(InputCursor* serverkey);
  bool GetPrivateKey(ScopedPassword* secret);

  const SecurePrimes primes_;

 private:
  // RFC 5054 requires at least 256 bits (aka, 32 bytes). 48 = 384 bits, which
  // should be way more than needed.
  static const int kBLength = 384;

  BigNumber A_;
  BigNumber B_;
  BigNumber b_;

  SrpSecret secret_;
  string username_;

  Prng* prng_;
  BigNumberContext* bnctx_;
};

#endif /* SRP_SERVER_H */
