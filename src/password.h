#ifndef PASSWORD_H
# define PASSWORD_H

# include "base.h"

# include <string>
# include <string.h>
# include <algorithm>

class Prng;
class InputCursor;

class StringBuffer {
 public:
  virtual char* Data() = 0;
  virtual const char* Data() const = 0;
  virtual int Used() const = 0;
  virtual int Size() const = 0;
  virtual void Resize(int used) = 0;

  string AsString() { 
    if (Size() >= 1 && Data()[Used() - 1] == '\0')
      return string(Data(), Used() - 1);
    return string(Data(), Used());
  }

  bool SameAs(const StringBuffer& other) const {
    if (Size() != other.Size())
      return false;
    if (memcmp(Data(), other.Data(), Size()))
      return false;
    return true;
  }
};

class StaticBuffer : public StringBuffer {
 public:
  static const int kBufferSize = 4096;
  StaticBuffer() : used_(0) {}
  StaticBuffer(const char* data, int size) {
    Resize(size);
    memcpy(buffer_, data, Size());
  }

  char* Data() { return buffer_; }
  const char* Data() const { return buffer_; }
  int Size() const { return kBufferSize; }
  void Resize(int used) { used_ = min(Size(), used); }
  int Used() const { return used_; }

 private:
  char buffer_[kBufferSize];
  int used_;
};

class ScopedPassword : public StaticBuffer {
 public:
  // On my laptop, 200003 iterations take roughly 1 second with the
  // internal implementation of pbkdf. It takes roughly twice as much
  // with the openssl implementation.
  static const int kPbkdfRounds = 200003;

  ScopedPassword() : StaticBuffer() {}
  ScopedPassword(const char* data, int size) : StaticBuffer(data, size) {}

  ~ScopedPassword() {
    Shred();
  }

  // Methods to turn a password into a cryptographic key based on PBKDF and SHA1.
  void ComputeKey(const char* salt, int saltlen, char* key, int keylen) const;
  static void ComputeSalt(Prng* prng, char* salt, int saltlen);

  void Shred() {
    for (int i = 0; i < Size(); i++)
      Data()[i] = '\0';
  }
};


#endif /* PASSWORD_H */
