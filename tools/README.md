# Host-side tools
Tools in this directory are compiled on the host that builds the system. They provide various utilities to work with the system's file formats, file systems, and so forth.

## ildc
Code generator for the RPC IDL. It takes in an IDL file that describes one or more RPC interfaces, and outputs some C++ code -- both the server and client stubs -- as well some structs and associated serialization code to encode the messages into the wire format. (This supports arbitrary user defined types by simply implementing the three methods in the `rpc` namespace for the user defined type.)
