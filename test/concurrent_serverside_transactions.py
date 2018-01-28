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
parser.add_argument("--server_port", type=int, default=5042)
parser.add_argument("--no_server", action="store_true")

args = parser.parse_args()

add_program = """
import db
success = False

while not success:
    tx = db.init_transaction()
    c = tx.get_collection("txtest")

    if not c.has_object('value'):
        # not good
        return False

    i = c.get('value')
    c.put('value', i+1)

    success, res = tx.commit(False)

return True
"""

def run_adds():
    conn = create_test_client(server=args.server, port=args.server_port)

    for i in range(args.num_ops):
        c = conn.get_collection(COLLECTION)
        c.call("program", [])

def load_data():
    conn = create_test_client(server=args.server, port=args.server_port)
    c = conn.get_collection(COLLECTION)
    res = c.put_code("program", add_program)

    if not res:
        raise RuntimeError("failed to put program")

    c.put("value", 0)

server = Testserver()

if not args.no_server:
    server.start(args.server_port)

print("Loading data...")
p = Process(target=load_data)
p.start()
p.join()

processes=[]
count = 0

print("Running transactions")
for _ in range(args.num_clients):
   p = Process(target=run_adds)
   p.start()
   processes.append(p)

exitcode = 0
for p in processes:
   p.join()
   if p.exitcode:
       exitcode = p.exitcode
   count += 1
   print(str((count / args.num_clients) * 100) + "%")

conn = create_test_client(server=args.server, port=args.server_port)
c = conn.get_collection(COLLECTION)

assert_equals(c.get("value"), args.num_clients * args.num_ops)

server.stop()
exit(exitcode)


