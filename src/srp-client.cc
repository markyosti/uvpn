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

int SrpClientSession::ParseServerHello(OutputCursor* serverhello) {
  uint16_t index;
  int missing;
  missing = DecodeFromBuffer(serverhello, &index);
  if (missing) {
    LOG_DEBUG("could not read index");
    return missing;
  }
  string salt;
  missing = DecodeFromBuffer(serverhello, &salt);
  if (missing) {
    LOG_DEBUG("could not read salt");
    return missing;
  }
  if (!primes_.ValidIndex(index)) {
    LOG_DEBUG("invalid index");
    return -1;
  }
  if (salt.size() < 8) {
    LOG_DEBUG("salt too small");
    return -1;
  }

  salt_ = salt;
  index_ = index;

  // TODO(SECURITY,DEBUG): remove this.
  LOG_DEBUG("index: %d, salt: %s",  index, ConvertToHex(salt_.c_str(), salt_.size()).c_str());
  return 0;
}

bool SrpClientSession::FillClientPublicKey(InputCursor* clientkey) {
  LOG_DEBUG();

  // A = g^a % N
  //  *g, generator.
  //  *N, prime number.
  //  *a, random number, at least 256 bits in length.
  
  // The loops below are extremely unlikely to ever be used, but the
  // server is required to refuse numbers matching those parameters,
  // as per rfc 5054.
  const BigNumber& g = primes_.GetGenerator(index_);
  const BigNumber& N = primes_.GetPrime(index_);
  do {
    do {
      a_.SetFromRandom(prng_, kALength);
    } while (a_.IsZero());

    bnctx_->ModExp(&A_, g, a_, N);
  } while (bnctx_->IsDivisibleBy(A_, N));

  EncodeToBuffer(A_, clientkey);
  return true;
}

int SrpClientSession::ParseServerPublicKey(OutputCursor* serverkey) {
  //   The premaster secret is calculated by the client as follows:
  //        *I, P = <read from user>
  //        *a = random()
  //        *N, g, s, B = <read from server>
  //        *A = g^a % N
  //        *u = SHA1(PAD(A) | PAD(B))
  //        *k = SHA1(N | PAD(g))
  //        *x = SHA1(s | SHA1(I | ":" | P))
  //        <premaster secret> = (B - (k * g^x)) ^ (a + (u * x)) % N
  int missing = DecodeFromBuffer(serverkey, &B_);
  if (missing) {
    LOG_DEBUG("error while parsing server public key");
    return missing;
  }
  LOG_DEBUG("server public key read");

  // Verify that N is not a divisor of B, otherwise we are in trouble.
  const BigNumber& N = primes_.GetPrime(index_);
  if (bnctx_->IsDivisibleBy(B_, N)) {
    LOG_ERROR("B mod N is 0, invalid");
    return -1;
  }
  return 0;
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
