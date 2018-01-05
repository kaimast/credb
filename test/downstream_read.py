#!/usr/bin/env python3
import math
import argparse
import random
import credb
import traceback
from test import *

def gen_data(length):
    return {random_string(32): random_string(4096) for _ in range(length)}

def put_upstream(args):
    """usage::

        ../build/credb upstream-test --listen
        ./downstream_read.py put --num_ops 10000
    """
    upstream_conn = credb.create_client("pythontestclient", args.upstream_name, args.upstream_host, args.upstream_port)
    c = upstream_conn.get_collection('test')
    d = gen_data(args.num_ops)
    for k, v in d.items():
        c.put(k, v)

def get_downstream(args):
    """usage::

        ../build/credb downstream-test --port 10086 --upstream localhost
        ./downstream_read.py get --num_ops 10000
    """
    downstream_conn = credb.create_client("pythontestclient", args.downstream_name, args.downstream_host, args.downstream_port)
    c = downstream_conn.get_collection('test')
    d = gen_data(args.num_ops)
    notfound = set(d.keys())
    for k, v in d.items():
        value = c.get(k)
        if value != v:
            raise 'key: {}, expected: {}, got: {}'.format(k, v, value)
        notfound.remove(k)
    if notfound:
        raise 'key {} not found'.format(next(iter(notfound)))
    print('success')

def both(args):
    """usage::

        ../build/credb upstream-test --listen
        ../build/credb downstream-test --port 10086 --upstream localhost
        ./downstream_read.py both --num_ops 10000
    """
    upstream_conn = credb.create_client("pythontestclient", args.upstream_name, args.upstream_host, args.upstream_port)
    downstream_conn = credb.create_client("pythontestclient", args.downstream_name, args.downstream_host, args.downstream_port)
    d = {}

    upstream_c = upstream_conn.get_collection('test')
    downstream_c =upstream_conn.get_collection('test')

    for _ in range(args.num_ops):
        if random.random() < 0.5:
            key = random_string(6)
            value = random_string(int(math.exp(random.random() * 10)))
            upstream_c.put(key, value)
            d[key] = value
        else:
            key = random.choice(list(d.keys()))
            expected = d[key]
            got = downstream_c.get(key)
            if expected != got:
                raise 'key: {}, expected: {}, got: {}'.format(key, expected, got)
    print('success')

def downstream_put(args):
    """usage::

        ../build/credb upstream-test --listen
        ../build/credb downstream-test --port 10086 --upstream localhost
        ./downstream_read.py downstream_put --num_ops 10000
    """
    upstream_client = credb.create_client("pythontestclient", args.upstream_name, args.upstream_host, args.upstream_port)
    downstream_client = credb.create_client("pythontestclient", args.downstream_name, args.downstream_host, args.downstream_port)
    def random_client():
        return upstream_client if random.random() < 0.5 else downstream_client

    d = {}
    for _ in range(args.num_ops):
        if random.random() < 0.5:
            key = random_string(6)
            value = random_string(int(math.exp(random.random() * 10)))
            random_client().put(key, value)
            d[key] = value
        else:
            key = random.choice(list(d.keys()))
            expected = d[key]
            got = random_client().get(key)
            if expected != got:
                raise 'key: {}, expected: {}, got: {}'.format(key, expected, got)
    print('success')


if __name__ == '__main__':
    action_dict = {'put': put_upstream, 'get': get_downstream, 'both': both, 'downstream_put': downstream_put}
    parser = argparse.ArgumentParser()
    parser.add_argument("--seed", type=int, default=20170714)
    parser.add_argument("--num_ops", type=int, required=True)
    parser.add_argument("--upstream_name", type=str, default='upstream-test')
    parser.add_argument("--upstream_host", type=str, default='localhost')
    parser.add_argument("--upstream_port", type=int, default=5042)
    parser.add_argument("--downstream_name", type=str, default='downstream-test')
    parser.add_argument("--downstream_host", type=str, default='localhost')
    parser.add_argument("--downstream_port", type=int, default=10086)
    parser.add_argument('action', choices=action_dict.keys())
    args = parser.parse_args()

    random.seed(args.seed)
    action_dict[args.action](args)

