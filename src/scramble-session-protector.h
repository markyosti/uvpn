#ifndef SCRAMBLE_SESSION_PROTECTOR_H
# define SCRAMBLE_SESSION_PROTECTOR_H

# include "openssl-protector.h"
# include "prng.h"

class Buffer;

// Provides NO SECURITY, it just scrambles the data in a way
// that's supposed to be somewhat expensive and non-trivial
// for a dumb hardware device to process.
class ScrambleSessionEncoder : public OpensslEncoder{
 public:
  ScrambleSessionEncoder(Prng* prng);
  virtual ~ScrambleSessionEncoder();

  bool Start(InputCursor* output, StartOptions options);
};

// Provides NO SECURITY, it just scrambles the data in a way
// that's supposed to be somewhat expensive and non-trivial
// for a dumb hardware device to process.
class ScrambleSessionDecoder : public OpensslDecoder{
 public:
  ScrambleSessionDecoder();
  virtual ~ScrambleSessionDecoder();

  Result Start(OutputCursor* input, InputCursor* output, StartOptions options);
};

#endif /* SCRAMBLE_SESSION_PROTECTOR_H */
