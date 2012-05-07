#ifndef CONVERSIONS_H
# define CONVERSIONS_H

# include "base.h"

# include <iostream>
# include <sstream>

inline void AppendCharToHex(char ch, string* str) {
  const char table[] = {
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
  };

  str->append(&table[(static_cast<unsigned char>(ch) & 0xf0) >> 4], 1);
  str->append(&table[static_cast<unsigned char>(ch) & 0xf], 1);
}

inline string ConvertToHex(const char* str, int size) {
  string hex;
  for (int i = 0; i < size; ++i) {
    AppendCharToHex(str[i], &hex);
  }

  return hex;
}

template<typename TYPE>
inline string ToString(const TYPE& data) {
  stringstream ss;
  ss << data;
  return ss.str();
}

template<typename TYPE>
inline bool FromString(const string& str, TYPE* output) {
  stringstream ss(str);
  //ss.exceptions(sstream::goodbit);
  ss >> *output;
  return !ss.fail();
}

#endif /* CONVERSIONS_H */
