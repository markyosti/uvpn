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

inline void EncodeToBuffer(uint8_t num, InputCursor* cursor) {
  cursor->Add(reinterpret_cast<char*>(&num), sizeof(num));
}

inline void EncodeToBuffer(uint32_t num, InputCursor* cursor) {
  num = htonl(num);
  cursor->Add(reinterpret_cast<char*>(&num), sizeof(num));
}

inline void EncodeToBuffer(uint16_t num, InputCursor* cursor) {
  num = htons(num);
  cursor->Add(reinterpret_cast<char*>(&num), sizeof(num));
}

inline void EncodeToBuffer(const char* str, int size, InputCursor* cursor) {
  EncodeToBuffer(static_cast<uint16_t>(size), cursor);
  cursor->Add(str, size);
}

inline void EncodeToBuffer(const string& str, InputCursor* cursor) {
  EncodeToBuffer(static_cast<uint16_t>(str.size()), cursor);
  cursor->Add(str.c_str(), str.size());
}

inline void EncodeToBuffer(
    OutputCursor* output, InputCursor* input) {
  input->Reserve(output->LeftSize());
  while (output->LeftSize()) {
    memcpy(input->Data(), output->Data(), output->ContiguousSize());

    input->Increment(output->ContiguousSize());
    output->Increment(output->ContiguousSize());
  }
}

inline bool DecodeFromBuffer(OutputCursor* cursor, uint8_t* num) {
  int retval = cursor->Get(reinterpret_cast<char*>(num), sizeof(*num));
  if (retval != sizeof(*num))
    return false;
  return true;
}

inline bool DecodeFromBuffer(OutputCursor* cursor, uint16_t* num) {
  int retval = cursor->Get(reinterpret_cast<char*>(num), sizeof(*num));
  if (retval != sizeof(*num))
    return false;
  *num = ntohs(*num);
  return true;
}

inline bool DecodeFromBuffer(OutputCursor* cursor, uint32_t* num) {
  int retval = cursor->Get(reinterpret_cast<char*>(num), sizeof(*num));
  if (retval != sizeof(*num))
    return false;
  *num = ntohl(*num);
  return true;
}

inline bool DecodeFromBuffer(OutputCursor* cursor, string* str) {
  uint16_t size;
  if (!DecodeFromBuffer(cursor, &size))
    return false;
  return cursor->GetString(str, size);
}

template<typename FIRST, typename SECOND>
inline void EncodeToBuffer(
    const pair<FIRST, SECOND>& tuple, InputCursor* cursor) {
  EncodeToBuffer(tuple.first, cursor);
  EncodeToBuffer(tuple.second, cursor);
}

template<typename FIRST, typename SECOND>
inline bool DecodeFromBuffer(
    OutputCursor* cursor, pair<FIRST, SECOND>* tuple) {
  return DecodeFromBuffer(cursor, &(tuple->first)) &&
         DecodeFromBuffer(cursor, &(tuple->second));
}

template<typename ELEMENT>
inline bool DecodeFromBuffer(
    OutputCursor* cursor, vector<ELEMENT>* data) {
  uint16_t size;
  if (!DecodeFromBuffer(cursor, &size))
    return false;
  data->resize(size);

  for (int i = 0; i < size; ++i) {
    ELEMENT temp;

    if (!DecodeFromBuffer(cursor, &temp))
      return false;
    (*data)[i] = temp;
  }

  return true;
}

template<typename ELEMENT>
inline bool EncodeToBuffer(
    const vector<ELEMENT>& data, InputCursor* cursor) {
  if (data.size() >= numeric_limits<uint16_t>::max())
    return false;

  EncodeToBuffer(static_cast<uint16_t>(data.size()), cursor);
  for (unsigned int i = 0; i < data.size(); ++i)
    EncodeToBuffer(data[i], cursor);

  return true;
}

#endif /* SERIALIZERS_H */
