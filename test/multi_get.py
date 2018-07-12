#! /usr/bin/python3

import credb
from random import randint
import argparse
from test import *
from multiprocessing import Process
import time

parser = argparse.ArgumentParser()
parser.add_argument("--num_clients", type=int, default=100)
parser.add_argument("--num_objs", type=int, default=100*1000)
parser.add_argument("--num_gets", type=int, default=1000)
parser.add_argument("--server", type=str, default="localhost")
parser.add_argument("--server_port", type=int, default=5042)
parser.add_argument("--no_server", action="store_true")

args = parser.parse_args()

COLLECTION = "testcol"
PAYLOAD = "foobar"

objects=[]

def run_gets(pos):
    conn = create_test_client(server=args.server, port=args.server_port, name="test"+str(pos))
    c = conn.get_collection(COLLECTION)

    for _ in range(args.num_gets):
        i = randint(0, args.num_objs-1)
        obj = c.get(objects[i])
        assert(obj == PAYLOAD)

def load_data():
    conn = create_test_client(server=args.server, port=args.server_port)
    c = conn.get_collection(COLLECTION)
    for obj in objects:
        c.put(obj, PAYLOAD)

print("Generating identifiers")
for _ in range(args.num_objs):
    objects.append(random_id())

server = Testserver()

if not args.no_server:
    server.start(args.server_port)

print("Loading data...")
p = Process(target=load_data)
p.start()
p.join()

processes=[]
count = 0

print("Running get operations")
st = time.perf_counter()
for pos in range(args.num_clients):
   p = Process(target=run_gets, args=[pos])
   p.start()
   processes.append(p)

exitcode = 0
for p in processes:
   p.join()
   if p.exitcode:
       exitcode = p.exitcode
   count += 1
   print(str((count / args.num_clients) * 100) + "%")
ed = time.perf_counter()
print(ed-st)

server.stop()
print("Done")
exit(exitcode)
