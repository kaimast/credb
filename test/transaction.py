#! /usr/bin/python3

from test import *
import credb

conn = create_test_client()
tx = conn.init_transaction(credb.IsolationLevel.Serializable)

tc = tx.get_collection('test')
tc.put("foo", 'bar')

res = tx.commit(True)
witness = res["witness"]

assert_equals(witness.digest()["operations"][0]["type"], "PutObject")
assert_true(res["success"])

tx = conn.init_transaction()

tc = tx.get_collection('test')
val = tc.get("foo")
assert_equals(val, "bar")

res = tx.commit(False)

assert_true(res["success"])

c = conn.get_collection('test')
c.clear()
