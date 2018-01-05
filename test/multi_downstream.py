#! /usr/bin/python3

import credb
import random
import math
import argparse
from test import *
from multiprocessing import Process

parser = argparse.ArgumentParser()
parser.add_argument("--num_clients", type=int, default=10)
parser.add_argument("--num_objs", type=int, default=100*1000)
parser.add_argument("--num_ops", type=int, default=10000)
parser.add_argument("--prob_write", type=int, default=50)
parser.add_argument("--server", type=str, default="localhost")
parser.add_argument("--listen_port", type=int, default=5042)
parser.add_argument("--server_port", type=int, default=50333)
parser.add_argument("--no_upstream", action='store_true')

args = parser.parse_args()

def load_data(start_id, stop_id):
    conn = create_test_client(server=args.server, port=args.server_port)
    c = conn.get_collection('test')
    for i in range(start_id, stop_id):
        c.put('key'+str(i), random_string(1024))

def run_test(server_port):
    conn = create_test_client(server=args.server, port=server_port, server_name="testserver"+str(server_port))
    c = conn.get_collection('test')
    for _ in range(args.num_ops):
        key = 'key' + str(random.randrange(args.num_objs))
        if random.random() * 100 < args.prob_write:
            c.put(key, random_string(1024))
        else:
            c.get(key)

# start upstream
if not args.no_upstream:
    server = Testserver()
    server.start(args.server_port, args.listen_port)


# load data
processes = []
NUM_WORKER_LOAD_DATA = 10
NUM_DATA_PER_WORKER = int(math.ceil(args.num_objs / NUM_WORKER_LOAD_DATA))
for i in range(NUM_WORKER_LOAD_DATA):
    p = Process(target=load_data, args=(NUM_DATA_PER_WORKER * i, NUM_DATA_PER_WORKER * (i+1)))
    p.start()
    processes.append(p)
for p in processes:
    p.join()
del processes[:]
print('done loading data')


# start downstream
downstreams = []
for i in range(args.num_clients):
    p = Downstream()
    p.start(args.server_port+1+i, args.listen_port)
    downstreams.append(p)
sleep(3 * args.num_clients)

# run test
print('start to run test')
for i in range(args.num_clients):
    p = Process(target=run_test, args=(args.server_port+1+i,))
    p.start()
    processes.append(p)

# Wait for termination
exitcode = 0
for p in processes:
    p.join()
    if p.exitcode:
        exitcode = p.exitcode
for d in downstreams:
    d.stop()
if not args.no_upstream:
    server.stop()
print("done")
exit(exitcode)
