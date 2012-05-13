#ifndef AES_SESSION_PROTECTOR_H
# define AES_SESSION_PROTECTOR_H

# include "openssl-protector.h"
# include "prng.h"

class Buffer;
class ScopedPassword;

class AesSessionKey {
 public:
  static const unsigned int kKeyLengthInBytes = 256 / 8;

  AesSessionKey(Prng* prng);
  ~AesSessionKey();

  void SendSalt(InputCursor* output);
  int RecvSalt(OutputCursor* output);

  // Needs to be called after a salt has been sent or received.
  void SetupKey(const ScopedPassword& secret);
  const char* GetKey() const;

 private:
  Prng* prng_;
  char salt_[AesSessionKey::kKeyLengthInBytes];
  char key_[AesSessionKey::kKeyLengthInBytes];

  NO_COPY(AesSessionKey);
};

class AesSessionEncoder : public OpensslEncoder {
 public:
  AesSessionEncoder(Prng* prng, const AesSessionKey& key);
  virtual ~AesSessionEncoder();

  virtual bool Start(InputCursor* output, StartOptions options);

 private:
  char key_[AesSessionKey::kKeyLengthInBytes];

  NO_COPY(AesSessionEncoder);
};

class AesSessionDecoder : public OpensslDecoder {
 public:
  AesSessionDecoder(Prng* prng, const AesSessionKey& key);
  virtual ~AesSessionDecoder();

  virtual Result Start(
      OutputCursor* input, InputCursor* output, StartOptions options);

 private:
  Prng* prng_;

  char key_[AesSessionKey::kKeyLengthInBytes];
  NO_COPY(AesSessionDecoder);
};

#endif /* AES_SESSION_PROTECTOR_H */
