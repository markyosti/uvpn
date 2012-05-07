#include "base64.h"

void Base64Encode(const string& input, string* output) {
  static const char table[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  output->reserve((input.size() * 4) / 3 + 3);

  const unsigned char* cursor = reinterpret_cast<const unsigned char*>(input.c_str());
  const unsigned char* end = cursor + input.size();
  for (; cursor < end; cursor += 3) {
    (*output) += table[(*cursor & 0xfc) >> 2];
    if (end - cursor > 1) {
      (*output) += table[((*cursor & 0x03) << 4) | ((*(cursor + 1) & 0xf0) >> 4)];
    } else {
      (*output) += table[((*cursor & 0x03) << 4)];
      (*output) += "=";
      (*output) += "=";
      break;
    }

    if (end - cursor > 2) {
      (*output) += table[((*(cursor + 1) & 0x0f) << 2) | ((*(cursor + 2) & 0xc0) >> 6)];
      (*output) += table[(*(cursor + 2) & 0x3f)];
    } else {
      (*output) += table[((*(cursor + 1) & 0x0f) << 2)];
      (*output) += "=";
      break;
    }
  }
}

bool Base64Decode(const string& input, string* output) {
  static const char table[] = {
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63, 
      52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1, 
      -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 
      15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1, 
      -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 
      41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1, 
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
      };

  if (input.size() % 4)
    return false;
  output->reserve((input.size() >> 2) * 3);

  const unsigned char* cursor = reinterpret_cast<const unsigned char*>(input.c_str());
  const unsigned char* end = cursor + input.size();
  for (; cursor < end; cursor += 4) {
    if (table[*cursor] < 0 || table[*(cursor + 1)] < 0)
      return false;

    (*output) += static_cast<char>(table[*cursor] << 2 | table[*(cursor + 1)] >> 4);
    if (*(cursor + 2) == '=') {
      if (*(cursor + 3) != '=')
	return false;
      break;
    } 

    if (table[*(cursor + 2)] < 0)
      return false;

    (*output) += static_cast<char>((table[*(cursor + 1)] & 0xf) << 4 | table[*(cursor + 2)] >> 2);
    if (*(cursor + 3) == '=')
      break;

    if (table[*(cursor + 3)] < 0)
      return false;

    (*output) += static_cast<char>((table[*(cursor + 2)] & 0x03) << 6 | table[*(cursor + 3)]);
  }

  return true;
}
