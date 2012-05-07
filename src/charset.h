#ifndef CHARSET_H
# define CHARSET_H

# include <string.h>

# include "macros.h"

class Charset {
 public:
  Charset() {
    memset(map_, 0, sizeof_array(map_));
  }

  bool Valid(char ch) const;
  bool Invalid(char ch) const;
  void Add(char ch);
  void Del(char ch);

 private:
  int Offset(char ch) const;
  unsigned int Mask(char ch) const;

  unsigned int map_[256 / sizeof_in_bits(unsigned int)];
};

inline int Offset(char ch) {
  return static_cast<unsigned char>(ch) / sizeof_in_bits(unsigned int);
}

inline int Mask(char ch) {
  return static_cast<unsigned char>(ch) % sizeof_in_bits(unsigned int);
}

inline void Charset::Add(char ch) {
  map_[Offset(ch)] |= Mask(ch);
}

inline void Charset::Del(char ch) {
  map_[Offset(ch)] &= ~Mask(ch);
}

inline bool Charset::Valid(char ch) const {
  return map_[Offset(ch)] & Mask(ch);
}

inline bool Charset::Invalid(char ch) const {
  return !(map_[Offset(ch)] & Mask(ch));
}

#endif /* CHARSET_H */
