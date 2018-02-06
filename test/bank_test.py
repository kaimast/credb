#!/usr/bin/env python3

import argparse
import sys
import credb

from time import time, sleep
from random import randint, sample
from test import Testserver
from subprocess import call
from multiprocessing import Process

parser = argparse.ArgumentParser()
parser.add_argument("--num_banks", type=int, default=2)
parser.add_argument("--num_clients", type=int, default=5)
parser.add_argument("--no_server", action="store_true")
parser.add_argument("--num_ops", type=int, default=100)
parser.add_argument("--verbose", action="store_true")

args = parser.parse_args()

NUM_BANKS=args.num_banks
NUM_CLIENTS=args.num_clients
NUM_OPS=args.num_ops

RUN_SERVER = (args.no_server == False)

servers = []

if RUN_SERVER:
    print("Starting servers")

    for i in range(NUM_BANKS):
        server = Testserver()
        server.start(9000 + i, server_name="bank"+str(i), listen=10000+i, sleep_time=0, quiet=(not args.verbose))
        servers.append(server)

    sleep(3.0)

print("Setting up servers")

for i in range(NUM_BANKS):
    call(["../test/bank-test/setup-banks1", str(i), str(NUM_BANKS), str(NUM_CLIENTS)])

for i in range(NUM_BANKS):
    call(["../test/bank-test/connect-banks", str(i), str(NUM_BANKS), str(NUM_CLIENTS)])

for i in range(NUM_BANKS):
    call(["../test/bank-test/setup-banks2", str(i), str(NUM_BANKS), str(NUM_CLIENTS)])

sleep(1.0)

print("Running clients")

def run_bank_client(CID):
    BID = CID % NUM_BANKS
    
    conn = credb.create_client("client" + str(CID), "bank" + str(BID), "localhost", port=9000+BID)
    programs = conn.get_collection('programs')

    other_clients = []

    for i in range(NUM_CLIENTS):
        if i != CID:
            other_clients.append(i)

    for i in range(NUM_OPS):
        other_client = sample(other_clients, 1)[0] #randint(0, NUM_CLIENTS-1)
        other_bank = other_client % NUM_BANKS

        if other_bank == BID:
            res = programs.call("move_money_locally", ["client" + str(CID), "client" + str(other_client), str(1)])

            if not res:
                raise RuntimeError("Failed to move money locally")
        else:
            res = programs.call("move_money_remotely", ["client" + str(CID), "client" + str(other_client), "bank" + str(other_bank), str(1)])

            if not res:
                raise RuntimeError("Failed to move money remotely")

clients=[]
count = 0

for i in range(NUM_CLIENTS):
   p = Process(target=run_bank_client, args=[i])
   p.start()
   clients.append(p)

exitcode = 0
for p in clients:
   count += 1
   p.join()
   if p.exitcode:
       exitcode = p.exitcode
   print(str((count / NUM_CLIENTS) * 100) + "%")

if RUN_SERVER:
    for server in servers:
        server.stop()

exit(exitcode)
