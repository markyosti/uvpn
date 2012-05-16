// Copyright (c) 2008,2009,2010,2011 Mark Moreno (kramonerom@gmail.com).
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 
//    1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 
//    2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// 
// THIS SOFTWARE IS PROVIDED BY Mark Moreno ''AS IS'' AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
// EVENT SHALL Mark Moreno OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
// THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// 
// The views and conclusions contained in the software and documentation are
// those of the authors and should not be interpreted as representing official
// policies, either expressed or implied, of Mark Moreno.

#ifndef UVPN_CTL_H
# define UVPN_CTL_H

# include "dispatcher.h"
# include "socket-transport.h"
# include "daemon-controller-client.h"
# include "yaarg.h"

class UvpnCtl {
 public:
  UvpnCtl(ConfigParser* parser);
  bool Init();

 private:
  void CommandServerShowClients();
  void CommandClientConnect();

  StringOption type_;
  StringOption name_;

  Command cmd_server_;
  CallbackCommand cmd_server_show_clients_;
  Command cmd_server_show_users_;
  Command cmd_server_save_state_;

  Command cmd_client_;
  CallbackCommand cmd_client_connect_;
  Command cmd_client_disconnect_;
  Command cmd_client_save_state_;

  StringOption opt_server_;

  Dispatcher dispatcher_;
  SocketTransport transport_;
  DaemonControllerClient client_;
};

#endif /* UVPN_CTL_H */
