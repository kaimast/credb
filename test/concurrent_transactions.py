#! /usr/bin/python3

import credb
from random import randint
import argparse
from test import *
from multiprocessing import Process
import time

COLLECTION="txtest"

parser = argparse.ArgumentParser()
parser.add_argument("--num_clients", type=int, default=50)
parser.add_argument("--num_ops", type=int, default=100)
parser.add_argument("--server", type=str, default="localhost")
parser.add_argument("--server_port", type=int, default=52424)
parser.add_argument("--no_server", action="store_true")
parser.add_argument("--verbose", action="store_true")
parser.add_argument("--unsafe_mode", action="store_true")

args = parser.parse_args()

def run_adds(pos):
    conn = create_test_client(server=args.server, port=args.server_port, name="testclient" + str(pos), unsafe_mode=args.unsafe_mode)

    for _ in range(args.num_ops):
        success = False

        while not success:
            tx = conn.init_transaction()
            c = tx.get_collection(COLLECTION)

            val = c.get("foo")
            c.put("foo.count", val["count"] +1)

            success, res = tx.commit(False)

            if not success:
                print('Retry, because "' + str(res) + '"')

    conn.close()

def load_data():
    conn = create_test_client(server=args.server, port=args.server_port, unsafe_mode=args.unsafe_mode)

    c = conn.get_collection(COLLECTION)
    c.put("foo", {"count": 0})
    conn.close()

server = Testserver()

if not args.no_server:
    server.start(args.server_port, quiet=(not args.verbose), unsafe_mode=args.unsafe_mode)

print("Loading data...")
p = Process(target=load_data)
p.start()
p.join()

processes=[]
count = 0

print("Running transactions")
for i in range(args.num_clients):
   p = Process(target=run_adds, args=[i])
   p.start()
   processes.append(p)

exitcode = 0
for p in processes:
   p.join()
   if p.exitcode:
       exitcode = p.exitcode
   count += 1
   print(str((count / args.num_clients) * 100) + "%")

conn = create_test_client(server=args.server, port=args.server_port, unsafe_mode=args.unsafe_mode)
c = conn.get_collection(COLLECTION)

assert_equals(c.get("foo.count"), args.num_clients * args.num_ops)

conn.close()
sleep(1)

server.stop()
exit(exitcode)


