#!/usr/bin/env python3
import math
import argparse
import random
import credb
import traceback
from multiprocessing import Process
from test import *

parser = argparse.ArgumentParser()
parser.add_argument("--server_port1", type=int, default=52424)
parser.add_argument("--server_port2", type=int, default=52425)
parser.add_argument("--no_server", action="store_true")
parser.add_argument("--num_calls", type=int, default=1000)
parser.add_argument("--one_way", action="store_true")
parser.add_argument("--verbose", action="store_true")

args = parser.parse_args()

if args.no_server:
    server1 = None
    server2 = None
else:
    server1 = Testserver()
    server2 = Testserver()
    server1.start(args.server_port1, server_name="testserver1", listen=42000, sleep_time=0, quiet=(not args.verbose))
    server2.start(args.server_port2, server_name="testserver2", listen=42001, sleep_time=0, quiet=(not args.verbose))
    sleep(4.0)

func1 = """
import db
collection = argv[0]
other_server = argv[1]
val = argv[2]
res = db.call_on_peer(other_server, collection, "func2", [val])
print("func1 done! val=" + val + " res=" + str(res))
return res
"""

func2 = """
val = int(argv[0])
res = (val % 2) == 0
print("func2 done! val=" + str(val) + " res=" + str(res))
return res
"""


def run_calls(server_name, server_port, other_server):
    conn = create_test_client(server="localhost", server_name=server_name, port=server_port)
    c = conn.get_collection('test')

    for i in range(args.num_calls, 0, -1):
        res = c.call("func1", ['test', other_server, str(i)])
        assert_equals(res, i % 2 == 0)


def setup_servers():
    conn1 = credb.create_client("c1", "testserver1", "localhost", port=args.server_port1)
    conn2 = credb.create_client("c2", "testserver2", "localhost", port=args.server_port2)
    c1 = conn1.get_collection('test')
    c2 = conn2.get_collection('test')

    c1.put_code("func1", func1)
    c1.put_code("func2", func2)
    c2.put_code("func1", func1)
    c2.put_code("func2", func2)

    conn1.peer("localhost:42001")


print("Setting up servers")
p0 = Process(target=setup_servers)
p0.start()
p0.join()

sleep(0.5)

print("Running call operations")
processes = []
count = 0
p1 = Process(target=run_calls, args=["testserver1", args.server_port1, "testserver2"])
p1.start()
processes.append(p1)

if not args.one_way:
    p2 = Process(target=run_calls, args=["testserver2", args.server_port2, "testserver1"])

    p2.start()
    processes.append(p2)

num_clients = len(processes)

exitcode = 0
for p in processes:
    count += 1
    p.join()
    if p.exitcode:
        exitcode = p.exitcode
    print(str((count / num_clients) * 100) + "%")

if server1:
    server1.stop()

if server2:
    server2.stop()

exit(exitcode)
