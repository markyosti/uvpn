#include "openssl-protector.h"
#include "buffer.h"
#include "openssl-helpers.h"

#include <openssl/evp.h>

const OpensslCryptoEngine OpensslProtector::kAES_256_CBC(EVP_aes_256_cbc());
const OpensslCryptoEngine OpensslProtector::kBF_CBC(EVP_bf_cbc());

OpensslProtector::OpensslProtector(const OpensslCryptoEngine& cipher) 
    : cipher_(cipher) {
  EVP_CIPHER_CTX_init(&ctx_);
}

OpensslProtector::~OpensslProtector() {
  EVP_CIPHER_CTX_cleanup(&ctx_);
}

bool OpensslEncoder::Start(const char* key, const char* iv, StartOptions options) {
  if (!EVP_EncryptInit_ex(&ctx_, cipher_.Get(), NULL,
                          (unsigned char*)(key),
                          (unsigned char*)(iv))) {
    ERR_print_errors_fp(stderr);
    LOG_DEBUG("encrypt init failed");
    return false;
  }

  if (options & AutoPadding) {
    LOG_DEBUG("padding enabled");
    EVP_CIPHER_CTX_set_padding(&ctx_, 1);
  } else {
    LOG_DEBUG("padding disabled");
    EVP_CIPHER_CTX_set_padding(&ctx_, 0);
  }

  return true;
}

bool OpensslEncoder::Continue(OutputCursor* input, InputCursor* output) {
  LOG_DEBUG();

  output->Reserve(input->LeftSize() + cipher_.BlockSize());
  while (input->LeftSize()) {
    int updatelen;
    if (!EVP_EncryptUpdate(&ctx_,
                           (unsigned char*)(output->Data()), &updatelen,
                           (unsigned char*)(input->Data()),
                           input->ContiguousSize())) {
      ERR_print_errors_fp(stderr);
      LOG_DEBUG("encrypt update failed");
      return false;
    }

    input->Increment(input->ContiguousSize());
    output->Increment(updatelen);
  }

  return true;
}

bool OpensslEncoder::End(InputCursor* output) {
  LOG_DEBUG();

  int finallen;
  if (!EVP_EncryptFinal_ex(&ctx_, reinterpret_cast<unsigned char*>(output->Data()), &finallen)) {
    ERR_print_errors_fp(stderr);
    LOG_DEBUG("encrypt final failed");
    return false;
  }

  output->Increment(finallen);
  EVP_CIPHER_CTX_cleanup(&ctx_);
  return true;
}

OpensslDecoder::Result OpensslDecoder::Start(
    const char* key, const char* iv, StartOptions options) {
  LOG_DEBUG();

  if (!EVP_DecryptInit_ex(&ctx_, cipher_.Get(), NULL,
                          (unsigned char*)(key),
                          (unsigned char*)(iv))) {
    ERR_print_errors_fp(stderr);
    LOG_DEBUG("decrypt init failed");
    return CORRUPTED_DATA;
  }

  if (options & AutoPadding) {
    LOG_DEBUG("padding enabled");
    EVP_CIPHER_CTX_set_padding(&ctx_, 1);
  } else {
    LOG_DEBUG("padding disabled");
    EVP_CIPHER_CTX_set_padding(&ctx_, 0);
  }

  return SUCCEEDED;
}

OpensslDecoder::Result OpensslDecoder::Continue(
    OutputCursor* input, InputCursor* output, uint32_t until) {
  LOG_DEBUG("already in context %d", ctx_.buf_len);

  // TODO(security): check that until + blocksize > until? we really don't
  // want to overflow until / updatelen here.

  // Round until to the next multiple of Blocksize.
  // This only works if the cipher blocksize is a power of 2, so check that
  // first!
  RUNTIME_FATAL_UNLESS(!(cipher_.BlockSize() & (cipher_.BlockSize() -1)))(
      "blocksize of cipher must be a power of 2 (%d)", cipher_.BlockSize());
  uint32_t rounded = (until + cipher_.BlockSize() - 1) & ~(cipher_.BlockSize() - 1);
  LOG_DEBUG("rounded until %d to %d, given block size of %d",
	    until, rounded, cipher_.BlockSize());

  until = rounded;
  output->Reserve(input->LeftSize() + cipher_.BlockSize());
  int updatelen = 0;
  for (uint32_t decoded = ctx_.buf_len; input->LeftSize(); decoded += updatelen) {
    int todecode;
    if (until) {
      if (decoded >= until)
	break;
      todecode = min(until - decoded, input->ContiguousSize());
    } else {
      todecode = input->ContiguousSize();
    }

    LOG_DEBUG("decoding leftsize %d, contiguous size %d, requested %d",
	      input->LeftSize(), input->ContiguousSize(), todecode);

    if (!EVP_DecryptUpdate(&ctx_,
                           (unsigned char*)(output->Data()), &updatelen,
                           (const unsigned char*)(input->Data()), todecode)) {
      ERR_print_errors_fp(stderr);
      LOG_FATAL("block decryption failed");
      return CORRUPTED_DATA;
    }

    LOG_DEBUG("decoding %d bytes in %d",
	      input->ContiguousSize(), updatelen);
    input->Increment(todecode);
    output->Increment(updatelen);
  }

  return SUCCEEDED;
}

OpensslDecoder::Result OpensslDecoder::End(InputCursor* output) {
  LOG_DEBUG();

  int finallen;
  if (!EVP_DecryptFinal_ex(&ctx_, (unsigned char*)(output->Data()), &finallen)) {
    ERR_print_errors_fp(stderr);
    LOG_FATAL("decrypt final failed for data in %p, %d bytes left in context", output->Data(), ctx_.buf_len);
    return CORRUPTED_DATA;
  }
  output->Increment(finallen);
  EVP_CIPHER_CTX_cleanup(&ctx_);
  return SUCCEEDED;
}

OpensslDecoder::Result OpensslDecoder::RemovePadding(
    OutputCursor* output, int datasize, uint8_t* padsize) {
  uint8_t pad = static_cast<uint8_t>(
      cipher_.BlockSize() - (datasize % cipher_.BlockSize()));

  LOG_DEBUG("removing %d bytes of padding", pad);

  if (padsize)
    *padsize = pad;
  if (output->LeftSize() < pad)
    return MORE_DATA_REQUIRED;

  output->Increment(pad);
  return SUCCEEDED;
}
    
bool OpensslEncoder::AddPadding(InputCursor* output, int datasize) {
  uint8_t pad = static_cast<uint8_t>(
      cipher_.BlockSize() - (datasize % cipher_.BlockSize()));

  LOG_DEBUG("adding %d bytes of padding (%d datasize, %d blocksize)", pad, datasize, cipher_.BlockSize());

  if (!pad)
    return true;

  Buffer temp;
  temp.Input()->Reserve(pad);
  prng_->Get(temp.Input()->Data(), pad);
  temp.Input()->Increment(pad);

  return Continue(temp.Output(), output);
}
