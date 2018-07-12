#! /usr/bin/python3

from test import *
import argparse
import credb

parser = argparse.ArgumentParser()
parser.add_argument("--server_port", type=int, default=52424)
parser.add_argument("--no_server", action="store_true")

args = parser.parse_args()

if args.no_server:
    server = None
else:
    server = Testserver()
    server.start(args.server_port)

conn = create_test_client(port=args.server_port)
c = conn.get_collection('test')

c.put("x", {"value": "foo"})

assert_equals(c.get("x.value"), "foo")

# only allow server to access the data
c.put_code("x.policy", 
"""from op_context import source_uri
return source_uri == 'server://testserver:test/program'""")

try:
    c.get("x.value")
    assert_true(False) # we shouldn't get here...
except RuntimeError:
    pass

c.put_code("program",
"""import db
c = db.get_collection('test')
return c.get('x.value')
""")

res = c.call("program", [])

assert_equals(res, "foo")
