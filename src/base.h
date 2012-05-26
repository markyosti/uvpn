#ifndef BASE_H
# define BASE_H

# include <stdint.h>
# include <memory>

using namespace std;

namespace std {
namespace tr1 {
}
}
using namespace std::tr1;

// Helpers to deal with the c++0x transition, some things are in std or tr1
// depending on compiler flags or version.
# ifdef __GXX_EXPERIMENTAL_CXX0X__
#  include <functional>
#  include <unordered_map>
#  define FUNCTOR_HASH(code) namespace std { code }
# else
#  include <tr1/functional>
#  include <tr1/unordered_map>
#  define FUNCTOR_HASH(code) namespace std { namespace tr1 { code } }
# endif
# define FUNCTOR_EQ(code) namespace std { code }

# define CLASS_ALIAS(name, original) class name : public original {}


#endif /* BASE_H */
