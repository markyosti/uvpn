// Copyright (c) 2008,2009,2010,2011 Mark Moreno (kramonerom@gmail.com).
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 
//    1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 
//    2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// 
// THIS SOFTWARE IS PROVIDED BY Mark Moreno ''AS IS'' AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
// EVENT SHALL Mark Moreno OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
// THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// 
// The views and conclusions contained in the software and documentation are
// those of the authors and should not be interpreted as representing official
// policies, either expressed or implied, of Mark Moreno.

//
// This file contains methods to serialize and unserialize C data types
// into a buffer. Methods are inlined, and generally very simple, they
// should be fairly efficient on most platforms, as long as you stay
// away from strings or types that require memory allocations.
//
// To serialize data into a buffer, you can use something like:
//
//   bool succeeded = EncodeToBuffer(whatever, my_input_cursor);
//
// Where whatever is any type, and my_input_cursor is an InputCursor.
// Encoders return false if the type can't be encoded. It's generally
// a rare occurrence, but can happen if, for example, a string or an
// array is too long. To add a new Encoder, just declare it somewhere.
// There's nothing magic, plain overloading.
//
// To unserialize data, use something like:
//
//   string value;
//   int status = DecodeFromBuffer(my_output_cursor, &value);
//
// As per EncodeToBuffer above, value can be of any supported type.
// my_output_cursor is an OutputCursor. The status is:
//
// < 0 - if there was some error performing the conversion.
//   0 - if everything went smoothly, conversion successfully happened.
// > 0 - if more data is required to complete the conversion. Note that
//       the value is a *hint* of how much more data will be needed. It
//       is guaranteed to be <= the number of additional bytes actually
//       needed.
//

#ifndef SERIALIZERS_H
# define SERIALIZERS_H

# include <vector>
# include <limits>

# include "base.h"
# include "buffer.h"
# include <arpa/inet.h>

// FIXME: we should have some boundary checks! otherwise client and/or
// server can crash each other. Assumption is that the client should never
// trust the server, and the other way around.

inline bool EncodeToBuffer(uint8_t num, InputCursor* cursor) {
  cursor->Add(reinterpret_cast<char*>(&num), sizeof(num));
  return true;
}

inline bool EncodeToBuffer(uint32_t num, InputCursor* cursor) {
  num = htonl(num);
  cursor->Add(reinterpret_cast<char*>(&num), sizeof(num));
  return true;
}

inline bool EncodeToBuffer(int16_t num, InputCursor* cursor) {
  num = htons(num);
  cursor->Add(reinterpret_cast<char*>(&num), sizeof(num));
  return true;
}

inline bool EncodeToBuffer(uint16_t num, InputCursor* cursor) {
  num = htons(num);
  cursor->Add(reinterpret_cast<char*>(&num), sizeof(num));
  return true;
}

inline bool EncodeToBuffer(const char* str, int size, InputCursor* cursor) {
  if (size > numeric_limits<uint16_t>::max())
    return false;

  EncodeToBuffer(static_cast<uint16_t>(size), cursor);
  cursor->Add(str, size);
  return true;
}

inline bool EncodeToBuffer(const string& str, InputCursor* cursor) {
  if (str.size() > numeric_limits<uint16_t>::max())
    return false;

  EncodeToBuffer(static_cast<uint16_t>(str.size()), cursor);
  cursor->Add(str.c_str(), str.size());
  return true;
}

inline bool EncodeToBuffer(
    OutputCursor* output, InputCursor* input) {
  input->Reserve(output->LeftSize());
  while (output->LeftSize()) {
    memcpy(input->Data(), output->Data(), output->ContiguousSize());

    input->Increment(output->ContiguousSize());
    output->Increment(output->ContiguousSize());
  }
  return true;
}

inline int DecodeFromBuffer(OutputCursor* cursor, uint8_t* num) {
  int retval = cursor->Consume(reinterpret_cast<char*>(num), sizeof(*num));
  return sizeof(*num) - retval;
}

inline int DecodeFromBuffer(OutputCursor* cursor, int16_t* num) {
  int retval = cursor->Consume(reinterpret_cast<char*>(num), sizeof(*num));
  if (retval < static_cast<int>(sizeof(*num)))
    return sizeof(*num) - retval;
  *num = ntohs(*num);
  return 0;
}

inline int DecodeFromBuffer(OutputCursor* cursor, uint16_t* num) {
  int retval = cursor->Consume(reinterpret_cast<char*>(num), sizeof(*num));
  if (retval < static_cast<int>(sizeof(*num)))
    return sizeof(*num) - retval;
  *num = ntohs(*num);
  return 0;
}

inline int DecodeFromBuffer(OutputCursor* cursor, uint32_t* num) {
  int retval = cursor->Consume(reinterpret_cast<char*>(num), sizeof(*num));
  if (retval < static_cast<int>(sizeof(*num)))
    return sizeof(*num) - retval;
  *num = ntohl(*num);
  return 0;
}

inline int DecodeFromBuffer(OutputCursor* cursor, string* str) {
  uint16_t size;
  int result = DecodeFromBuffer(cursor, &size);
  if (result)
    return result;
  return cursor->ConsumeString(str, size);
}

template<typename FIRST, typename SECOND>
inline void EncodeToBuffer(
    const pair<FIRST, SECOND>& tuple, InputCursor* cursor) {
  EncodeToBuffer(tuple.first, cursor);
  EncodeToBuffer(tuple.second, cursor);
}

template<typename FIRST, typename SECOND>
inline int DecodeFromBuffer(
    OutputCursor* cursor, pair<FIRST, SECOND>* tuple) {
  int result;
  result = DecodeFromBuffer(cursor, &(tuple->first));
  if (result)
    return result;
  return DecodeFromBuffer(cursor, &(tuple->second));
}

template<typename ELEMENT>
inline int DecodeFromBuffer(
    OutputCursor* cursor, vector<ELEMENT>* data) {
  uint16_t size;
  int result;
  result = DecodeFromBuffer(cursor, &size);
  if (result)
    return result;
  data->resize(size);

  for (int i = 0; i < size; ++i) {
    ELEMENT temp;

    int result = DecodeFromBuffer(cursor, &temp);
    if (result)
      return result + size - i;
    (*data)[i] = temp;
  }

  return 0;
}

template<typename ELEMENT>
inline bool EncodeToBuffer(
    const vector<ELEMENT>& data, InputCursor* cursor) {
  if (data.size() > numeric_limits<uint16_t>::max())
    return false;

  EncodeToBuffer(static_cast<uint16_t>(data.size()), cursor);
  for (unsigned int i = 0; i < data.size(); ++i)
    EncodeToBuffer(data[i], cursor);

  return true;
}

#endif /* SERIALIZERS_H */
