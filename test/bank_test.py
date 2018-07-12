#!/usr/bin/env python3

import argparse
import sys
import credb

from time import time, sleep
from random import randint, sample
from test import Testserver
from subprocess import call
from multiprocessing import Process, Value

parser = argparse.ArgumentParser()
parser.add_argument("--num_banks", type=int, default=5)
parser.add_argument("--num_clients", type=int, default=5)
parser.add_argument("--no_server", action="store_true")
parser.add_argument("--num_ops", type=int, default=100)
parser.add_argument("--verbose", action="store_true")
parser.add_argument("--setup_only", action="store_true", help="Don't run the test just setup the banks")

args = parser.parse_args()

NUM_BANKS=args.num_banks
NUM_CLIENTS=args.num_clients
NUM_OPS=args.num_ops

RUN_SERVER = (args.no_server == False)

def get_balance_runner(balance):
    for i in range(NUM_BANKS):
        CID = i
        BID = i

        conn = credb.create_client("balance" + str(CID), "bank" + str(BID), "localhost", port=9000+BID)

        b = conn.get_collection("programs").call("balance", [])
        balance.value += b

        conn.close()

    print("balance is: " + str(balance.value))
    exit(balance)

def get_balance():
    # We need to run this in a seperate process because of event loop reasonsa
    balance = Value('i', 0)

    p = Process(target=get_balance_runner, args=(balance,))
    p.start()
    p.join()

    return balance.value

servers = []
if RUN_SERVER:
    print("Starting servers")

    for i in range(NUM_BANKS):
        server = Testserver()
        server.start(9000 + i, server_name="bank"+str(i), listen=10000+i, sleep_time=1, quiet=(not args.verbose))
        servers.append(server)

    sleep(10)

print("Setting up servers")

for i in range(NUM_BANKS):
    call(["../test/bank-test/setup-banks1", str(i), str(NUM_BANKS), str(NUM_CLIENTS)])

for i in range(NUM_BANKS):
    call(["../test/bank-test/connect-banks", str(i), str(NUM_BANKS), str(NUM_CLIENTS)])

for i in range(NUM_BANKS):
    call(["../test/bank-test/setup-banks2", str(i), str(NUM_BANKS), str(NUM_CLIENTS)])

if args.setup_only:
    exit(0)

sleep(1.0)

start_balance = get_balance()

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

    conn.close()
    exit(0)

clients=[]
count = 0

for i in range(NUM_CLIENTS):
   p = Process(target=run_bank_client, args=[i])
   p.start()
   clients.append(p)

print("waiting for clients to terminate")

exitcode = 0
for p in clients:
   count += 1
   p.join()
   if p.exitcode != 0:
       exitcode = p.exitcode
   print(str((count / NUM_CLIENTS) * 100) + "%")

end_balance = get_balance()

if start_balance == end_balance:
    print("Success. Cleaning up...")
else:
    exitcode = -1
    print(str(start_balance) + " != " + str(end_balance))
    print("Failure. Cleaning up...")

if RUN_SERVER:
    for server in servers:
        server.stop()

exit(exitcode)
