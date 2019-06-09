#! /usr/bin/python3

from test import *
from test_skeleton import *
import credb

conn, server = single_server_setup()

tx = conn.init_transaction(credb.IsolationLevel.Serializable)

tc = tx.get_collection('test')
tc.put("foo", 'bar')

success, witness = tx.commit(True)

# FIXMEassert_equals(witness.digest()["operations"][0]["type"], "PutObject")
assert_true(success)

tx = conn.init_transaction()

tc = tx.get_collection('test')
val = tc.get("foo")
assert_equals(val, "bar")

success, witness  = tx.commit(False)

assert_true(success)

c = conn.get_collection('test')
c.clear()
conn.close()

if server:
    server.stop()
