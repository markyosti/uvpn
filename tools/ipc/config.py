#!/usr/bin/python

INTERFACES = [
  Interface(
    name="DaemonController",
    sends=[
      Rpc(name="ClientConnect",
        sends=[StringParameter("server")],
        receives=[]),
      Rpc(name="ServerShowClients",
        sends=[],
        receives=[Repeated(StringParameter("client"))]),
    ],
    receives=[
      Rpc(name="GetParameterFromUser",
        sends=[Repeated(StringParameter("name"))],
        receives=[Repeated(StringParameter("value"))]),
    ]
  )
]
