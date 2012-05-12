#!/usr/bin/python

import sys
import definitions

class InterfaceGenerator(object):
  def __init__(self):
    self.config = None
    self.interfaces = None

  def ParseFile(self, fname):
    self.config = {}
    parameters = dict(
        (key, getattr(definitions, key)) for key in dir(definitions))
    exec file(fname) in parameters, self.config
    self.interfaces = self.config["INTERFACES"]
    return True

  def OutputInterfaces(self):
    for interface in self.interfaces:
      print interface.OutputClientInterface()
      print interface.OutputServerInterface()

def FreakOut(message):
  print >>sys.stderr, message
  sys.exit(1)
  
def main(argv):
  if len(argv) < 2:
    FreakOut("Need name of interface file to process")

  generator = InterfaceGenerator()
  if not generator.ParseFile(argv[1]):
    return 1

  generator.OutputInterfaces()


if __name__ == "__main__":
  sys.exit(main(sys.argv))
