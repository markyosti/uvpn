#ifndef USER_CHATTER_H
# define USER_CHATTER_H

# include "base.h"
# include "password.h"

class UserChatter {
 public:
  enum InputType {
    // Any character, not displayed on the screen.
    T_PASSWORD,
    // A user readable string (eg, a username, server, ...).
    T_STRING,
    // A user readable numeric token (eg, an otp).
    T_NUMERIC
  };

  typedef const char* InputName;
  static InputName kInputNameUserName;
  static InputName kInputNamePassword;

  struct InputDescriptor {
    // The type of input to be expected.
    InputType type; // describes the allowed characters and input kind.
    InputName name; // describes what the input is, uniquely.

    // Which prompt to show to the user? UserName? Password?
    // Kerberos realm? gimme an otp?
    const char* prompt;

    // OnMouseOver or in help screen, what is this field?
    const char* description;
  };

  static const InputDescriptor kUserName;
  static const InputDescriptor kPassword;

  virtual bool Get(const InputDescriptor& descriptor, StringBuffer* data) = 0;
};

#endif /* USER_CHATTER_H */
