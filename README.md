# CreDB

[![Build Status](https://travis-ci.org/kaimast/credb.svg?branch=master)](https://travis-ci.org/kaimsat/credb)

A trustable peer-to-peer DB built on top of SGX.

## Tutorial

### Running a server
Server are started with a human-readable name. You can optionally specify a hostname to bind to. 

```
./credb <server_name>
```

### Peering Two Nodes
Run the first node as a listener.

```
./credb testserver1 --listen --port 4242
```

Then you can spin up a second node to connect to the first node. They will automatically initiate the handshake.

```
./credb testserver2 --connect localhost --port 4243
```

That's it! Note, that you have to specify two different client ports (using the --ports) flag, if you run on the same machine.

### Connecting to a server
First, you have to start a server. We will use the default port (no --port flag) in this example.

```
./credb testserver
```

### Running you first program
PeerDB comes with C++ and python bindings. The following python code should just work out of the box. For more examples take a look at the files in "test/".

```python
#! /usr/bin/python3
import credb

conn = credb.create_client('test', 'testserver', 'localhost')

c = conn.get_collection('test')

c.put("foo", "bar")
assert(c.get("foo") == "bar")

c.put("foo", "xyz")
assert(c.get_history() == ["xyz", "bar"]
```

### Json support
Both API bindings come with support for JSON documents. Get and put operations can read/mutate parts of documents as well.

```
c.put("foo", {"bar" : 42})
assert(c.get("foo.bar") == 42)
```

## Development
### Dependencies
* yael ( http://github.com/kaimast/yael )
* libdocument ( http://github.com/kaimast/libdocument )
* cowlang ( http://github.com/kaimast/cowlang )
* google-logging
* boost.python
* boost.program_options
* python 3
* GCC6
* ninja and meson
* linux SGX SDK

### Building
The following step build credb using meson. Only x64 linux supported so far.

```
meson build
cd build
ninja
```

### Installing
``ninja install`` will install both the C++ and Python client libraries.
