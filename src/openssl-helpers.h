#ifndef OPENSSL_HELPERS_H
# define OPENSSL_HELPERS_H

#include <openssl/evp.h>
#include <openssl/bn.h>
#include <openssl/engine.h>
#include <openssl/hmac.h>

#include "base.h"
#include "password.h"
#include <string>

extern void InitOpenSSL();

class Prng;
class Buffer;
class InputCursor;
class OutputCursor;

class ScopeCipherContextCleaner {
 public:
  explicit ScopeCipherContextCleaner(EVP_CIPHER_CTX* ctx)
      : ctx_(ctx) {
  }

  ~ScopeCipherContextCleaner() {
    EVP_CIPHER_CTX_cleanup(ctx_);
  }

 private:
  EVP_CIPHER_CTX* ctx_;
};

class BigNumber {
 public:
  BigNumber();
  ~BigNumber();

  void SetFromInt(int value);
  void SetFromBinary(const string& value);
  void SetFromRandom(Prng* prng, int length);
  bool SetFromAscii(const string& value);
  bool SetFromHex(const string& value);

  void ExportAsBinary(string* value) const;
  void ExportAsBinary(char** buffer, int* size) const;
  void ExportAsBinary(StringBuffer* buffer) const;

  void ExportAsAscii(string* value) const;
  void ExportAsHex(string* value) const;

  BIGNUM* Get() { return &bn_; }
  const BIGNUM* Get() const { return &bn_; }

 private:
  BIGNUM bn_;
};

extern bool EncodeToBuffer(const BigNumber&, InputCursor* cursor);
extern int DecodeFromBuffer(OutputCursor* cursor, BigNumber* bn);

class BigNumberContext {
 public:
  BigNumberContext();
  ~BigNumberContext();

  void ModExp(BigNumber* result, const BigNumber& base,
	      const BigNumber& exp, const BigNumber& module);
  void ModSub(BigNumber* result, const BigNumber& base,
	      const BigNumber& sub, const BigNumber& module);
  void ModAdd(BigNumber* result, const BigNumber& base,
	      const BigNumber& add, const BigNumber& module);
  void ModMul(BigNumber* result, const BigNumber& base,
	      const BigNumber& mul, const BigNumber& module);

  void Add(BigNumber* result, const BigNumber& add1, 
	   const BigNumber& add2);
  void Sub(BigNumber* result, const BigNumber& big,
	   const BigNumber& sub);
  void Mul(BigNumber* result, const BigNumber& base,
	   const BigNumber& multiplier);

 private:
  BN_CTX* bn_ctx_;
};

template<typename OTYPE, void (INIT)(OTYPE*),  void (DONE)(OTYPE*)>
class OpensslContext {
 public:
  OpensslContext() {
    INIT(&context_);
  }

  ~OpensslContext() {
    DONE(&context_);
  }

  OTYPE* Get() {
    return &context_;
  }

 private:
  OTYPE context_;
};

template<typename ENGINE>
class OpensslEngine {
 public:
  OpensslEngine(const ENGINE* engine)
      : engine_(engine) {
  }

  const ENGINE* Get() const { return engine_; }

 protected:
  const ENGINE* engine_;
};

class Hmac {
 public:
  class Engine : public OpensslEngine<EVP_MD> {
   public:
    Engine(const EVP_MD* engine) : OpensslEngine<EVP_MD>(engine) {}
    int Length() const { return EVP_MD_size(engine_); }
  };

  typedef OpensslContext<HMAC_CTX, HMAC_CTX_init, HMAC_CTX_cleanup> Context;

  static const Engine kSHA1;
  static const Engine kSHA256;
  static const Engine kSHA512;

  Hmac(Context* context, const char* password, int size, const Engine& engine);

  void Update(const char* data, int length);
  void Get(char* hash);

 private:
  Context* context_;
};

class Digest {
 public:
  typedef OpensslEngine<EVP_MD> Engine;
 
  static const Engine kSHA1;
  static const Engine kSHA256;
  static const Engine kSHA512;

  Digest(const Engine& engine);
  ~Digest();

  void Update(const char* value, int size);
  void Update(const string& str) {
    Update(str.c_str(), str.size());
  }

  void Get(string* value);
  string ToString();

 private:
  EVP_MD_CTX ctx_;
};

#endif /* OPENSSL_HELPERS_H */
