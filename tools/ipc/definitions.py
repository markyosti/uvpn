#!/usr/bin/python

import textwrap
import string

COMMENT_IMPLEMENT_METHODS_HERE = (
    "  // You have to implement the methods here to process incoming requests.")
COMMENT_METHODS_TO_SEND_HERE = (
    "\n  // The methods here are the ones you can use to send requests.")
PRIVATE_METHODS = (
    "\n private:\n  // Forget about the methods here, not for you.")


INTERFACE_SERVER_SUFFIX = "ServerIpc"
INTERFACE_CLIENT_SUFFIX = "ClientIpc"

INTERFACE_START = (
"""class %(class_name)s : public Ipc%(class_type)sInterface {
 public:""")


CLIENT_RPC_REQUEST_DISPATCH = (
    "  virtual void Process%(name)sRequest("
    "%(send_params)s) = 0;")
CLIENT_RPC_SEND_REQUEST_START = "  void SendRequest%(name)s(%(send_params)s) {%(serialize_request)s"
CLIENT_RPC_SEND_REQUEST_END = "    Send();\n  }"
CLIENT_RPC_REQUEST_PARSE_START = "  void Parse%(name)sRequest(OutputCursor* cursor) {%(unserialize_request)s"
CLIENT_RPC_REQUEST_PARSE_END = "    Process%(name)sRequest(%(send_unqual)s);\n  }"
CLIENT_RPC_CALL_REQUEST_DISPATCH = "      Parse%(name)sRequest(cursor);"

CLIENT_RPC_REPLY_DISPATCH = (
    "  virtual void Process%(name)sReply("
    "%(receive_params)s) = 0;")
CLIENT_RPC_SEND_REPLY_START = "  void SendReplyFor%(name)s(%(receive_params)s) {%(serialize_reply)s"
CLIENT_RPC_SEND_REPLY_END = "    Send();\n  }"
CLIENT_RPC_REPLY_PARSE_START = "  void Parse%(name)sReply(OutputCursor* cursor) {%(unserialize_reply)s"
CLIENT_RPC_REPLY_PARSE_END = "    Process%(name)sReply(%(receive_unqual)s);\n  }"
CLIENT_RPC_CALL_REPLY_DISPATCH = "      Parse%(name)sReply(cursor);"

DISPATCH_START = "\n  // This is the method that will dispatch the incoming requests.\n  int Dispatch(OutputCursor* cursor) {"
DISPATCH_FUNCTION = \
"""\
      case %(dispatch_num)s:
%(dispatch_function)s;
        // TODO: error handling
        break;
"""
DISPATCH_BODY = \
"""\
    uint16_t num;
    if (!DecodeFromBuffer(cursor, &num)) {
      // TODO: error handling
      return -1;
    }

    switch (num) {
%(dispatch_functions)s
      default:
        // TODO: error handling
        return -1;
    }

    return 0;"""

DISPATCH_END = "  }"

INTERFACE_END = "};"


class Interface(object):
  def __init__(self, name, sends=[], receives=[]):
    self.name = name
    self.sends = sends
    self.receives = receives
    self.client_class_name = "%s%s" % (name, INTERFACE_CLIENT_SUFFIX)
    self.server_class_name = "%s%s" % (name, INTERFACE_SERVER_SUFFIX)

  def FormatMethods(self, cname, ctype, to_implement, public, private):
    result = []
    result.append(INTERFACE_START % {
        "class_name": cname, "class_type": ctype}) 
    if to_implement:
      result.append(COMMENT_IMPLEMENT_METHODS_HERE)
      result.extend(to_implement)
    if public:
      result.append(COMMENT_METHODS_TO_SEND_HERE)
      result.extend(public)
    if private:
      result.append(PRIVATE_METHODS)
      result.extend(private)
    result.append(INTERFACE_END)
    return "\n".join(result)

  def GetServerInterface(self):
    return self.GetInterface(
        self.server_class_name, "Server",
        "GetClientReceiveInterface", "GetClientSendInterface")

  def GetClientInterface(self):
    return self.GetInterface(
        self.client_class_name, "Client",
        "GetClientSendInterface", "GetClientReceiveInterface")

  def GenerateDispatcher(self, to_dispatch):
    functions = []
    for num, method in to_dispatch:
      functions.append(DISPATCH_FUNCTION % {
          "dispatch_num": num, "dispatch_function": "  " + method})
      
    return [
        DISPATCH_START,
        DISPATCH_BODY % {"dispatch_functions": "\n".join(functions)},
        DISPATCH_END]

  def GetInterface(self, cname, ctype, send_method, receive_method):
    methods_to_implement = []
    methods_public = []
    methods_private = []
    to_dispatch = []

    for num, rpc in enumerate(self.sends, 1):
      mti, mpub, mpriv, disp = getattr(rpc, send_method)(num)

      methods_to_implement.extend(mti)
      methods_public.extend(mpub)
      methods_private.extend(mpriv)
      to_dispatch.extend(disp)

    for num, rpc in enumerate(self.receives, 0x4000):
      mti, mpub, mpriv, disp = getattr(rpc, receive_method)(num)

      methods_to_implement.extend(mti)
      methods_public.extend(mpub)
      methods_private.extend(mpriv)
      to_dispatch.extend(disp)

    methods_private.extend(self.GenerateDispatcher(to_dispatch))
    return self.FormatMethods(
        cname, ctype, methods_to_implement, methods_public, methods_private)

class Rpc(object):
  def __init__(self, name, sends=[], receives=[]):
    self.name = name
    self.sends = sends
    self.receives = receives

  def GetArguments(self):
    send_args = []
    for send in self.sends:
      send_args.append(send.GetSendType() + " " + send.name)
    send_unqual = [s.name for s in self.sends]

    receive_args = []
    for recv in self.receives:
      receive_args.append(recv.GetReceiveType() + " " + recv.name)
    receive_unqual = [s.name for s in self.receives]

    return (", ".join(send_args) or "void",
            ", ".join(send_unqual),
            ", ".join(receive_args) or "void",
            ", ".join(receive_unqual))

  def GetSerializationCode(self, num, sends, receives):
    serialize_request = ["    " + "EncodeToBuffer(static_cast<int16_t>(" + str(num) + "), SendCursor());"]
    for send in sends:
      serialize_request.append("    " + "\n    ".join(send.GetSerializationCode()))
    unserialize_request = []
    for send in sends:
      unserialize_request.append("    " + "\n    ".join(send.GetUnserializationCode()))

    serialize_reply = ["    " + "EncodeToBuffer(static_cast<int16_t>(" + str(-num) + "), SendCursor());"]
    for recv in receives:
      serialize_reply.append("    " + "\n    ".join(recv.GetSerializationCode()))
    unserialize_reply = []
    for recv in receives:
      unserialize_reply.append("    " + "\n    ".join(recv.GetUnserializationCode()))

    if serialize_reply:
      serialize_reply = "\n" + "\n".join(serialize_reply)
    else:
      serialize_reply = ""
    if serialize_request:
      serialize_request = "\n" + "\n".join(serialize_request)
    else:
      serialize_request = ""

    if unserialize_reply:
      unserialize_reply = "\n" + "\n".join(unserialize_reply)
    else:
      unserialize_reply = ""
    if unserialize_request:
      unserialize_request = "\n" + "\n".join(unserialize_request)
    else:
      unserialize_request = ""

    return serialize_reply, serialize_request, unserialize_reply, unserialize_request

  def GetExpansionDict(self, num, sends, receives):
    send_args, send_unqual, receive_args, receive_unqual = self.GetArguments()
    (serialize_reply, serialize_request,
     unserialize_reply, unserialize_request) = self.GetSerializationCode(
        num, sends, receives)

    args = {
      "name": self.name,
      "serialize_request": serialize_request,
      "serialize_reply": serialize_reply,
      "unserialize_reply": unserialize_reply,
      "unserialize_request": unserialize_request,
      "send_params": send_args,
      "send_unqual": send_unqual,
      "receive_unqual": receive_unqual,
      "receive_params": receive_args,
    }
    return args

  def GetClientSendInterface(self, num):
    args = self.GetExpansionDict(num, self.sends, self.receives)
    to_implement = [CLIENT_RPC_REPLY_DISPATCH % args]
    public = [CLIENT_RPC_SEND_REQUEST_START % args, CLIENT_RPC_SEND_REQUEST_END % args]
    private = [CLIENT_RPC_REPLY_PARSE_START % args, CLIENT_RPC_REPLY_PARSE_END % args]
    return to_implement, public, private, [(-num, CLIENT_RPC_CALL_REPLY_DISPATCH % args)]

  def GetClientReceiveInterface(self, num):
    args = self.GetExpansionDict(num, self.sends, self.receives)
    to_implement = [CLIENT_RPC_REQUEST_DISPATCH % args]
    public = [CLIENT_RPC_SEND_REPLY_START % args, CLIENT_RPC_SEND_REPLY_END % args]
    private = [CLIENT_RPC_REQUEST_PARSE_START % args, CLIENT_RPC_REQUEST_PARSE_END % args]
    return to_implement, public, private, [(num, CLIENT_RPC_CALL_REQUEST_DISPATCH % args)]


class Parameter(object):
  def __init__(self, name):
    self.name = name

  def GetSerializationCode(self):
    return ["EncodeToBuffer(" + self.name + ", SendCursor());"]

  def GetUnserializationCode(self):
    return ["%s %s;" % (self.GetSimpleType(), self.name),
            "DecodeFromBuffer(cursor, &%s);" % (self.name)]


class StringParameter(Parameter):
  def GetSimpleType(self):
    return "string"

  def GetSendType(self):
    return "const string&"

  def GetReceiveType(self):
    return "const string&"


class Repeated(Parameter):
  def __init__(self, other):
    self.name = other.name
    self.other = other

  def GetSimpleType(self):
    return "vector<" + self.other.GetSimpleType() + ">"

  def GetSendType(self):
    return "const vector<" + self.other.GetSimpleType() + ">&"

  def GetReceiveType(self):
    return "const vector<" + self.other.GetSimpleType() + ">&"
