#! /usr/bin/python3

from test import *
import credb

conn1 = create_test_client()
conn2 = create_test_client()

c1 = conn1.get_collection("multi")
c2 = conn2.get_collection("multi")

a = random_id()
res = c1.put(a, "FOO")

assert_true(res)
assert_equals(c1.get(a), c2.get(a))

b = random_id()
res = c2.put(b, "BAR")

assert_true(res)
assert_equals(c1.get(b), "BAR")
assert_equals(c1.get(b), c2.get(b))

c2.put(b, "CORNELL")

assert_equals(c1.get(b), c2.get(b))


