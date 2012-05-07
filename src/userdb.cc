#include "userdb.h"
#include "errors.h"
#include "base64.h"

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

UdbSecretFile::UdbSecretFile(const string& name) 
    : name_(name),
      fd_(-1) {
}

UdbSecretFile::~UdbSecretFile() {
  Close();
}

bool UdbSecretFile::LockFd(int fd) {
  struct flock lock;
  memset(&lock, 0, sizeof(struct flock));
  lock.l_type = F_WRLCK;
  lock.l_whence = SEEK_SET;
  lock.l_start = 0;
  lock.l_len = 0;

  if (fcntl(fd, F_SETLK, &lock) == -1)
    return false;
  return true;
}

bool UdbSecretFile::Open() {
  DEBUG_FATAL_UNLESS(fd_ == -1)(
      "opening already opened db?");

  fd_ = open(name_.c_str(), O_CREAT | O_RDWR, 0600);
  if (fd_ < 0) {
    LOG_PERROR("unable to open %s", name_.c_str());
    return false;
  }

  if (!LockFd(fd_)) {
    if (errno == EAGAIN) {
      LOG_ERROR("lock for %s held by a different process", name_.c_str());
      return false;
    }

    LOG_PERROR("unable to lock %s (continuing anyway)", name_.c_str());
  }

  if (!ParseFd(fd_, &users_)) {
    LOG_ERROR("userdb %s contains errors", name_.c_str());
    Close();
    return false;
  }

  return true;
}

bool UdbSecretFile::DelUser(const string& username) {
  if (fd_ < 0) {
    LOG_ERROR("cannot delete user %s - db is not opened.", username.c_str());
    return false;
  }

  map<string, string*>::iterator it = users_.find(username);
  if (it == users_.end())
    return false;

  delete it->second;
  users_.erase(it);
  return true;
}

bool UdbSecretFile::SetUser(const string& username, const string& secret) {
  if (fd_ < 0) {
    LOG_ERROR("cannot set user %s - db is not opened.", username.c_str());
    return false;
  }

  map<string, string*>::iterator it = users_.find(username);
  if (it == users_.end())
    return false;

  delete it->second;
  it->second = new string(secret);
  return true;
}

bool UdbSecretFile::AddUser(const string& username, const string& secret) {
  if (fd_ < 0) {
    LOG_ERROR("cannot add user %s - db is not opened.", username.c_str());
    return false;
  }

  map<string, string*>::const_iterator it = users_.find(username);
  if (it != users_.end()) {
    LOG_ERROR("cannot add user %s - an user by that name already exists", username.c_str());
    return false;
  }

  users_[username] = new string(secret);
  return true;
}

bool UdbSecretFile::GetUser(const string& username, string* secret) {
  if (fd_ < 0) {
    LOG_ERROR("cannot get user %s - db is not opened.", username.c_str());
    return false;
  }

  map<string, string*>::const_iterator it = users_.find(username);
  if (it != users_.end()) {
    if (secret)
      secret->assign(*it->second);
    return true;
  }

  return false;
}

bool UdbSecretFile::ParseFd(int fd, map<string, string*>* users) {
  if (lseek(fd, SEEK_SET, 0L) < 0)
    LOG_PERROR("unable to seek fd %d (continuing anyway)", fd);

  enum State {
    END_OF_RECORD,
    PARSING_USERNAME,
    PARSING_SECRET
  } state = END_OF_RECORD;
  char buffer[kBufferSize];

  string username;
  string secret;
  while (1) {
    int r = read(fd, &buffer, kBufferSize);
    if (r < 0) {
      if (errno == EAGAIN)
	continue;
      // FIXME
      break;
    }

    if (r == 0)
      break;

    int left = r;
    char* cursor = buffer;
    while (left > 0) {
      switch (state) {
        case END_OF_RECORD:
        case PARSING_USERNAME: {
          char* colon = static_cast<char*>(memchr(cursor, ':', left));
          if (!colon) {
            state = PARSING_USERNAME;
            username.append(cursor, left);
            cursor += left;
            left = 0;
            break;
          }
          username.append(cursor, colon - cursor);
          left -= (colon - cursor) + 1;
          cursor = colon + 1;
          // NO BREAK HERE!
        }

        case PARSING_SECRET: {
          char* eol = static_cast<char*>(memchr(cursor, '\n', left));
          if (!eol) {
            state = PARSING_SECRET;
            secret.append(cursor, left);
            cursor += left;
            left = 0;
            break;
          }
          secret.append(cursor, eol - cursor);
          left -= (eol - cursor) + 1;
          cursor = eol + 1;

          state = END_OF_RECORD;

          string* decoded = new string;
          Base64Decode(secret, decoded);
          (*users)[username] = decoded;
	  LOG_DEBUG("PARSED %s -> %s", username.c_str(), secret.c_str());
	  secret.clear();
	  username.clear();
        }
      }
    }
  }

  switch (state) {
    case PARSING_SECRET: {
      string* decoded = new string;
      Base64Decode(secret, decoded);
      (*users)[username] = decoded;
      break;
    }

    case END_OF_RECORD: {
      break;
    }

    case PARSING_USERNAME: {
      LOG_PERROR("userdb %d is corrupted (missing secret?)", fd);
      return false;
    }
  }

  return true;
}

bool UdbSecretFile::Close() {
  if (fd_ >= 0) {
    close(fd_);
    fd_ = -1;
  }

  for (map<string, string*>::const_iterator it = users_.begin();
       it != users_.end(); ++it) {
    delete it->second;
  }
  users_.clear();
  return true;
}

bool UdbSecretFile::Commit() {
  if (fd_ < 0) {
    LOG_ERROR("cannot commit %s - db is not opened.", name_.c_str());
    return false;
  }

  if (lseek(fd_, SEEK_SET, 0L) < 0)
    LOG_PERROR("unable to seek %s (continuing anyway)", name_.c_str());
  if (ftruncate(fd_, 0L) < 0)
    LOG_PERROR("unable to truncate %s (continuing anyway)", name_.c_str());

  for (map<string, string*>::const_iterator it = users_.begin();
       it != users_.end(); ++it) {
    // TODO: this does not work! (write EAGAIN, handle errors, ...)
    write(fd_, it->first.c_str(), it->first.size());
    write(fd_, ":", 1);

    string secret;
    Base64Encode(*it->second, &secret);
    LOG_DEBUG("DUMPING %s -> %s", it->first.c_str(), secret.c_str());
    write(fd_, secret.c_str(), secret.size());
    write(fd_, "\n", 1);
  }

  return true;
}

bool UdbSecretFile::ValidUsername(
    const string& username, string* reason) {
  if (username.find(':') != string::npos || username.find('\n') != string::npos
      || username.find('\n') != string::npos) {
    if (reason)
      reason->assign("usernames cannot contain ':', '\\n', or '\\0' character.");
    return false;
  }
  return true;
}
