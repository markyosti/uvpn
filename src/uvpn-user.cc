#include "base.h"
#include "userdb.h"
#include "errors.h"
#include "terminal.h"
#include "conversions.h"

#include "srp-common.h"
#include "srp-passwd.h"

#include <string.h>
#include <string>

// TODO: move this in password.h and password.cc (in ScopedPassword).
bool GetPassword(ScopedPassword* retval) {
  Terminal terminal;
  for (int attempt = 0; attempt < 3; attempt++) {
    ScopedPassword password;
    if (!terminal.ReadPassword("Insert password: ", retval)) {
      LOG_ERROR("password read %s", "failed");
      return false;
    }
  
    ScopedPassword confirm;
    if (!terminal.ReadPassword("Confirm password: ", &confirm)) {
      LOG_ERROR("password read %s", "failed");
      return false;
    }

    if (password.SameAs(confirm))
      return true;
  }

  LOG_ERROR("failed to retrieve password: "
	"user must be able to type the same password twice in less than %d attempts", 3);
  return false;
}

bool AddUser(UserDb* udb, UserSecret* secret, int argc, char** argv) {
  if (argc < 1) {
    LOG_ERROR("no username supplied (%d)", argc);
    return false;
  }
  const string username(argv[0]);

  string reason;
  if (!udb->ValidUsername(username, &reason)) {
    LOG_ERROR("username '%s' is invalid - %s", username.c_str(), reason.c_str());
    return false;
  }

  ScopedPassword password;
  if (!GetPassword(&password)) {
    return false;
  }
  // TODO(SECURITY,DEBUG): remove this.
  LOG_DEBUG("password: %*s, %s", password.Used(), password.Data(),
	    ConvertToHex(password.Data(), password.Used()).c_str());

  string tostore;
  if (!secret->FromPassword(username, password)) {
    return false;
  }
  secret->ToSecret(&tostore);

  // TODO(SECURITY,DEBUG): remove this.
  string dump;
  secret->ToAscii(&dump);
  LOG_DEBUG("dump: %s", dump.c_str());

  UserDbSession session(udb);
  if (!udb->AddUser(username, tostore)) {
    // FIXME!
    return false;
  }


  if (!udb->Commit()) {
    // FIXME!
    return false;
  }
  return true;
}

bool DelUser(UserDb* udb, int argc, char** argv) {
  if (argc < 1) {
    LOG_ERROR("no username supplied (%d)", argc);
    return false;
  }
  const string username(argv[0]);

  UserDbSession session(udb);
  if (!udb->DelUser(username)) {
    // FIXME!
    return false;
  }

  if (!udb->Commit()) {
    // FIXME!
    return false;
  }
  return true;
}

bool GetUser(UserDb* udb, UserSecret* secret, int argc, char** argv) {
  if (argc < 1) {
    LOG_ERROR("no username supplied (%d)", argc);
    return false;
  }
  const string username(argv[0]);

  UserDbSession session(udb);
  string stored;
  if (!udb->GetUser(username, &stored)) {
    LOG_ERROR("user %s does not exist", username.c_str());
    return false;
  }

  if (!secret->FromSecret(username, stored)) {
    // FIXME!
    LOG_ERROR("invalid entry for user %s", username.c_str());
    return false;
  }

  string dump;
  secret->ToAscii(&dump);

  printf("username: %s\n%s\n", username.c_str(), dump.c_str());
  return true;
}

bool GenerateUser(UserDb* udb, int argc, char** argv) {
  return false;
}

int main(int argc, char** argv) {
  if (argc < 2) {
    LOG_FATAL("you must specify an argument (only %d specified)", argc);
  }

  UdbSecretFile udb("/root/uvpn.passwd");
  SecurePrimes primes;
  // FIXME: instead of "1" below, choose a key based on command line options.
  SrpSecret secret(primes, 1);

  const char* command = argv[1];
  if (!strcmp(command, "add")) {
    AddUser(&udb, &secret, argc - 2, argv + 2);
  } else if (!strcmp(command, "del")) {
    DelUser(&udb, argc - 2, argv + 2);
  } else if (!strcmp(command, "change")) {
//    ChangeUser(argc - 2, argv + 2);
  } else if (!strcmp(command, "generate")) {
//    GenerateUser(argc - 2, argv + 2);
  } else if (!strcmp(command, "get")) {
    GetUser(&udb, &secret, argc - 2, argv + 2);
  } else {
    LOG_FATAL("unknown command %s", command);
  }
}
