#include "openssl-helpers.h"
#include "errors.h"
#include "conversions.h"
#include "serializers.h"
#include "prng.h"

void InitOpenSSL() {
  ENGINE_load_builtin_engines();

  if (!RAND_status()) {
    // Feed more entropy in case.
  }
}

const Digest::Engine Digest::kSHA1(EVP_sha1());
const Digest::Engine Digest::kSHA256(EVP_sha256());
const Digest::Engine Digest::kSHA512(EVP_sha512());

const Hmac::Engine Hmac::kSHA1(EVP_sha1());
const Hmac::Engine Hmac::kSHA256(EVP_sha256());
const Hmac::Engine Hmac::kSHA512(EVP_sha512());

BigNumberContext::BigNumberContext() {
  bn_ctx_ = BN_CTX_new();
}

BigNumberContext::~BigNumberContext() {
  BN_CTX_free(bn_ctx_);
}

void BigNumberContext::ModExp(
    BigNumber* result, const BigNumber& base, const BigNumber& exp,
    const BigNumber& module) {
  if (!BN_mod_exp(result->Get(), base.Get(), exp.Get(), module.Get(), bn_ctx_))
    LOG_FATAL("BN_mod_exp failed! %ld", ERR_get_error());
}

void BigNumberContext::ModSub(
    BigNumber* result, const BigNumber& base, const BigNumber& sub,
    const BigNumber& module) {
  if (!BN_mod_sub(result->Get(), base.Get(), sub.Get(), module.Get(), bn_ctx_))
    LOG_FATAL("BN_mod_exp failed! %ld", ERR_get_error());
}

void BigNumberContext::ModMul(
    BigNumber* result, const BigNumber& base, const BigNumber& mul,
    const BigNumber& module) {
  if (!BN_mod_mul(result->Get(), base.Get(), mul.Get(), module.Get(), bn_ctx_))
    LOG_FATAL("BN_add failed! %ld", ERR_get_error());
}

void BigNumberContext::Mod(
    BigNumber* result, const BigNumber& base, const BigNumber& module) {
  if (!BN_mod(result->Get(), base.Get(), module.Get(), bn_ctx_))
    LOG_FATAL("BN_add failed! %ld", ERR_get_error());
}

void BigNumberContext::ModAdd(
    BigNumber* result, const BigNumber& base, const BigNumber& add,
    const BigNumber& module) {
  if (!BN_mod_add(result->Get(), base.Get(), add.Get(), module.Get(), bn_ctx_))
    LOG_FATAL("BN_mod_exp failed! %ld", ERR_get_error());
}

void BigNumberContext::Add(
    BigNumber* result, const BigNumber& add1, const BigNumber& add2) {
  if (!BN_add(result->Get(), add1.Get(), add2.Get()))
    LOG_FATAL("BN_add failed! %ld", ERR_get_error());
}

void BigNumberContext::Mul(
    BigNumber* result, const BigNumber& base, const BigNumber& mul) {
  if (!BN_mul(result->Get(), base.Get(), mul.Get(), bn_ctx_))
    LOG_FATAL("BN_add failed! %ld", ERR_get_error());
}

bool BigNumberContext::IsDivisibleBy(
    const BigNumber& number, const BigNumber& divisor) {
  BigNumber result;
  Mod(&result, number, divisor);
  if (result.IsZero())
    return true;
  return false;
}

bool EncodeToBuffer(const BigNumber& bn, InputCursor* cursor) {
  int size = BN_num_bytes(bn.Get());
  if (size > numeric_limits<uint16_t>::max())
    return false;

  EncodeToBuffer(static_cast<uint16_t>(size), cursor);
  cursor->Reserve(size);
  BN_bn2bin(bn.Get(), reinterpret_cast<unsigned char*>(cursor->Data()));
  cursor->Increment(size);

  // TODO(SECURITY,DEBUG): remove this.
  string debug;
  bn.ExportAsHex(&debug);
  LOG_DEBUG("sent bignumber %s", debug.c_str());
  return true;
}

int DecodeFromBuffer(OutputCursor* cursor, BigNumber* bn) {
  uint16_t size;
  int missing = DecodeFromBuffer(cursor, &size);
  if (missing) {
    LOG_ERROR("too small, missing %d bytes", size);
    return missing;
  }
  if (cursor->ContiguousSize() >= size) {
    BN_bin2bn(reinterpret_cast<const unsigned char*>(cursor->Data()), size, bn->Get());
    cursor->Increment(size);
  } else {
    // If we don't have enough data, not much to do, we need more space.
    if (cursor->LeftSize() < size) {
      LOG_ERROR("less space than necessary, %d - %d", cursor->LeftSize(), size);
      return size - cursor->LeftSize();
    }

    // If the data is not contiguous in the buffer, we must make it contiguous
    // (in a temporary buffer), and then parse it.
    // TODO: we can do better than this...
    string temp;
    cursor->GetString(&temp, size);
    bn->SetFromBinary(temp);
  }

  // TODO(SECURITY,DEBUG): remove this.
  string debug;
  bn->ExportAsHex(&debug);
  LOG_DEBUG("received bignumber %s", debug.c_str());
  return 0;
}

BigNumber::BigNumber() {
  BN_init(&bn_);
}

BigNumber::~BigNumber() {
  BN_free(&bn_);
}

void BigNumber::SetFromInt(int value) {
  BN_set_word(&bn_, value);
}

void BigNumber::SetFromBinary(const string& value) {
  BN_bin2bn(reinterpret_cast<const unsigned char*>(value.c_str()), value.size(), &bn_);
}

bool BigNumber::SetFromAscii(const string& value) {
  BIGNUM* bn = &bn_;
  return BN_dec2bn(&bn, value.c_str()) == static_cast<int>(value.size());
}

bool BigNumber::SetFromHex(const string& value) {
  BIGNUM* bn = &bn_;
  return BN_hex2bn(&bn, value.c_str()) == static_cast<int>(value.size());
}

void BigNumber::SetFromRandom(Prng* prng, int length) {
  string buffer;

  length = length >> 3;
  buffer.resize(length);
  prng->Get(&(buffer[0]), length);
  SetFromBinary(buffer);
}

void BigNumber::ExportAsBinary(string* value) const {
  char* buffer = new char[BN_num_bytes(&bn_)];
  BN_bn2bin(&bn_, reinterpret_cast<unsigned char*>(buffer));
  value->assign(buffer, BN_num_bytes(&bn_));
  delete [] buffer;
}

void BigNumber::ExportAsBinary(char** buffer, int* size) const {
  *size = BN_num_bytes(&bn_);
  *buffer = new char[*size];
  BN_bn2bin(&bn_, reinterpret_cast<unsigned char*>(*buffer));
}

void BigNumber::ExportAsBinary(StringBuffer* buffer) const {
  buffer->Resize(BN_num_bytes(&bn_));
  RUNTIME_FATAL_UNLESS(buffer->Size() >= BN_num_bytes(&bn_))("invalid size");
  BN_bn2bin(&bn_, reinterpret_cast<unsigned char*>(buffer->Data()));
}

void BigNumber::ExportAsAscii(string* value) const {
  char* ascii = BN_bn2dec(&bn_);
  value->assign(ascii);
  OPENSSL_free(ascii);
}

void BigNumber::ExportAsHex(string* value) const {
  char* hex = BN_bn2hex(&bn_);
  value->assign(hex);
  OPENSSL_free(hex);
}

bool BigNumber::IsZero() const {
  return BN_is_zero(&bn_);
}

Digest::Digest(const Engine& engine) {
  EVP_MD_CTX_init(&ctx_);
  RUNTIME_FATAL_UNLESS(EVP_DigestInit_ex(&ctx_, engine.Get(), NULL))();
}

Digest::~Digest() {
  RUNTIME_FATAL_UNLESS(EVP_MD_CTX_cleanup(&ctx_))();
}

void Digest::Update(const char* value, int size) {
  RUNTIME_FATAL_UNLESS(EVP_DigestUpdate(&ctx_, value, size))();
}

void Digest::Get(string* str) {
  unsigned char md[EVP_MAX_MD_SIZE];
  unsigned int len;
  
  EVP_DigestFinal_ex(&ctx_, md, &len);
  str->assign(reinterpret_cast<char*>(md), len);
}

string Digest::ToString() {
  string digest;
  Get(&digest);

  string retval;
  for (unsigned int i = 0; i < digest.size(); i++) {
    AppendCharToHex(digest.c_str()[i], &retval);
  }

  return retval;
}

Hmac::Hmac(
    Context* context, const char* password, int size, const Engine& engine)
    : context_(context) {
  HMAC_Init_ex(context_->Get(), password, size, engine.Get(), NULL); 
}

void Hmac::Update(const char* data, int length) {
  HMAC_Update(context_->Get(),
	      reinterpret_cast<const unsigned char*>(data), length);
}

void Hmac::Get(char* hash) {
  HMAC_Final(context_->Get(), reinterpret_cast<unsigned char*>(hash), NULL);
}
