#include "user-chatter.h"

UserChatter::InputName UserChatter::kInputNameUserName = "username";
UserChatter::InputName UserChatter::kInputNamePassword = "password";

const UserChatter::InputDescriptor UserChatter::kUserName = {
  UserChatter::T_STRING,
  UserChatter::kInputNameUserName,
  "Username",
  "The username to use to authenticate you with the remote server"
};

const UserChatter::InputDescriptor UserChatter::kPassword = {
  UserChatter::T_PASSWORD,
  UserChatter::kInputNamePassword,
  "Password",
  "The password associated with the specified username"
};
