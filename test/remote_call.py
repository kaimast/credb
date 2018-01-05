#!/usr/bin/env python3
import math
import argparse
import random
import credb
import traceback
from test import *

parser = argparse.ArgumentParser()
parser.add_argument("--server_port1", type=int, default=52424)
parser.add_argument("--server_port2", type=int, default=52425)
parser.add_argument("--no_server", action="store_true")

args = parser.parse_args()

print("Setting up servers")

if args.no_server:
    server1 = None
    server2 = None
else:
    server1 = Testserver()
    server2 = Testserver()
    server1.start(args.server_port1, server_name="testserver1", listen=42000, sleep_time=0)
    server2.start(args.server_port2, server_name="testserver2", listen=42001, sleep_time=0)
    sleep(2.0)

func1 = """
import ledger
res = ledger.call_on_peer("testserver2", "func2", argv)
print("func1 done!")
return res
"""

func2 = """
val = int(argv[0])
res = (val % 2) == 0
print("func2 done!")
return res
"""

c1 = credb.create_client("c1", "testserver1", "localhost", port = args.server_port1)
c2 = credb.create_client("c2", "testserver2", "localhost", port = args.server_port2)

c1.put_code("func1", func1)

c1.peer("localhost:42001")

sleep(0.5)

c2.put_code("func2", func2)

print("Running calls")

assert_false(c1.call("func1", ["1"]))
assert_true(c1.call("func1", ["2"]))
assert_false(c1.call("func1", ["3"]))
assert_true(c1.call("func1", ["4"]))

print("Done")

if server1:
    server1.stop()

if server2:
    server2.stop()

