#include "srp-passwd.h"
#include "openssl-helpers.h"
#include "errors.h"
#include "serializers.h"
#include "base64.h"
#include "conversions.h"

#include <sstream>

SrpSecret::SrpSecret(const SecurePrimes& primes, int index)
    : primes_(primes),
      index_(index) {
  DEBUG_FATAL_UNLESS(index == -1 || primes.ValidIndex(index))(
      "not a valid prime number, it seems? index: %d", index);
}

SrpSecret::~SrpSecret() {
}

bool SrpSecret::FromPassword(const string& username, const ScopedPassword& password) {
  string salt;

  // Generate salt.
  salt.resize(kSaltLen);
  prng_.Get(&*salt.begin(), kSaltLen);

  return FromPassword(username, salt, index_, password);
}

bool SrpSecret::FromPassword(
    const string& username, const string& salt, 
    int index, const ScopedPassword& password) {
  salt_ = salt;
  index_ = index;

  Digest userpassword(Digest::kSHA1);
  userpassword.Update(username);
  userpassword.Update(":", 1);
  userpassword.Update(password.Data(), password.Used());

  string hash;
  userpassword.Get(&hash);

  Digest salted(Digest::kSHA1);
  salted.Update(salt_);
  salted.Update(hash);
  salted.Get(&hash);

  // See rfc5054 - x = SHA1(s | SHA1(I | ":" | P))
  x_.SetFromBinary(hash);

  const BigNumber& n = primes_.GetPrime(index);
  const BigNumber& g = primes_.GetGenerator(index);

  bnctx_.ModExp(&v_, g, x_, n);
  return true;
}

void SrpSecret::ToSecret(string* secret) {
  // We now have all the pieces we need, just glue them together.
  Buffer buffer;
  EncodeToBuffer(static_cast<uint16_t>(index_), buffer.Input());
  EncodeToBuffer(salt_, buffer.Input());
  EncodeToBuffer(v_, buffer.Input());
  buffer.Output()->GetString(secret);
}

bool SrpSecret::FromSecret(const string& username, const string& secret) {
  // We now have all the pieces we need, just glue them together.
  Buffer buffer;
  // TODO: instead of copying into buffer, add support for "external chunks" in buffers.
  //   (eg, a chunk that is not allocated internally, but that can point to a string or anything else)
  buffer.Input()->Add(secret.c_str(), secret.size());

  if (!DecodeFromBuffer(buffer.Output(), reinterpret_cast<uint16_t*>(&index_)) ||
      !DecodeFromBuffer(buffer.Output(), &salt_) ||
      !DecodeFromBuffer(buffer.Output(), &v_)) {
    LOG_ERROR("error decoding %s", "data");
    return false;
  }

  if (salt_.size() < 4) {
    LOG_ERROR("error salt is too short %d", salt_.size());
    return false;
  }

  if (!primes_.ValidIndex(index_))
    return false;

  return true;
}

void SrpSecret::ToAscii(string* dump) {
  // TODO: do I really want to use ostringstream?
  ostringstream oss;

  string salt(ConvertToHex(salt_.c_str(), salt_.size()));

  string v;
  v_.ExportAsHex(&v);

  oss << "index: " << index_ << "\nsalt: " << salt << "\nv: " << v;
  dump->assign(oss.str());
}

void SrpSecret::PadToN(
    const string& N, string* topad) {
  if (N.size() > topad->size())
    topad->insert(0, N.size() - topad->size(), '\0');
}

void SrpSecret::CalculateU(
    const BigNumber& N, const BigNumber& A, const BigNumber& B, BigNumber* u) {
  string Nstr;
  N.ExportAsBinary(&Nstr);

  string Astr;
  A.ExportAsBinary(&Astr);
  PadToN(Nstr, &Astr);

  string Bstr;
  B.ExportAsBinary(&Bstr);
  PadToN(Nstr, &Bstr);

  Digest digest(Digest::kSHA1);
  digest.Update(Astr);
  digest.Update(Bstr);

  string ustr;
  digest.Get(&ustr);
  u->SetFromBinary(ustr);
}

void SrpSecret::CalculateK(
    const BigNumber& N, const BigNumber& g, BigNumber* k) {
  string Nstr;
  N.ExportAsBinary(&Nstr);
  string gstr;
  g.ExportAsBinary(&gstr);
  PadToN(Nstr, &gstr);

  Digest digest(Digest::kSHA1);
  digest.Update(Nstr);
  digest.Update(gstr);

  string kstr;
  digest.Get(&kstr);
  k->SetFromBinary(kstr);
}

