#!/usr/bin/env python3

import argparse
import os
import sys
import time
import credb
import multiprocessing
import numpy as np
from tqdm import tqdm
from test import *

def uniform(length, ops, args):
    assert len(args) == 0
    return np.random.randint(length, size=ops)

def zipf(length, ops, args):
    assert len(args) == 1
    a = float(args[0])
    return np.random.zipf(a, size=ops)

def normal(length, ops, args):
    assert len(args) == 2
    mean = float(args[0])
    stddev = np.sqrt(float(args[0]))
    return mean + np.random.randn(ops) * stddev

def get_distribution(length, ops, args):
    args = args.split('-')
    distributions = [uniform, zipf, normal]
    d = {x.__name__.lower(): x for x in distributions}
    xs = d[args[0].lower()](length, ops, args[1:])
    offset = random.randrange(length)
    return (xs + offset) % length

def create_client(args):
    return credb.create_client('latency bench client', args.server_name, args.host, args.port)

def do_bench(args):
    c = create_client(args)
    obj = random_string(args.obj_size)
    read_distribution = iter(get_distribution(args.num_objs, args.num_ops, args.read_distribution))
    write_distribution = iter(get_distribution(args.num_objs, args.num_ops, args.write_distribution))
    res = np.empty(args.num_ops, dtype=np.float64)

    for i in tqdm(range(args.num_ops)):
        if random.random() < args.prob_write:
            key = 'latency' + str(next(write_distribution))
            st = time.perf_counter()
            c.put(key, obj)
            ed = time.perf_counter()
        else:
            key = 'latency' + str(next(read_distribution))
            st = time.perf_counter()
            c.get(key)
            ed = time.perf_counter()
        res[i] = (ed - st) * 1e6

    res.sort()
    print('mean:', np.mean(res), 'us')
    print('std :', np.std(res), 'us')
    print('50% :', res[int(args.num_ops * 0.5)], 'us')
    print('70% :', res[int(args.num_ops * 0.7)], 'us')
    print('90% :', res[int(args.num_ops * 0.9)], 'us')
    print('99% :', res[int(args.num_ops * 0.99)], 'us')
    print('99.9:', res[int(args.num_ops * 0.999)], 'us')
    print('9999:', res[int(args.num_ops * 0.9999)], 'us')
    np.save(args.output, res)
    print('written to', args.output)

def do_prepare_worker(args, start, count):
    c = create_client(args)
    obj = random_string(args.obj_size)
    for i in range(start, start + count):
        key = 'latency' + str(i)
        assert c.put(key, obj)

def do_prepare(args):
    ps = []
    num_workers = 16
    per_worker = int(np.ceil(args.num_objs / num_workers))
    for i in range(num_workers):
        p = multiprocessing.Process(target=do_prepare_worker, args=(args, per_worker*i, per_worker))
        p.start()
        ps.append(p)
    for p in ps:
        p.join()
    print('done')

if __name__ == '__main__':
    action_dict = {'bench': do_bench, 'prepare': do_prepare}
    parser = argparse.ArgumentParser()
    parser.add_argument('--server_name', type=str, default='latency-bench')
    parser.add_argument('--host', type=str, default='localhost')
    parser.add_argument('--port', type=int, default=5042)
    parser.add_argument('--num_objs', type=int, default=1000000)
    parser.add_argument('--num_ops', type=int, default=100000)
    parser.add_argument('--obj_size', type=int, default=1024)
    parser.add_argument('--prob_write', type=int, default=0)
    parser.add_argument('--read_distribution', type=str, default='zipf-1.5')
    parser.add_argument('--write_distribution', type=str, default='uniform')
    parser.add_argument('--output', type=str, default='latency.csv')
    parser.add_argument('action', choices=list(action_dict.keys()))
    args = parser.parse_args()

    action_dict[args.action](args)
