#ifndef USERDB_H
# define USERDB_H

# include "base.h"
# include "password.h"

# include <map>
# include <string>

class UserSecret {
 public:
  virtual ~UserSecret() {}

  virtual bool FromPassword(const string& username, const ScopedPassword& pwd) = 0;
  virtual bool FromSecret(const string& username, const string& secret) = 0;

  virtual void ToSecret(string* dump) = 0;
  virtual void ToAscii(string* dump) = 0;
};

class UserDb {
 public:
  UserDb() {}
  virtual ~UserDb() {}

  virtual bool Open() = 0;
  virtual bool Close() = 0;

  virtual bool Commit() = 0;

  virtual bool ValidUsername(const string& username, string* reason) = 0;

  virtual bool GetUser(const string& username, string* secret) = 0;
  virtual bool AddUser(const string& username, const string& secret) = 0;
  virtual bool SetUser(const string& username, const string& secret) = 0;

  virtual bool DelUser(const string& username) = 0;
};

class UserDbSession {
 public:
  UserDbSession(UserDb* udb) : udb_(udb) {
    udb_->Open();
  }

  ~UserDbSession() {
    udb_->Close();
  }

 private:
  UserDb* udb_;
};

class UdbSecretFile : public UserDb {
  static const int kBufferSize = 16384;

 public:
  UdbSecretFile(const string& name);
  virtual ~UdbSecretFile();

  virtual bool Open();
  virtual bool Close();

  virtual bool Commit();

  virtual bool ValidUsername(const string& username, string* reason);

  virtual bool GetUser(const string& username, string* secret);
  virtual bool AddUser(const string& username, const string& secret);
  virtual bool SetUser(const string& username, const string& secret);

  virtual bool DelUser(const string& username);

 private:
  static bool LockFd(int fd);
  static bool ParseFd(int fd, map<string, string*>* users);

  static void DecodeSecret(const string& secret, string* output);
  static void EncodeSecret(const string& secret, string* output);

  const string name_;
  map<string, string*> users_;
  int fd_;
};

#endif /* USERDB_H */
