// Generated automatically from daemon-controller.ipc, on 2012-05-12 12:47:44.398388
// by running "generator.py ./daemon-controller.ipc"
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
  void ParseClientConnectReply(OutputCursor* cursor) {
    ProcessClientConnectReply();
  }
  void ParseServerShowClientsReply(OutputCursor* cursor) {
    vector<string> client;
    DecodeFromBuffer(cursor, &client);
    ProcessServerShowClientsReply(client);
  }
  void ParseGetParameterFromUserRequest(OutputCursor* cursor) {
    vector<string> name;
    DecodeFromBuffer(cursor, &name);
    ProcessGetParameterFromUserRequest(name);
  }

  // This is the method that will dispatch the incoming requests.
  int Dispatch(OutputCursor* cursor) {
    uint16_t num;
    if (!DecodeFromBuffer(cursor, &num)) {
      // TODO: error handling
      return -1;
    }

    switch (num) {
      case -1:
        ParseClientConnectReply(cursor);;
        // TODO: error handling
        break;

      case -2:
        ParseServerShowClientsReply(cursor);;
        // TODO: error handling
        break;

      case 16384:
        ParseGetParameterFromUserRequest(cursor);;
        // TODO: error handling
        break;

      default:
        // TODO: error handling
        return -1;
    }

    return 0;
  }
};