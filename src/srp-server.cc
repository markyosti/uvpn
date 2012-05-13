#include "srp-server.h"
#include "serializers.h"
#include "openssl-helpers.h"
#include "srp-passwd.h"
#include "conversions.h"

int SrpServerSession::ParseClientHello(
    OutputCursor* initmessage, string* username) {
  int result = DecodeFromBuffer(initmessage, username);
  if (result < 0) {
    LOG_DEBUG("invalid buffer for username");
    return result;
  }
  if (result > 0) {
    LOG_DEBUG("need more data in buffer");
    return result;
  }

  LOG_DEBUG("parsed hello %s", username->c_str());
  return 0;
}

SrpServerSession::SrpServerSession(Prng* prng, BigNumberContext* bnctx)
    : secret_(primes_, 0),
      prng_(prng),
      bnctx_(bnctx) {
  LOG_DEBUG();
}

bool SrpServerSession::InitSession(
    const string& username, const string& secretstr) {
  username_.assign(username);
  if (!secret_.FromSecret(username_, secretstr)) {
    LOG_ERROR("secret for user %s is invalid", username_.c_str());
    return false;
  }

  LOG_DEBUG("session for %s initialized", username.c_str());
  return true;
}

bool SrpServerSession::FillServerHello(InputCursor* hello) {
  LOG_DEBUG();

  EncodeToBuffer(static_cast<uint16_t>(secret_.index()), hello);
  EncodeToBuffer(secret_.salt(), hello);
  // TODO(SECURITY,DEBUG): remove this.
  LOG_DEBUG("index: %d, salt: %s",  secret_.index(), ConvertToHex(secret_.salt().c_str(), secret_.salt().size()).c_str());
  return true;
}

bool SrpServerSession::FillServerPublicKey(InputCursor* serverkey) {
  LOG_DEBUG();
  // B = k*v + g^b % N
  //  *v, verifier stored in secret.
  //  *k, SHA1(N | PAD(g)).
  //  *b, random number, at least 256 bits in length.
  //  *N, prime number.
  //  *g, generator.

  const BigNumber& g = primes_.GetGenerator(secret_.index());
  const BigNumber& N = primes_.GetPrime(secret_.index());

  BigNumber k;
  secret_.CalculateK(N, g, &k);

  b_.SetFromRandom(prng_, kBLength);

  BigNumber kv;
  bnctx_->ModMul(&kv, k, secret_.v(), N);

  bnctx_->ModExp(&B_, g, b_, N);
  bnctx_->ModAdd(&B_, B_, kv, N); 

  EncodeToBuffer(B_, serverkey);
  return true;
}

int SrpServerSession::ParseClientPublicKey(OutputCursor* clientkey) {
  LOG_DEBUG();

  int result = DecodeFromBuffer(clientkey, &A_);
  if (result) {
    LOG_DEBUG("%d - either invalid key or need more data", result);
    return result;
  }

  // TODO: verify A mod N, MUST be != 0
  return 0;
}

bool SrpServerSession::GetPrivateKey(ScopedPassword* secret) {
  LOG_DEBUG();

  //const BigNumber& g = primes_.GetGenerator(secret_.index());
  const BigNumber& N = primes_.GetPrime(secret_.index());

  // (A * v^u)^b
  BigNumber u;
  secret_.CalculateU(N, A_, B_, &u);

  BigNumber vu; 
  bnctx_->ModExp(&vu, secret_.v(), u, N);
  BigNumber avu;
  bnctx_->ModMul(&avu, A_, vu, N);

  BigNumber ps;
  bnctx_->ModExp(&ps, avu, b_, N);

  ps.ExportAsBinary(secret);
  return true;
}
