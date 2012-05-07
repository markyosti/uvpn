#ifndef TERMINAL_USER_CHATTER_H
# define TERMINAL_USER_CHATTER_H

# include "user-chatter.h"

class TerminalUserChatter : public UserChatter {
 public:
  bool Get(const InputDescriptor& descriptor, StringBuffer* data);
};

#endif /* TERMINAL_USER_CHATTER_H */
