#ifndef OPENSSL_SESSION_PROTECTOR_H
# define OPENSSL_SESSION_PROTECTOR_H

# include "prng.h"
# include "protector.h"

# include <openssl/evp.h>

class Buffer;

// This is just C++ wrapper around openssl engines.
class OpensslCryptoEngine {
 public:
  OpensslCryptoEngine(const EVP_CIPHER* engine) : engine_(engine) {}

  unsigned int KeyLength() const { return EVP_CIPHER_key_length(engine_); }
  unsigned int IVLength() const { return EVP_CIPHER_iv_length(engine_); }
  unsigned int BlockSize() const { return EVP_CIPHER_block_size(engine_); }
  const EVP_CIPHER* Get() const { return engine_; }

 private:
  const EVP_CIPHER* engine_;
};

// Provides no security, but scrambles the data being sent so
// it cannot be easily sniffed or matched.
// Uses openssl by encrypting the data with a random key.
class OpensslProtector {
 public:
  // AES 256 bits with CBC.
  // TODO: we want to use something like CWC to ensure that the packet
  // has not been modified. But it's not supported by openssl :(
  static const OpensslCryptoEngine kAES_256_CBC;
  // Blowfish with CBC.
  static const OpensslCryptoEngine kBF_CBC;

  OpensslProtector(const OpensslCryptoEngine& cipher);
  virtual ~OpensslProtector();

 protected:
  EVP_CIPHER_CTX ctx_;
  const OpensslCryptoEngine& cipher_;
};

class OpensslEncoder : public OpensslProtector, public EncodeSessionProtector {
 public:
  OpensslEncoder(const OpensslCryptoEngine& cipher, Prng* prng)
      : OpensslProtector(cipher), prng_(prng) {
  }

  virtual ~OpensslEncoder() {}

  virtual bool Start(const char* key, const char* iv, StartOptions options);
  virtual bool Continue(OutputCursor* input, InputCursor* output);
  virtual bool End(InputCursor* output);

  virtual bool AddPadding(InputCursor* output, int datasize);

 protected:
  Prng* prng_;
};

class OpensslDecoder : public OpensslProtector, public DecodeSessionProtector {
 public:
  OpensslDecoder(const OpensslCryptoEngine& cipher)
      : OpensslProtector(cipher) {
  }
  virtual ~OpensslDecoder() {}

  virtual Result Start(const char* key, const char* iv, StartOptions options);
  virtual Result Continue(
      OutputCursor* input, InputCursor* output, uint32_t until=0);
  virtual Result End(InputCursor* output);

  virtual Result RemovePadding(
      OutputCursor* output, int datasize, uint8_t* padsize);
};

#endif /* OPENSSL_SESSION_PROTECTOR_H */
