#! /usr/bin/python3

import credb
import argparse
from test import *
from multiprocessing import Process

parser = argparse.ArgumentParser()
parser.add_argument("--num_clients", type=int, default=100)
parser.add_argument("--num_puts", type=int, default=100*1000)
parser.add_argument("--server", type=str, default="localhost")
parser.add_argument("--server_port", type=int, default=52424)
parser.add_argument("--no_server", action="store_true")
parser.add_argument("--verbose", action="store_true")

COLLECTION = "testcol"
PAYLOAD = "foobar"

args = parser.parse_args()

def run_puts():
    conn = create_test_client(server=args.server, port=args.server_port)
    c = conn.get_collection(COLLECTION)

    for _ in range(args.num_puts):
        c.put(random_id(), PAYLOAD)

processes=[]
count = 0

server = Testserver()

if not args.no_server:
    server.start(args.server_port, quiet=(not args.verbose))

for _ in range(args.num_clients):
   p = Process(target=run_puts)
   p.start()
   processes.append(p)

exitcode = 0
for p in processes:
   p.join()
   if p.exitcode:
       exitcode = p.exitcode
   count += 1
   print(str((count / args.num_clients) * 100) + "% done")

server.stop()
exit(exitcode)
