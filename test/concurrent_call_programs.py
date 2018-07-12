#!/usr/bin/env python3
import math
import argparse
import random
import credb
import traceback
from multiprocessing import Process
from test import *

parser = argparse.ArgumentParser()
parser.add_argument("--server_port", type=int, default=5042)
parser.add_argument("--no_server", action="store_true")
parser.add_argument("--num_calls", type=int, default=1000)
parser.add_argument("--num_clients", type=int, default=10)
parser.add_argument("--verbose", action="store_true")

args = parser.parse_args()

if args.no_server:
    server = None
else:
    server = Testserver()
    server.start(args.server_port, quiet=(not args.verbose))

func = """
val = int(argv[0])
return (val % 2) == 0"""


def run_calls(pos):
    conn = create_test_client(server="localhost", port=args.server_port, name="testclient" + str(pos))
    c = conn.get_collection('test')

    for i in range(args.num_calls):
        res = c.call("func", [str(i)])
        assert_equals(res, i % 2 == 0)


def setup_server():
    conn = credb.create_client("c", "testserver", "localhost", port=args.server_port)
    c = conn.get_collection('test')
    c.put_code("func", func)


def run_test():
    print("Setting up server")
    p0 = Process(target=setup_server)
    p0.start()
    p0.join()
    print("Running call operations")
    processes = []
    count = 0

    for i in range(args.num_clients):
        p = Process(target=run_calls, args=[i])
        p.start()
        processes.append(p)

    exitcode = 0
    for p in processes:
        count += 1
        p.join()
        if p.exitcode:
            exitcode = p.exitcode
    return exitcode


p = Process(target=run_test)
p.start()
p.join()

if server:
    server.stop()
exit(p.exitcode)
