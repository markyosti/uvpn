#include "base.h"
#include "errors.h"
#include "conversions.h"
#include "interfaces.h"
#include "stl-helpers.h"
#include "ip-addresses.h"

#include <string.h>
#include <string>

// TODO: make error messages / help screens usable.

void HandleRoutes(NetworkConfig* networking, int argc, char** argv) {
  if (argc < 1)
    LOG_FATAL("what do you want to do with?");

  const char* command(argv[0]);
  if (!strcmp(command, "show")) {
    list<RoutingEntry*> entries;
    AUTO_DELETE_ELEMENTS(entries);

    // TODO: this is ugly, use something better to align fields.
    printf("network            gateway         source          int metric\n");
    networking->GetRoutingTable()->GetRoutes(&entries);
    for (list<RoutingEntry*>::iterator it(entries.begin());
	 it != entries.end(); ++it) {
      RoutingEntry* entry(*it);

      printf("%-19s%-16s%-16s%-4d%d\n",
	     entry->destination.AsString().c_str(),
	     entry->gateway ? entry->gateway->AsString().c_str() : "connected",
	     entry->source ? entry->source->AsString().c_str() : "0.0.0.0",
	     entry->interface, entry->metric);
    }
  } else if (!strcmp(command, "add")) {
    if (argc < 3)
      LOG_FATAL("need interface and network address (in cidr format) to add");

    const char* device(argv[1]);
    Interface* interface(networking->GetInterface(device));
    if (!interface)
      LOG_FATAL("invalid interface name %s", device);

    IPRange network;
    if (!network.Parse(argv[2]))
      LOG_FATAL("invalid network");

    IPAddress* gateway(NULL);
    if (argc >= 4) {
      gateway = IPAddress::Parse(argv[3]);
      if (!gateway)
	LOG_FATAL("invalid gateway");
    }

    networking->GetRoutingTable()->AddRoute(
        *network.GetAddress(), network.GetCidr(), gateway, NULL, interface);
  } else if (!strcmp(command, "del")) {
    if (argc < 3)
      LOG_FATAL("need interface and network address (in cidr format) to del");

    const char* device(argv[1]);
    Interface* interface(networking->GetInterface(device));
    if (!interface)
      LOG_FATAL("invalid interface name %s", device);

    IPRange network;
    if (!network.Parse(argv[2]))
      LOG_FATAL("invalid network");

    IPAddress* gateway(NULL);
    if (argc >= 4) {
      gateway = IPAddress::Parse(argv[3]);
      if (!gateway)
	LOG_FATAL("invalid gateway");
    }

    networking->GetRoutingTable()->DelRoute(
        *network.GetAddress(), network.GetCidr(), gateway, NULL, interface);
  } else{
    LOG_FATAL("unknown command %s", command);
  }
}

void HandleInterface(NetworkConfig* networking, int argc, char** argv) {
  if (argc < 2)
    LOG_FATAL("what do you want to do with this interface?");

  const char* name(argv[0]);
  Interface* interface(networking->GetInterface(name));

  if (!interface)
    LOG_FATAL("interface %s is unknown.", name);

  const char* command(argv[1]);
  bool status;
  if (!strcmp(command, "up")) {
    status = interface->Enable();
  } else if (!strcmp(command, "down")) {
    status = interface->Disable();
  } else if (!strcmp(command, "add")) {
    if (argc < 3)
      LOG_FATAL("need network address (in cidr format) to add");

    const char* address(argv[2]);
    IPRange range;
    if (!range.Parse(address))
      LOG_FATAL("%s is an invalid address", address);
    status = interface->AddAddress(*range.GetAddress(), range.GetCidr());
  } else if (!strcmp(command, "del")) {
    if (argc < 3)
      LOG_FATAL("need network address (in cidr format) to del");

    const char* address(argv[2]);
    IPRange range;
    if (!range.Parse(address))
      LOG_FATAL("%s is an invalid address", address);
    status = interface->DelAddress(*range.GetAddress(), range.GetCidr());
  } else {
    LOG_FATAL("unknown command %s", command);
  }

  if (status)
    LOG_INFO("command succeeded");
  else
    LOG_INFO("command failed");
}

int main(int argc, char** argv) {
  if (argc < 2) {
    LOG_FATAL("you must specify an argument (only %d specified)", argc);
  }

  NetworkConfig networking;
  const char* command(argv[1]);
  if (!strcmp(command, "routes")) {
    HandleRoutes(&networking, argc - 2, argv + 2);
  } else if (!strcmp(command, "interface")) {
    HandleInterface(&networking, argc - 2, argv + 2);
  } else {
    LOG_FATAL("unknown command %s", command);
  }
}
