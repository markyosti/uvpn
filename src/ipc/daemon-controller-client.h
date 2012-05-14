// Generated automatically from daemon-controller.ipc, on 2012-05-13 18:16:36.645483
// by running "generator.py ipc/daemon-controller.ipc"
// *** DO NOT MODIFY MANUALLY, otherwise your changes will be lost.***

class DaemonControllerClientIpc : public IpcClientInterface {
 public:
  // You have to implement the methods here to process incoming requests.
  virtual void ProcessClientConnectReply(void) = 0;
  virtual void ProcessServerShowClientsReply(const vector<string>& client) = 0;
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
  void SendReplyForGetParameterFromUser(const vector<string>& value) {
    EncodeToBuffer(static_cast<int16_t>(-16384), SendCursor());
    EncodeToBuffer(value, SendCursor());
    Send();
  }

 private:
  // Forget about the methods here, not for you.
  int ParseClientConnectReply(OutputCursor* cursor) {
    ProcessClientConnectReply();
    return 0;
  }
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
      case -1:
        result = ParseClientConnectReply(cursor);;
        break;

      case -2:
        result = ParseServerShowClientsReply(cursor);;
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