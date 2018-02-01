#! /usr/bin/python3

from test import *
import argparse
import multiprocessing
import credb

parser = argparse.ArgumentParser()
parser.add_argument("--server_port", type=int, default=5042)
parser.add_argument("--no_server", action="store_true")

args = parser.parse_args()

if args.no_server:
    server = None
else:
    server = Testserver()
    server.start(args.server_port)

def run_test():
    conn = create_test_client(port=args.server_port)
    c = conn.get_collection('test')

    func = """
val = int(argv[0])
return (val % 2) == 0"""

    c.put_code("x", func)

    assert_false(c.call("x", ["1"]))
    assert_true(c.call("x", ["2"]))
    assert_false(c.call("x", ["3"]))
    assert_true(c.call("x", ["4"]))

p = multiprocessing.Process(target=run_test)
p.start()
p.join()

if server:
    server.stop()
exit(p.exitcode)
