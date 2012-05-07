#ifndef HASH_H
# define HASH_H

// this is mostly for size_t
// TODO: is this the same as std::size_t?
# include <sys/types.h>

# ifndef FNV_MAGIC
#  define FNV_MAGIC 0x01000193
# endif

inline size_t FNVHash(const char* tohash, size_t size, size_t seed) {
  register size_t hash = seed;
  unsigned const char* cur = reinterpret_cast<unsigned const char*>(tohash);
  unsigned const char* end = cur + size;

  while (cur < end) {
    hash *= FNV_MAGIC;
    hash ^= *cur++;
  }

  return hash;
}

inline size_t DragonHash(const char* tohash, size_t size, size_t seed) {
  register size_t hash = seed;
  register size_t tmp;

  unsigned const char* cur = reinterpret_cast<unsigned const char*>(tohash);
  unsigned const char* end = cur + size;

  /* Calculate hash */
  while (cur < end) {
    hash = (hash << 4) + *cur++;
    tmp = hash & 0xf0000000;
    if(tmp != 0) {
      hash ^= (tmp >> 24);
      hash ^= tmp;
    }
  }

  return hash;
}

# ifndef DEFAULT_HASH
#  define DEFAULT_HASH DragonHash
# endif

inline size_t DefaultHash(const char* tohash, size_t size, size_t seed) {
  return DEFAULT_HASH(tohash, size, seed);
}

inline size_t DefaultHash(const char* tohash, size_t size) {
  return DEFAULT_HASH(tohash, size, 0);
}

#endif /* HASH_H */
