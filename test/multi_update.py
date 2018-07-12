#! /usr/bin/python3

import credb
import argparse
from random import randint
from test import *
from multiprocessing import Process

parser = argparse.ArgumentParser()
parser.add_argument("--num_clients", type=int, default=10)
parser.add_argument("--num_objs", type=int, default=100*1000)
parser.add_argument("--num_updates", type=int, default=1000)
parser.add_argument("--server", type=str, default="localhost")
parser.add_argument("--server_port", type=int, default=52424)
parser.add_argument("--no_server", action="store_true")

args = parser.parse_args()

objects=[]

for _ in range(args.num_objs):
    i = random_id()
    objects.append(i)

def run_updates(pos):
    conn = create_test_client(server=args.server, port=args.server_port, name="test"+str(pos))
    c = conn.get_collection('test')
    
    for _ in range(args.num_updates):
        i = randint(0, args.num_objs-1)
        c.put(objects[i], "foobar2")

def load_data():
    conn = create_test_client(server=args.server, port=args.server_port)
    for i in objects:
        c = conn.get_collection('test')
        c.put(i, "foobar")

server = Testserver()

if not args.no_server:
    server.start(args.server_port)

print("Setting up data")
p = Process(target=load_data)
p.start()
p.join()

print("Running updates")
processes=[]

for i in range(args.num_clients):
   p = Process(target=run_updates, args=[i])
   p.start()
   processes.append(p)

count=0
exitcode = 0
for p in processes:
   p.join()
   if p.exitcode:
       exitcode = p.exitcode
   count += 1
   print(str((count / args.num_clients) * 100) + "% done")

server.stop()
exit(exitcode)
