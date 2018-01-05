#!/usr/bin/env python3

# to verify the debug snapshot is working:
#    ./credb testserver
#    ../test/debug_snapshot.py put
#    ../test/debug_snapshot.py dump
#    killall credb
#    ../test/debug_snapshot.py load
#    ../test/debug_snapshot.py get
#
# to test with hash index:
#    ./credb testserver
#    ../test/debug_snapshot.py put --create_index
#    killall credb
#    ../test/debug_snapshot.py load
#    ../test/debug_snapshot.py find

import argparse
import math
import multiprocessing
import time
import random
import credb
from test import *

def gen_value(i, size):
    return {"i": i, "content": random_string(args.obj_size)}

def put_data(args, start_id, stop_id):
    random.seed(args.seed + start_id)
    c = credb.create_client('client', args.name, args.host, args.port)
    for i in range(start_id, stop_id):
        assert c.put('key'+str(i), gen_value(i, args.obj_size))

def check_data(args, start_id, stop_id):
    random.seed(args.seed + start_id)
    c = credb.create_client('client', args.name, args.host, args.port)
    for i in range(start_id, stop_id):
        assert c.get('key'+str(i)) == gen_value(i, args.obj_size)

def find_data(args, start_id, stop_id):
    random.seed(args.seed + start_id)
    c = credb.create_client('client', args.name, args.host, args.port)
    accum = 0
    for i in range(start_id, stop_id):
        st = time.perf_counter()
        key, eid, doc = c.find_one('key', {'i': i})
        ed = time.perf_counter()
        assert key == 'key' + str(i)
        assert doc == gen_value(i, args.obj_size)
        accum += ed - st
    print('avg find time: {}us'.format(accum / (stop_id - start_id) * 1e6))

def run_multiclient(args, target):
    processes = []
    NUM_WORKER_LOAD_DATA = 10
    NUM_DATA_PER_WORKER = int(math.ceil(args.num_objs / NUM_WORKER_LOAD_DATA))
    for i in range(NUM_WORKER_LOAD_DATA):
        p = multiprocessing.Process(target=target, args=(args, NUM_DATA_PER_WORKER * i, NUM_DATA_PER_WORKER * (i+1)))
        p.start()
        processes.append(p)
    for p in processes:
        p.join()

def put(args):
    if args.create_index:
        def create_index(args):
            c = credb.create_client('client', args.name, args.host, args.port)
            c.create_index('debug_snapshot_on_i', 'key', ['i'])
        p = multiprocessing.Process(target=create_index, args=(args,))
        p.start()
        p.join()
    run_multiclient(args, put_data)

def get(args):
    run_multiclient(args, check_data)

def find(args):
    run_multiclient(args, find_data)

def dump(args):
    c = credb.create_client('client', args.name, args.host, args.port)
    assert c.dump_everything(args.filename)

def load(args):
    c = credb.create_client('client', args.name, args.host, args.port)
    assert c.load_everything(args.filename)

if __name__ == '__main__':
    action_dict = {'get': get, 'put': put, 'dump': dump, 'load': load, 'find': find}
    parser = argparse.ArgumentParser()
    parser.add_argument('--name', type=str, default='testserver')
    parser.add_argument('--host', type=str, default='localhost')
    parser.add_argument('--port', type=int, default=5042)
    parser.add_argument('--num_objs', type=int, default=100000)
    parser.add_argument('--obj_size', type=int, default=1000)
    parser.add_argument('--seed', type=int, default=20170926)
    parser.add_argument('--filename', type=str, default='dump.credb')
    parser.add_argument('--create_index', action='store_true')
    parser.add_argument('action', choices=action_dict.keys())

    args = parser.parse_args()
    action_dict[args.action](args)
