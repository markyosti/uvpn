#!/usr/bin/python

"""
Generates C++ header files based on .ipc files.

Using IPCs in any programming language requires sending and receiving data.
Usually, you start by serializing an object representing your request, and
writing code to unserialize it and to invoke some code to handle the request.

This tool allows you to write a generic .ipc file describing the interface
between an IPC client and a server. Within this ipc file, you need to describe
which requests a client can issue to a server, and which replies it should
expect. The interface is bidirectional, so you can also describe requests
a server can issue to a client, and the replies it expects.

The generator will create .h files with the serialization and unserialization
code, and skeleton classes so you just have to implement the code to handle
the requests. The generated code is asynchronous.
"""

import argparse
import datetime
import os
import sys
import definitions
import textwrap

class InterfaceGenerator(object):
  def __init__(self):
    self.config = None
    self.interfaces = None

  def ParseFile(self, fname):
    self.config = {}
    parameters = dict(
        (key, getattr(definitions, key)) for key in dir(definitions))
    try:
      f = file(fname)
    except IOError as e:
      return "could not read file %s: %s" % (fname, e.strerror)
    try:
      exec f in parameters, self.config
    except Exception as e:
      return "file %s: %s" % (fname, str(e))
    try:
      self.interfaces = self.config["INTERFACES"]
    except KeyError:
      return "file %s: must define an 'INTERFACES' list."

    if type(self.interfaces) != type([]):
      return "file %s: 'INTERFACES' must be a list." % fname

  def GetClientInterfaces(self):
    result = []
    for interface in self.interfaces:
      result.append(interface.GetClientInterface())
    return "\n".join(result)

  def GetServerInterfaces(self):
    result = []
    for interface in self.interfaces:
      result.append(interface.GetServerInterface())
    return "\n".join(result)

def main(argv):
  parser = argparse.ArgumentParser(description="Generates C++ interface files.")
  parser.add_argument(
      "files", metavar="FILE", type=str, nargs="+",
      help="An interface definition file to process. Interface definition "
           "files are python files containing an INTERFACE list of Interface "
           "objects, as defined in definitions.py. See examples for more "
           "details.")
  parser.add_argument(
      "--client-template", "-c", type=str, nargs=1, default="%(filename)s-client.h",
      help="Template for the name of the server interface file to generate.")
  parser.add_argument(
      "--server-template", "-s", type=str, nargs=1, default="%(filename)s-server.h",
      help="Template for the name of the server interface file to generate.")

  BANNER = textwrap.dedent("""\
      // Generated automatically from %(input)s, on %(date)s
      // by running "%(args)s"
      // *** DO NOT MODIFY MANUALLY, otherwise your changes will be lost.***\n
      """)

  args = parser.parse_args()
  for fname in args.files:
    directory = os.path.dirname(fname)
    short_name = os.path.splitext(os.path.basename(fname))[0]
    banner = BANNER % {
      "input": os.path.basename(fname),
      "date": str(datetime.datetime.now()),
      "args": " ".join([os.path.basename(sys.argv[0])] + sys.argv[1:])
    }

    try:
      client_name = os.path.join(
          directory, str(args.client_template) % {"filename": short_name})
      server_name = os.path.join(
          directory, str(args.server_template) % {"filename": short_name})
    except KeyError as e:
      parser.error(
          "invalid --client-name or --server-name: unknown key %s" % str(e))

    generator = InterfaceGenerator()
    error = generator.ParseFile(fname)
    if error:
      print >>sys.stderr, "ERROR:", error
      continue

    print "+ processing %s" % (fname)
    try:
      interfaces = generator.GetClientInterfaces()
      print "  + generated client interface"
    except Exception as e:
      print >>sys.stderr, (
          "ERROR: %s, cannot generate client interface: %s" % (fname, str(e)))

    try:
      open(client_name, "w").write(banner + interfaces)
      print "  + file %s written" % (client_name)
    except IOError as e:
      print >>sys.stderr, (
          "ERROR: could not save client interface: %s" % str(e))

    try:
      interfaces = generator.GetServerInterfaces()
      print "  + generated server interface"
    except Exception as e:
      print >>sys.stderr, (
          "ERROR: %s, cannot generate server interface: %s" % (fname, str(e)))

    try:
      open(server_name, "w").write(banner + interfaces)
      print "  + file %s written" % (server_name)
    except IOError as e:
      print >>sys.stderr, (
          "ERROR: could not save server interface: %s" % str(e))

if __name__ == "__main__":
  sys.exit(main(sys.argv))
