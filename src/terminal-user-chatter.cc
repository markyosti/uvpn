#include "terminal-user-chatter.h"
#include "terminal.h"

bool TerminalUserChatter::Get(
    const InputDescriptor& descriptor, StringBuffer* data) {
  bool status(true);

  Terminal terminal;
  terminal.Print(descriptor.prompt);
  terminal.Print(": ");
  switch (descriptor.type) {
    case T_PASSWORD: {
      status = terminal.ReadPassword("", data);
      break;
    }

    case T_STRING:
    case T_NUMERIC: {
      int read = terminal.ReadLine(data->Data(), data->Size());
      if (read < 0)
	status = false;
      else
        data->Resize(read);
      break;
    }
  }

  return status;
}
