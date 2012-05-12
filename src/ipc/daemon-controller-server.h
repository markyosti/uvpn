// Generated automatically from daemon-controller.ipc, on 2012-05-12 12:47:44.398388
// by running "generator.py ./daemon-controller.ipc"
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
  void ParseClientConnectRequest(OutputCursor* cursor) {
    string server;
    DecodeFromBuffer(cursor, &server);
    ProcessClientConnectRequest(server);
  }
  void ParseServerShowClientsRequest(OutputCursor* cursor) {
    ProcessServerShowClientsRequest();
  }
  void ParseGetParameterFromUserReply(OutputCursor* cursor) {
    vector<string> value;
    DecodeFromBuffer(cursor, &value);
    ProcessGetParameterFromUserReply(value);
  }

  // This is the method that will dispatch the incoming requests.
  int Dispatch(OutputCursor* cursor) {
    uint16_t num;
    if (!DecodeFromBuffer(cursor, &num)) {
      // TODO: error handling
      return -1;
    }

    switch (num) {
      case 1:
        ParseClientConnectRequest(cursor);;
        // TODO: error handling
        break;

      case 2:
        ParseServerShowClientsRequest(cursor);;
        // TODO: error handling
        break;

      case -16384:
        ParseGetParameterFromUserReply(cursor);;
        // TODO: error handling
        break;

      default:
        // TODO: error handling
        return -1;
    }

    return 0;
  }
};