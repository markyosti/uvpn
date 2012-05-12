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

CLIENT_RPC_REPLY = (
    "  virtual void Process%(client_rpc_name)sReply("
    "%(client_rpc_receive_params)s) = 0;")
CLIENT_RPC_SEND_START = "  void SendRequest%(client_rpc_name)s(%(client_rpc_send_params)s) {%(client_send_serialization)s"
CLIENT_RPC_SEND_END = "    Send();\n  }"

CLIENT_RPC_RECEIVE = (
    "  virtual void Process%(client_rpc_name)sRequest("
    "%(client_rpc_send_params)s) = 0;")
CLIENT_RPC_REPLY_START = "  void SendReplyFor%(client_rpc_name)s(%(client_rpc_receive_params)s) {%(client_send_serialization)s"
CLIENT_RPC_REPLY_END = "    Send();\n  }"

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

  def OutputServerInterface(self):
    return self.OutputInterface(
        self.server_class_name, "Server",
        "GetClientReceiveInterface", "GetClientSendInterface")

  def OutputClientInterface(self):
    return self.OutputInterface(
        self.client_class_name, "Client",
        "GetClientSendInterface", "GetClientReceiveInterface")

  def OutputInterface(self, cname, ctype, send_method, receive_method):
    methods_to_implement = []
    methods_public = []
    methods_private = []

    for num, rpc in enumerate(self.sends, 1):
      mti, mpub, mpriv = getattr(rpc, send_method)(num)

      methods_to_implement.extend(mti)
      methods_public.extend(mpub)
      methods_private.extend(mpriv)

    for num, rpc in enumerate(self.receives, 0x4000):
      mti, mpub, mpriv = getattr(rpc, receive_method)(num)

      methods_to_implement.extend(mti)
      methods_public.extend(mpub)
      methods_private.extend(mpriv)

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

    receive_args = []
    for recv in self.receives:
      receive_args.append(recv.GetReceiveType() + " " + recv.name)

    return ", ".join(send_args) or "void", ", ".join(receive_args) or "void",

  def GetSerializationCode(self, num, sends, receives):
    send_args = ["    " + "EncodeToBuffer(static_cast<int16_t>(" + str(num) + "), SendCursor());"]
    for send in sends:
      send_args.append("    " + send.GetSerializationCode())

    receive_args = []
    for recv in receives:
      receive_args.append("    " + recv.GetUnserializationCode())
    
    if send_args:
      send_args = "\n" + "\n".join(send_args)
    else:
      send_args = ""
    if receive_args:
      receive_args = "\n" + "\n".join(receive_args)
    else:
      receive_args = ""
    return send_args, receive_args

  def GetClientSendInterface(self, num):
    send_args, receive_args = self.GetArguments()
    send_serialization, receive_serialization = self.GetSerializationCode(
        num, self.sends, self.receives)

    args = {
      "client_rpc_name": self.name,
      "client_send_serialization": send_serialization,
      "client_receive_serialization": receive_serialization,
      "client_rpc_send_params": send_args,
      "client_rpc_receive_params": receive_args,
    }
    to_implement = [CLIENT_RPC_REPLY % args]
    public = [CLIENT_RPC_SEND_START % args, CLIENT_RPC_SEND_END % args]
    return to_implement, public, []

  def GetClientReceiveInterface(self, num):
    send_args, receive_args = self.GetArguments()
    send_serialization, receive_serialization = self.GetSerializationCode(
        -num, self.receives, self.sends)

    args = {
      "client_rpc_name": self.name,
      "client_send_serialization": send_serialization,
      "client_receive_serialization": receive_serialization,
      "client_rpc_send_params": send_args,
      "client_rpc_receive_params": receive_args,
    }

    to_implement = [CLIENT_RPC_RECEIVE % args]
    public = [CLIENT_RPC_REPLY_START % args, CLIENT_RPC_REPLY_END % args]
    return to_implement, public, []


class Parameter(object):
  def __init__(self, name):
    self.name = name

  def GetSerializationCode(self):
    return "EncodeToBuffer(" + self.name + ", SendCursor());"

  def GetUnserializationCode(self):
    return "DecodeFromBuffer(ReceiveCursor(), &" + self.name + ");"


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

  def GetSendType(self):
    return "const vector<" + self.other.GetSimpleType() + ">&"

  def GetReceiveType(self):
    return "const vector<" + self.other.GetSimpleType() + ">&"
