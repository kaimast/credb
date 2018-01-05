#! /usr/bin/python3

import credb
from test import *

c1 = credb.create_client("test1", "testserver1", "localhost")
c2 = credb.create_client("test2", "testserver2", "localhost", port=4242)

c1.put("foo", "bar")
res = c2.mirror("testserver1", "foo")

assert_true(res)
assert_equals(c1.get("foo"), "bar")
assert_equals(c2.get("foo"), "bar")

c1.put("foo", "xyz")

assert_equals(c2.get("foo"), "xyz")

c2.put("foo", "blabla")

assert_equals(c1.get("foo"), "blabla")
