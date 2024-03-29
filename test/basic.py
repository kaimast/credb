#! /usr/bin/python3

from test import *
from test_skeleton import *

import credb
from datetime import datetime

conn, server = single_server_setup()

c = conn.get_collection("foobar")

a = random_id()
b = random_id()

doc1 = {"xyz":[{"a":3}, 42], "city": "Ithaca"}
res = c.put(a, doc1)

assert_true(res)
assert_equals(c.get(a), doc1)

doc2 = { "city": "NYC"}
doc3 = {"a":{"b":{"c":3}},"city": "Berkeley", "flag": 1}

res = c.put(b, doc2)
e3 = c.put(b, doc3)

assert_true(res)
assert_equals(c.get_history(b), [doc3, doc2])

res = c.find()
assert_equals(len(res), 2)
assert_equals_unordered(res, [(b, doc3), (a, doc1)])

found = c.find({"city":"Ithaca"}, ["city"])

assert_equals(len(found), 1)

k, val = found[0]
assert_equals(k, a)

k2, val2 = c.find_one({"city":"Ithaca"}, ["city"])

assert_equals(val2, val)

res = c.find({"city": {"$in": ["Berkeley", "Bamberg"]}})
assert_equals(len(res), 1)

res = c.find({"flag": {"$in": [3,2,1]}})
assert_equals(len(res), 1)

k, res = c.find_one({"flag":1,"city":"Berkeley"}, ["city"])
assert_equals(res, {"city":"Berkeley"})

# Find with index
c.create_index("cityindex",["city"])
k, res = c.find_one({"flag":1, "city":"Berkeley"}, ["city"])
assert_equals(res, {"city":"Berkeley"})

c.drop_index("cityindex")

res = c.remove(k)

try:
    res = c.find_one({"flag":1,"city":"Berkeley"})
    assert_true(false) # we should not get here
except RuntimeError:
    pass

k, res = c.find_one({"xyz.*.a" : 3})
assert_equals(res, doc1)

# Put a plain object
c.put(a, "foobar")
assert_equals(c.get(a), "foobar")
c.put(a, 4242)
assert_equals(c.get(a), 4242)

#Put a datetime object
now = datetime(1984,5,11, hour=12, minute=50, second=2, microsecond=0)

c.put(a, now)
assert_equals(c.get(a), now)

c.clear()
conn.close()

if server:
    server.stop()
