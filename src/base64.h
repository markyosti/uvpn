#ifndef BASE64_H
# define BASE64_H

# include "base.h"
# include <string>

void Base64Encode(const string& input, string* output);
bool Base64Decode(const string& input, string* output);

#endif /* BASE64_H */
