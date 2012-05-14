// Generated automatically from daemon-controller.ipc, on 2012-05-13 18:16:36.645483
// by running "generator.py ipc/daemon-controller.ipc"
// *** DO NOT MODIFY MANUALLY, otherwise your changes will be lost.***

class DaemonControllerServerIpc : public IpcServerInterface {
 public:
  // You have to implement the methods here to process incoming requests.
  virtual void ProcessClientConnectRequest(const string& server) = 0;
  virtual void ProcessServerShowClientsRequest(void) = 0;
  virtual void ProcessGetParameterFromUserReply(const vector<string>& value) = 0;

  // The methods here are the ones you can use to send requests.
  void SendReplyForClientConnect(void) {
    EncodeToBuffer(static_cast<int16_t>(-1), SendCursor());
    Send();
  }
  void SendReplyForServerShowClients(const vector<string>& client) {
    EncodeToBuffer(static_cast<int16_t>(-2), SendCursor());
    EncodeToBuffer(client, SendCursor());
    Send();
  }
  void SendRequestGetParameterFromUser(const vector<string>& name) {
    EncodeToBuffer(static_cast<int16_t>(16384), SendCursor());
    EncodeToBuffer(name, SendCursor());
    Send();
  }

 private:
  // Forget about the methods here, not for you.
  int ParseClientConnectRequest(OutputCursor* cursor) {
    string server;
    int result;
    if ((result = DecodeFromBuffer(cursor, &server))) {
      LOG_DEBUG("unserialization returned %d", result);
      return result;
    }
    ProcessClientConnectRequest(server);
    return 0;
  }
  int ParseServerShowClientsRequest(OutputCursor* cursor) {
    ProcessServerShowClientsRequest();
    return 0;
  }
  int ParseGetParameterFromUserReply(OutputCursor* cursor) {
    vector<string> value;
    int result;
    if ((result = DecodeFromBuffer(cursor, &value))) {
      LOG_DEBUG("unserialization returned %d", result);
      return result;
    }
    ProcessGetParameterFromUserReply(value);
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
      case 1:
        result = ParseClientConnectRequest(cursor);;
        break;

      case 2:
        result = ParseServerShowClientsRequest(cursor);;
        break;

      case -16384:
        result = ParseGetParameterFromUserReply(cursor);;
        break;

      default:
        LOG_ERROR("unknonw RPC number %d, ignoring", num);
        return -1;
    }

    return result;
  }
};