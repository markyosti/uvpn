#include "srp-client.h"
#include "serializers.h"
#include "conversions.h"

SrpClientSession::SrpClientSession(Prng* prng, BigNumberContext* bnctx, const string& username) 
    : secret_(primes_, -1),
      username_(username),
      prng_(prng),
      bnctx_(bnctx) {
  InitOpenSSL();
}

void SrpClientSession::FillClientHello(InputCursor* username) {
  LOG_DEBUG("username is %s", username_.c_str());
  EncodeToBuffer(username_, username);
}

bool SrpClientSession::ParseServerHello(OutputCursor* serverhello) {
  uint16_t index;
  if (!DecodeFromBuffer(serverhello, &index)) {
    LOG_DEBUG("could not read index");
    return false;
  }
  string salt;
  if (!DecodeFromBuffer(serverhello, &salt)) {
    LOG_DEBUG("could not read salt");
    return false;
  }
  if (!primes_.ValidIndex(index)) {
    LOG_DEBUG("invalid index");
    return false;
  }
  if (salt.size() < 8) {
    LOG_DEBUG("salt too small");
    return false;
  }

  salt_ = salt;
  index_ = index;

  // TODO(SECURITY,DEBUG): remove this.
  LOG_DEBUG("index: %d, salt: %s",  index, ConvertToHex(salt_.c_str(), salt_.size()).c_str());
  return true;
}

bool SrpClientSession::FillClientPublicKey(InputCursor* clientkey) {
  LOG_DEBUG();

  // A = g^a % N
  //  *g, generator.
  //  *N, prime number.
  //  *a, random number, at least 256 bits in length.
  a_.SetFromRandom(prng_, kALength);
  // TODO: A must be != 0

  const BigNumber& g = primes_.GetGenerator(index_);
  const BigNumber& N = primes_.GetPrime(index_);

  bnctx_->ModExp(&A_, g, a_, N);
  EncodeToBuffer(A_, clientkey);

  // TODO: A mod N MUST be != 0
  return true;
}

bool SrpClientSession::ParseServerPublicKey(OutputCursor* serverkey) {
  //   The premaster secret is calculated by the client as follows:
  //        *I, P = <read from user>
  //        *a = random()
  //        *N, g, s, B = <read from server>
  //        *A = g^a % N
  //        *u = SHA1(PAD(A) | PAD(B))
  //        *k = SHA1(N | PAD(g))
  //        *x = SHA1(s | SHA1(I | ":" | P))
  //        <premaster secret> = (B - (k * g^x)) ^ (a + (u * x)) % N
  if (!DecodeFromBuffer(serverkey, &B_)) {
    LOG_DEBUG("error while parsing server public key");
    return false;
  }
  LOG_DEBUG("server public key read");
  // TODO: verify B mod N, MUST be != 0
  return true;
}

bool SrpClientSession::GetPrivateKey(
    const ScopedPassword& password, ScopedPassword* secret) {
  LOG_DEBUG();

  const BigNumber& g = primes_.GetGenerator(index_);
  const BigNumber& N = primes_.GetPrime(index_);

  secret_.FromPassword(username_, salt_, index_, password);

  BigNumber k;
  secret_.CalculateK(N, g, &k);

  //        u = SHA1(PAD(A) | PAD(B))
  BigNumber u;
  secret_.CalculateU(N, A_, B_, &u);

  // kmul = (k * (g^x)) % N
  BigNumber kmul;
  bnctx_->ModMul(&kmul, k, secret_.v(), N);
  // bsub = (B - kmul) % N
  BigNumber bsub;
  bnctx_->ModSub(&bsub, B_, kmul, N);
  // umul = (u * x) % N
  BigNumber umul;
  bnctx_->ModMul(&umul, u, secret_.x(), N);
  // aadd = (a + umul) % N
  BigNumber aadd;
  bnctx_->ModAdd(&aadd, a_, umul, N);

  BigNumber ps;
  // <premaster secret> = (B - (k * g^x)) ^ (a + (u * x)) % N
  bnctx_->ModExp(&ps, bsub, aadd, N);

  ps.ExportAsBinary(secret);
  return true;
}
