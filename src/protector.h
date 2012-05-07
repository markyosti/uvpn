#ifndef PROTECTOR_H
# define PROTECTOR_H

# include "base.h"
# include "macros.h"

class InputCursor;
class OutputCursor;

class SessionProtector {
 public:
  virtual ~SessionProtector() {}

  enum StartOptions {
    NoPadding = 0,
    AutoPadding = BIT(1)
  };

  enum Result {
    SUCCEEDED,
    MORE_DATA_REQUIRED,
    CORRUPTED_DATA
  };

  // Encrypt and decrypt data. This interface was designed mostly on openssl API.

  // Note that althouugh it's possible to call Start and End multiple times
  // on the same stream, it's generally not a good idea. Start will look for (or
  // provide) an iv every time (cost in size), and the context will be cleaned
  // every time End is called, which means that CBC/ECB/whatever mode is used
  // will start over, beside costing us resources.
  //
  // However, there's a problem in case multiple packets are encoded within
  // the same stream: we can't wait until we have enough bytes to fit a
  // cipher block or until End is called to send the traffic. In that
  // case, you can call the 'AddPadding' function in the middle of a stream
  // to force all data in buffers to be padded and flushed.
  // However, if AddPadding is called on the sender, you must use
  // 'RemovePadding' on the receiver, at the end of every packet.
  // It's up to the user of the Protector to implement this correctly.
};

class EncodeSessionProtector : virtual public SessionProtector {
 public:
  bool Encode(OutputCursor* input, InputCursor* output) {
    return Start(output, AutoPadding) &&
	   Continue(input, output) && End(output);
  }

  // Must be called once, when we start decrypting / encrypting a
  // stream of data.
  virtual bool Start(InputCursor* output, StartOptions options) = 0;

  // Must be called once, when we are done decrypting / encrypting a stream
  // of data. If the data to encrypt was not a multiple of the block size,
  // this will add padding and flush the pending data.
  virtual bool End(InputCursor* output) = 0;

  // Encrypts / decrypts some more data. Note that if this is a block
  // cipher, data must be processed in multiples of blocksize length.
  // Leftover data will be kept in buffers until more data is provided,
  // or until End is called, in which case the data will be padded to
  // the multiple of block size. The padding will be magically be removed
  // on decryption.
  virtual bool Continue(OutputCursor* input, InputCursor* output) = 0;

  // output is the encrypted stream buffer, datasize is the amount of
  // data that has been written / will be written. AddPadding adds data
  // to the encrypted stream to a multiple of the cipher block size so
  // we never have to wait for more data to decode what we have so far.
  virtual bool AddPadding(InputCursor* output, int datasize) = 0;
};

class DecodeSessionProtector : virtual public SessionProtector {
 public:
  // Must be called once, when we start decrypting / encrypting a
  // stream of data.
  virtual Result Start(
      OutputCursor* input, InputCursor* output, StartOptions options) = 0;
  // Must be called once, when we are done decrypting / encrypting a stream
  // of data. If the data to encrypt was not a multiple of the block size,
  // this will add padding and flush the pending data.
  virtual Result End(InputCursor* output) = 0;

  Result Decode(OutputCursor* input, InputCursor* output) {
    Result result;
    if ((result = Start(input, output, AutoPadding)) != SUCCEEDED ||
        (result = Continue(input, output)) != SUCCEEDED)
      return result;
    return End(output);
  }

  // Decrypts as much data as is available in input, up until 'until'
  // bytes have been deciphered, at which point it stops.
  // Note that deciphering will most likely stop at a block boundary,
  // so even if until is set, more bytes than requested might be
  // deciphered. This is ok if data is encoded and decoded with the
  // aid of AddPadding and RemovePadding.
  virtual Result Continue(
      OutputCursor* input, InputCursor* output, uint32_t until=0) = 0;

  // output is the cleartext stream, RemovePadding will remove the padding
  // that was added with AddPadding from the stream. datasize must be set
  // to the size that was originally passed to AddPadding.
  virtual Result RemovePadding(
      OutputCursor* output, int datasize, uint8_t* padsize) = 0;
};


#endif /* PROTECTOR_H */
