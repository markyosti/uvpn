// Generated automatically from daemon-controller.ipc, on 2012-05-15 21:10:53.885787
// by running "generator.py ipc/daemon-controller.ipc"
// *** DO NOT MODIFY MANUALLY, otherwise your changes will be lost.***

#ifndef IPC_DAEMON_CONTROLLER_CLIENT_H
# define IPC_DAEMON_CONTROLLER_CLIENT_H
class DaemonControllerClientIpc : public IpcClientInterface {
 public:
  // You have to implement the methods here to process incoming requests.
  virtual void ProcessServerShowClientsReply(const vector<string>& client) = 0;
  virtual void ProcessRequestServerStatusReply(const vector<string>& status) = 0;
  virtual void ProcessGetParameterFromUserRequest(const vector<string>& name) = 0;

  // The methods here are the ones you can use to send requests.
  void SendRequestClientConnect(const string& server) {
    EncodeToBuffer(static_cast<int16_t>(1), SendCursor());
    EncodeToBuffer(server, SendCursor());
    Send();
  }
  void SendRequestServerShowClients(void) {
    EncodeToBuffer(static_cast<int16_t>(2), SendCursor());
    Send();
  }
  void SendRequestRequestServerStatus(const vector<string>& server) {
    EncodeToBuffer(static_cast<int16_t>(3), SendCursor());
    EncodeToBuffer(server, SendCursor());
    Send();
  }
  void SendReplyForGetParameterFromUser(const vector<string>& value) {
    EncodeToBuffer(static_cast<int16_t>(-16384), SendCursor());
    EncodeToBuffer(value, SendCursor());
    Send();
  }

 private:
  // Forget about the methods here, not for you.
  int ParseServerShowClientsReply(OutputCursor* cursor) {
    vector<string> client;
    int result;
    if ((result = DecodeFromBuffer(cursor, &client))) {
      LOG_DEBUG("unserialization returned %d", result);
      return result;
    }
    ProcessServerShowClientsReply(client);
    return 0;
  }
  int ParseRequestServerStatusReply(OutputCursor* cursor) {
    vector<string> status;
    int result;
    if ((result = DecodeFromBuffer(cursor, &status))) {
      LOG_DEBUG("unserialization returned %d", result);
      return result;
    }
    ProcessRequestServerStatusReply(status);
    return 0;
  }
  int ParseGetParameterFromUserRequest(OutputCursor* cursor) {
    vector<string> name;
    int result;
    if ((result = DecodeFromBuffer(cursor, &name))) {
      LOG_DEBUG("unserialization returned %d", result);
      return result;
    }
    ProcessGetParameterFromUserRequest(name);
    return 0;
  }

  // This is the method that will dispatch the incoming requests.
  int Dispatch(OutputCursor* cursor) {
    int16_t num;
    int result = DecodeFromBuffer(cursor, &num);
    if (result) {
      LOG_DEBUG("while looking for rpc number, got status %d", result);
      return result;
    }

    switch (num) {
      case -2:
        result = ParseServerShowClientsReply(cursor);;
        break;

      case -3:
        result = ParseRequestServerStatusReply(cursor);;
        break;

      case 16384:
        result = ParseGetParameterFromUserRequest(cursor);;
        break;

      default:
        LOG_ERROR("unknonw RPC number %d, ignoring", num);
        return -1;
    }

    return result;
  }
};
#endif  /* IPC_DAEMON_CONTROLLER_CLIENT_H */