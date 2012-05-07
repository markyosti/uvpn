#ifndef SERVER_UNETHICAL_TRANSCODER_H
# define SERVER_UNETHICAL_TRANSCODER_H

# include "server-transcoder.h"
# include "transport.h"
# include "packet-queue.h"
# include "sockaddr.h"
# include "server-authenticator.h"

class Dispatcher;
class ClientIOChannel;

class ServerUnethicalTranscoder : public ServerTranscoder {
 public:
  ServerUnethicalTranscoder(
      Dispatcher* dispatcher, Transport* transport, const Sockaddr& address,
      ServerTranscoder::protector_generator_t* generator);

  void Start(connect_handler_t* handler);

 private:
  class ConnectionDispatcherEncoder : public EncodeSessionProtector {
   public:
    virtual bool Start(
        OutputCursor* input, InputCursor* output, StartOptions options);
    virtual bool Continue(OutputCursor* input, InputCursor* output);
    virtual bool End(OutputCursor* input, InputCursor* output);

    virtual bool AddPadding(InputCursor* output, int datasize);
  };

  class ConnectionDispatcherDecoder : public DecodeSessionProtector {
   public:
    ConnectionDispatcherDecoder();
    virtual ~ConnectionDispatcherDecoder();

    virtual Result Start(
        OutputCursor* input, InputCursor* output, StartOptions options);
    virtual Result Continue(
        OutputCursor* input, InputCursor* output, uint32_t until);
    virtual Result End(OutputCursor* input, InputCursor* output);

    virtual Result RemovePadding(
        OutputCursor* output, int datasize, uint8_t* padsize);
  };

  Dispatcher* dispatcher_;
  Transport* transport_;
  ServerTranscoder::protector_generator_t* application_generator_;
  ServerTranscoder::connect_handler_t transcoder_connect_handler_;

  ConnectionDispatcherEncoder connection_dispatcher_encoder_;
  ConnectionDispatcherDecoder connection_dispatcher_decoder_;

  void ConnectCallback(EncodeSessionProtector* encoder,
		       DecodeSessionProtector* decoder,
		       ServerTranscoderSession* session);

  void GenerateProtector(
      EncodeSessionProtector** encoder, DecodeSessionProtector** decoder);

  vector<ServerTranscoder*> transcoders_;
};


#endif /* SERVER_UNETHICAL_TRANSCODER_H */
